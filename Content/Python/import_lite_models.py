"""
Aerondel > Import Lite Models: imports the Aerondel-Lite CC0 glTF models (Quaternius, Kenney) with Interchange and writes
Content/Data/rigs.json, which UAerondelModelLibrary reads at runtime to put rigged bodies on monsters, the player and NPCs.

  Tools/ImportSource/models_ue/rigged/*.glb      -> /Game/Aerondel/Models/Rigged/<name>/   (skeletal mesh + anim sequences)
  Tools/ImportSource/models_ue/characters/*.glb  -> /Game/Aerondel/Models/Characters/<name>/ (Kenney folk, skeletal)
  Tools/ImportSource/models_ue/{buildings,nature,props}/*.glb -> /Game/Aerondel/Models/<Sub>/  (static meshes, for set dressing)
  Tools/ImportSource/rig_map.json             -> monster / npc / player mapping (generated from web js/rigs.js)

Resumable: models whose assets already exist (or that are listed in Saved/Aerondel/lite_import_done.json) are skipped, and
rigs.json is rewritten after every model, so after a crash just run it again and it carries on where it stopped.
"Import Lite Models (10 per run)" imports at most 10 new models per run for very weak laptops. To force a fresh
import of one model, delete its folder under /Game/Aerondel/Models and its line in lite_import_done.json. Restart PIE (or call UAerondelModelLibrary.ReloadRigTable) after it finishes.
Nanite stays off (DX11 / 8 GB target); every skeletal mesh is set to no Nanite, LOD sync and cheap shadows.
"""
import json
import os
import re
import time
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


def _set(obj, **kv):
    """Set whichever of these properties exist (names shift a little between 5.x versions)."""
    for k, v in kv.items():
        try:
            obj.set_editor_property(k, v)
        except Exception:
            pass


def _pipeline(skeletal):
    """Interchange generic pipeline: skeletal mesh + all animations into one per-rig skeleton (each rig imports into its
    own folder, so the skeleton is shared by that rig's mesh and clips), smooth recomputed normals, no Nanite, no
    per-asset materials explosion (one material per glTF material)."""
    p = unreal.InterchangeGenericAssetsPipeline()
    mesh = p.get_editor_property("mesh_pipeline")
    _set(mesh, import_static_meshes=not skeletal, import_skeletal_meshes=skeletal, combine_skeletal_meshes=True,
         create_physics_asset=False, import_morph_targets=False, build_nanite=False, recompute_normals=True,
         recompute_tangents=True, use_mikk_t_space=True, compute_weighted_normals=True, generate_lightmap_u_vs=not skeletal)
    common = p.get_editor_property("common_meshes_properties")
    _set(common, recompute_normals=True, recompute_tangents=True, use_mikk_t_space=True, compute_weighted_normals=True,
         keep_sections_separate=False)
    common_sk = p.get_editor_property("common_skeletal_meshes_and_animations_properties")
    _set(common_sk, import_only_animations=False, convert_static_mesh_to_skeletal_mesh=False)
    anim = p.get_editor_property("animation_pipeline")
    _set(anim, import_animations=skeletal, import_bone_tracks=True, import_custom_attribute=False)
    return p


def import_glb(path, dest, skeletal=False):
    """Interchange import with our pipeline; falls back to a plain AssetImportTask if the scripted manager is missing."""
    try:
        mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
        src = unreal.InterchangeManager.create_source_data(path)
        params = unreal.ImportAssetParameters()
        params.is_automated = True
        params.replace_existing = True
        params.override_pipelines.append(unreal.SoftObjectPath(_pipeline(skeletal).get_path_name()))
        mgr.import_asset(dest, src, params)
        return []
    except Exception as e:
        unreal.log_warning("[Aerondel] Interchange scripted import unavailable (%s); using AssetImportTask" % e)
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


# Extra import-only throttles for 128 MB iGPUs (2 s TDR): no parallel compiles, small texture pool, no realtime thumbnails.
IMPORT_CVARS = {
    "Editor.AsyncSkinnedAssetCompilationMaxConcurrency": 1, "Editor.AsyncStaticMeshCompilationMaxConcurrency": 1,
    "Editor.AsyncTextureCompilationMaxConcurrency": 1, "Editor.AsyncAnimationCompilationMaxConcurrency": 1,
    "r.Streaming.PoolSize": 64, "r.Streaming.FramesForFullUpdate": 30, "r.ShaderCompiler.MaxShaderJobs": 2,
    "Interchange.FeatureFlags.Import.ThumbnailGeneration": 0,
}
MAX_TEX = 512           # imported textures are capped to this (Max In-Game size) right after import
PAUSE_S = 1.0           # pause between models so the driver drains its queue
BATCH_SMALL = 10        # "small batch" menu entry: new models per run


def _cvars(values):
    old = {}
    for k, v in values.items():
        old[k] = safe._get(k)
        safe._cmd("%s %s" % (k, v))
    return old


def _no_thumbnails():
    """Best effort: turn off thumbnail rendering on save (setting names vary between engine versions)."""
    for cls_name in ("EditorLoadingSavingSettings", "ContentBrowserSettings", "EditorPerProjectUserSettings"):
        cls = getattr(unreal, cls_name, None)
        if cls is None:
            continue
        try:
            cdo = unreal.get_default_object(cls)
        except Exception:
            continue
        _set(cdo, b_disable_thumbnail_rendering=True, disable_thumbnail_rendering=True, real_time_thumbnails=False,
             b_enable_realtime_material_instance_thumbnails=False, enable_realtime_material_instance_thumbnails=False)


def _flush():
    """Save, GC, flush the render thread and pause: one model's GPU work is finished before the next starts."""
    try:
        safe.save_dirty()
    except Exception as e:
        unreal.log_warning("[Aerondel] save failed: %s" % e)
    for _ in range(2):
        try:
            unreal.SystemLibrary.collect_garbage()
        except Exception:
            pass
    for fn in ("flush_rendering_commands",):
        for owner in (getattr(unreal, "RenderingLibrary", None), getattr(unreal, "EditorLevelLibrary", None), unreal.SystemLibrary):
            f = getattr(owner, fn, None) if owner else None
            if f:
                try:
                    f()
                    break
                except Exception:
                    pass
    safe._cmd("FlushRenderingCommands")
    time.sleep(PAUSE_S)


def _cap_textures(folder, recursive=True, names=None):
    for a in assets_in(folder) if recursive else unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False):
        if _cls(a) != "Texture2D" or (names is not None and str(a.asset_name) not in names):
            continue
        t = unreal.load_asset(obj_path(a))
        if not t:
            continue
        _set(t, max_texture_size=MAX_TEX, never_stream=False)
        grp = getattr(getattr(unreal, "TextureGroup", None), "TEXTUREGROUP_CHARACTER", None)
        if grp is not None:
            _set(t, lod_group=grp)
        unreal.EditorAssetLibrary.save_loaded_asset(t, False)


def _cls(a):
    return str(a.asset_class_path.asset_name) if hasattr(a, "asset_class_path") else str(a.asset_class)


def _done_path():
    return os.path.join(unreal.Paths.project_saved_dir(), "Aerondel", "lite_import_done.json")


def _load_done():
    try:
        return set(json.load(open(_done_path(), encoding="utf-8")))
    except Exception:
        return set()


def _save_done(done):
    os.makedirs(os.path.dirname(_done_path()), exist_ok=True)
    with open(_done_path(), "w", encoding="utf-8") as fh:
        json.dump(sorted(done), fh, indent=1)


def _scan_rig(folder):
    mesh, anims = None, []
    for a in assets_in(folder):
        c = _cls(a)
        if c == "SkeletalMesh" and not mesh:
            mesh = obj_path(a)
        elif c == "AnimSequence":
            anims.append((clip_name(a.asset_name), obj_path(a)))
    return mesh, anims


def _static_names(folder):
    return {str(a.asset_name).lower() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False)}


def _write_rigs(root, rigs):
    rmap = json.load(open(os.path.join(root, "rig_map.json"), encoding="utf-8"))
    rigs = dict(rigs)
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
    return out, path


def run(batch=None):
    """batch=None imports everything still missing; batch=N stops after N newly imported models (re-run for more)."""
    root = src_dir()
    # models_ue = Unreal-safe copies (plain glTF: no meshopt/quantization, PNG not WebP, validator-clean); made from
    # models/ (the web's compressed copies) by Tools/make_ue_models.mjs. Interchange cannot read the compressed ones.
    models = os.path.join(root, "models_ue")
    if not os.path.isdir(models):
        unreal.log_error("[Aerondel] %s is missing (git pull?)" % models)
        return
    rigs = {}
    groups = [("rigged", "Rigged"), ("characters", "Characters")]
    files = [(g, d, f) for g, d in groups for f in sorted(os.listdir(os.path.join(models, g))) if f.lower().endswith(".glb")]
    statics = [(g, g.capitalize(), f) for g in ("buildings", "nature", "props") if os.path.isdir(os.path.join(models, g))
               for f in sorted(os.listdir(os.path.join(models, g))) if f.lower().endswith(".glb")]
    done = _load_done()
    imported, skipped, stopped = 0, 0, False
    _no_thumbnails()
    old = _cvars(IMPORT_CVARS)
    try:
        # One model at a time: import -> cap textures -> save -> GC -> flush render thread -> pause.
        with safe.SafeRun("Importing Lite models" + (" (batch of %d)" % batch if batch else ""), len(files) + len(statics)) as task:
            for g, d, f in files:
                key = os.path.splitext(f)[0]
                folder = "%s/%s/%s" % (DEST, d, key)
                mesh, anims = _scan_rig(folder)
                if mesh:
                    skipped += 1  # already imported (by an earlier, maybe crashed, run): keep it, just map it
                    task.frame("%s (already imported)" % f)
                else:
                    if batch and imported >= batch:
                        stopped = True
                        break
                    task.frame(f)
                    import_glb(os.path.join(models, g, f), folder, skeletal=True)
                    _flush()
                    _cap_textures(folder)
                    mesh, anims = _scan_rig(folder)
                    if not mesh:
                        unreal.log_warning("[Aerondel] %s: no skeletal mesh imported" % f)
                        _flush()
                        continue
                    tune_mesh(mesh)
                    imported += 1
                    _flush()
                done.add("%s/%s" % (g, f))
                _save_done(done)
                states = {s: pick(anims, s) for s in CLIP}
                rigs[key] = {"mesh": mesh, "anims": {s: p for s, p in states.items() if p}}
                _write_rigs(root, rigs)  # partial rigs.json after every model, so a crash loses nothing
                unreal.log("[Aerondel] %s: %d clips, states %s" % (key, len(anims), ",".join(sorted(rigs[key]["anims"]))))
            if not stopped:
                for g, d, f in statics:
                    key = os.path.splitext(f)[0].lower()
                    folder = "%s/%s" % (DEST, d)
                    tag = "%s/%s" % (g, f)
                    names = _static_names(folder)
                    if tag in done or any(n == key or n == "sm_" + key or n.startswith(key + "_") or n.startswith("sm_" + key + "_") for n in names):
                        skipped += 1
                        task.frame("%s (already imported)" % f)
                        done.add(tag)
                        continue
                    if batch and imported >= batch:
                        stopped = True
                        break
                    task.frame(f)
                    before = names
                    import_glb(os.path.join(models, g, f), folder)
                    _flush()
                    _cap_textures(folder, recursive=False, names={n for n in (str(a.asset_name) for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False)) if n.lower() not in before})
                    imported += 1
                    done.add(tag)
                    _save_done(done)
                    _flush()
            _save_done(done)
    finally:
        for k, v in old.items():
            if v is not None:
                safe._cmd("%s %s" % (k, int(v) if float(v).is_integer() else v))
    out, path = _write_rigs(root, rigs) if rigs else (None, None)
    if task.cancelled:
        unreal.log_warning("[Aerondel] Import cancelled after %d new models; re-run to continue (finished models are skipped)." % imported)
        return
    if out:
        unreal.log("[Aerondel] rigs.json: %d rigs, %d monsters, %d npcs -> %s" % (len(out["rigs"]), len(out["monsters"]), len(out["npcs"]), path))
    msg = "[Aerondel] Lite models: %d imported this run, %d already there." % (imported, skipped)
    if stopped:
        unreal.log_warning(msg + " Batch limit reached: run it again for the next %d." % batch)
    else:
        unreal.log(msg + " All models done.")
