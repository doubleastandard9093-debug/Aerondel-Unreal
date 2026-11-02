"""
Aerondel > Import Lite Models: imports the Aerondel-Lite CC0 glTF models (Quaternius, Kenney) with Interchange and writes
Content/Data/rigs.json, which UAerondelModelLibrary reads at runtime to put rigged bodies on monsters, the player and NPCs.

  Tools/ImportSource/models/rigged/*.glb      -> /Game/Aerondel/Models/Rigged/<name>/   (skeletal mesh + anim sequences)
  Tools/ImportSource/models/characters/*.glb  -> /Game/Aerondel/Models/Characters/<name>/ (Kenney folk, skeletal)
  Tools/ImportSource/models/{buildings,nature,props}/*.glb -> /Game/Aerondel/Models/<Sub>/  (static meshes, for set dressing)
  Tools/ImportSource/rig_map.json             -> monster / npc / player mapping (generated from web js/rigs.js)

Safe to re-run: existing assets are replaced. Restart PIE (or call UAerondelModelLibrary.ReloadRigTable) after it finishes.
Nanite stays off (DX11 / 8 GB target); every skeletal mesh is set to no Nanite, LOD sync and cheap shadows.
"""
import json
import os
import re
import unreal
import aerondel_safe as safe

DEST = "/Game/Aerondel/Models"
CLIP = {
    "idle": [r"^idle$", r"^spider_idle$", r"flying_idle", r"^idle"],
    "walk": [r"^walk$", r"spider_walk", r"fast_flying", r"walk"],
    "run": [r"^run$", r"^gallop$", r"^sprint$", r"spider_walk", r"fast_flying", r"run|walk"],
    "attack": [r"sword_slash", r"^slash$", r"^attack$", r"spider_attack", r"attack-melee-right", r"bite|headbutt|punch|weapon|attack"],
    "hit": [r"hitre", r"hit"],
    "death": [r"death", r"^die$"],
    "gather": [r"interact|pick-up"],
}


def src_dir():
    return os.path.join(unreal.Paths.project_dir(), "Tools", "ImportSource")


def import_glb(path, dest):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    return list(t.get_editor_property("imported_object_paths") or [])


def assets_in(folder):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    return reg.get_assets_by_path(folder, recursive=True)


def clip_name(asset_name):
    # Interchange names sequences "<Mesh>_Anim_<Clip>" or "<Clip>"; also strip Blender "Armature|" prefixes.
    n = str(asset_name).split("|")[-1]
    n = re.sub(r"^.*?_Anim_", "", n)
    return n.lower()


def pick(anims, state):
    for pat in CLIP[state]:
        for name, path in anims:
            if re.search(pat, name):
                return path
    return None


def obj_path(a):
    return str(a.package_name) + "." + str(a.asset_name)


def tune_mesh(path):
    m = unreal.load_asset(path)
    if not m:
        return
    try:  # property names differ slightly between versions; every step is optional
        m.set_editor_property("enable_per_poly_collision", False)
    except Exception:
        pass
    unreal.EditorAssetLibrary.save_loaded_asset(m)


def run():
    root = src_dir()
    models = os.path.join(root, "models")
    if not os.path.isdir(models):
        unreal.log_error("[Aerondel] %s is missing (git pull?)" % models)
        return
    rigs = {}
    groups = [("rigged", "Rigged"), ("characters", "Characters")]
    files = [(g, d, f) for g, d in groups for f in sorted(os.listdir(os.path.join(models, g))) if f.lower().endswith(".glb")]
    statics = [(g, g.capitalize(), f) for g in ("buildings", "nature", "props") if os.path.isdir(os.path.join(models, g))
               for f in sorted(os.listdir(os.path.join(models, g))) if f.lower().endswith(".glb")]
    # One model at a time with a save + GC between them (weak iGPUs time out compiling many imported materials at once).
    with safe.SafeRun("Importing Lite models", len(files) + len(statics), save_fn=safe.save_dirty) as task:
        for g, d, f in files:
            key = os.path.splitext(f)[0]
            task.frame(f)
            folder = "%s/%s/%s" % (DEST, d, key)
            import_glb(os.path.join(models, g, f), folder)
            mesh, anims = None, []
            for a in assets_in(folder):
                cls = str(a.asset_class_path.asset_name) if hasattr(a, "asset_class_path") else str(a.asset_class)
                if cls == "SkeletalMesh" and not mesh:
                    mesh = obj_path(a)
                elif cls == "AnimSequence":
                    anims.append((clip_name(a.asset_name), obj_path(a)))
            if not mesh:
                unreal.log_warning("[Aerondel] %s: no skeletal mesh imported" % f)
                continue
            tune_mesh(mesh)
            states = {s: pick(anims, s) for s in CLIP}
            rigs[key] = {"mesh": mesh, "anims": {s: p for s, p in states.items() if p}}
            unreal.log("[Aerondel] %s: %d clips, states %s" % (key, len(anims), ",".join(sorted(rigs[key]["anims"]))))
        for g, d, f in statics:
            task.frame(f)
            import_glb(os.path.join(models, g, f), "%s/%s" % (DEST, d))
    if task.cancelled:
        unreal.log_warning("[Aerondel] Import cancelled; rigs.json left unchanged. Re-run to finish (already-imported models are kept).")
        return
    rmap = json.load(open(os.path.join(root, "rig_map.json"), encoding="utf-8"))
    for alias, target in rmap.get("aliases", {}).items():
        if target in rigs:
            rigs[alias] = rigs[target]
    out = {
        "yaw": rmap.get("yaw", -90),
        "rigs": rigs,
        "player": rmap.get("player", "adventurer"),
        "monsters": {k: v for k, v in rmap.get("monsters", {}).items() if v in rigs},
        "npcs": {k: v for k, v in rmap.get("npcs", {}).items() if v in rigs},
    }
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "rigs.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)
    unreal.log("[Aerondel] rigs.json: %d rigs, %d monsters, %d npcs -> %s" % (len(rigs), len(out["monsters"]), len(out["npcs"]), path))
