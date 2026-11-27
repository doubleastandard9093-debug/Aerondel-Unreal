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
from aerondel_util import safe_asset_name
import importlib as _importlib
import aerondel_rig_math as _aerondel_rig_math
_importlib.reload(_aerondel_rig_math)
from aerondel_rig_math import bake_glb_for_unreal, validate_baked_for_interchange
from aerondel_clips import CLIP, clip_name, pick, apply_state_fallbacks

DEST = "/Game/Aerondel/Models"
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
    per-asset materials explosion (one material per glTF material).

    Axis / scale: the glTF translator already maps glTF Y-up to UE Z-up. Quaternius/Kenney files still
    carry a CharacterArmature node at -90 deg X with scale 100 that is *not* a skin joint; bake_meshes
    folds that node into the skeletal mesh and its clips so bind pose and animations share one space.
    use_t0_as_ref_pose stays OFF: these glTFs list Death as animation[0], so T0 would rebind the mesh
    to a corpse pose (bent, looking at the sky). force_front_x_axis / convert_scene are set when the
    property exists (FBX-style translators); _set no-ops them on glTF.
    """
    p = unreal.InterchangeGenericAssetsPipeline()
    mesh = p.get_editor_property("mesh_pipeline")
    _set(mesh, import_static_meshes=not skeletal, import_skeletal_meshes=skeletal, combine_skeletal_meshes=True,
         create_physics_asset=False, import_morph_targets=False, build_nanite=False, recompute_normals=True,
         recompute_tangents=True, use_mikk_t_space=True, compute_weighted_normals=True, generate_lightmap_u_vs=not skeletal)
    common = p.get_editor_property("common_meshes_properties")
    _set(common, recompute_normals=True, recompute_tangents=True, use_mikk_t_space=True, compute_weighted_normals=True,
         keep_sections_separate=False, bake_meshes=True, bake_pivot_meshes=True)
    common_sk = p.get_editor_property("common_skeletal_meshes_and_animations_properties")
    _set(common_sk, import_only_animations=False, convert_static_mesh_to_skeletal_mesh=False,
         use_t0_as_ref_pose=False, try_auto_select_skeleton=True)
    anim = p.get_editor_property("animation_pipeline")
    _set(anim, import_animations=skeletal, import_bone_tracks=True, import_custom_attribute=False)
    # Present on FBX import data / some pipeline versions; ignored when missing.
    _set(p, convert_scene=True, convert_scene_unit=True, force_front_x_axis=True)
    return p


def baked_glb_destination(src_path, saved_dir):
    """Where the Interchange-legal bake is written: Saved/Aerondel/baked_glb/<basename>."""
    return os.path.join(saved_dir, os.path.basename(src_path))


def import_glb(path, dest, skeletal=False):
    """Interchange import with our pipeline; falls back to a plain AssetImportTask if the scripted manager is missing.

    Skeletal meshes are always imported from the baked copy (one skin, identity root mesh,
    upright joint rest). The source glb is never sent to Interchange on a bake failure —
    that is what produced the lying multi-skin bind pose.
    """
    if skeletal:
        source = path
        try:
            saved = os.path.join(unreal.Paths.project_saved_dir(), "Aerondel", "baked_glb")
            os.makedirs(saved, exist_ok=True)
            baked = baked_glb_destination(source, saved)
            bake_glb_for_unreal(source, baked)
            problems = validate_baked_for_interchange(baked)
            if problems:
                raise RuntimeError("; ".join(problems))
            path = baked
            unreal.log("[Aerondel] baked %s -> %s (one skin, scene-root identity mesh, shared IBMs)" % (
                os.path.basename(source), baked))
        except Exception as e:
            unreal.log_error("[Aerondel] bind-space bake failed for %s: %s — not importing the source glb (it would lie down / split skins)" % (source, e))
            return []
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


def _additive_none():
    enum = getattr(unreal, "AdditiveAnimationType", None) or getattr(unreal, "EAdditiveAnimationType", None)
    if enum is None:
        return 0
    return getattr(enum, "AAT_NONE", None) or getattr(enum, "NONE", 0)


def tune_anims(folder, mesh_path):
    """Point every imported clip at the folder's skeleton and strip additive mode.

    A clip imported against a different skeleton (or marked additive with no base pose) is what
    folds the Adventurer into a bunched, sky-facing pose at runtime. Missing properties are skipped.
    """
    mesh = unreal.load_asset(mesh_path) if mesh_path else None
    skel = None
    if mesh:
        try:
            skel = mesh.get_editor_property("skeleton")
        except Exception:
            skel = getattr(mesh, "skeleton", None)
    none = _additive_none()
    for a in assets_in(folder):
        if _cls(a) != "AnimSequence":
            continue
        seq = unreal.load_asset(obj_path(a))
        if not seq:
            continue
        if skel:
            try:
                seq.set_editor_property("skeleton", skel)
            except Exception:
                pass
        try:
            seq.set_editor_property("additive_anim_type", none)
        except Exception:
            pass
        try:
            unreal.EditorAssetLibrary.save_loaded_asset(seq)
        except Exception:
            pass


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
    """Set import-only cvars that actually exist on this engine. Missing names are skipped silently."""
    old = {}
    for k, v in values.items():
        if not safe._cvar_exists(k):
            continue
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
    # Do not call the console command FlushRenderingCommands: it is not a cvar in UE 5.8
    # and spams "failed FindConsoleObject". The Python flush_rendering_commands above is enough.
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


def _is_anim_sequence(cls_name):
    n = str(cls_name or "")
    return n == "AnimSequence" or n.endswith("AnimSequence") or n == "AnimSequenceBase"


def _is_skeletal_mesh(cls_name):
    n = str(cls_name or "")
    return n == "SkeletalMesh" or n.endswith("SkeletalMesh")


def _scan_rig_once(folder):
    mesh, anims = None, []
    for a in assets_in(folder):
        c = _cls(a)
        if _is_skeletal_mesh(c) and not mesh:
            mesh = obj_path(a)
        elif _is_anim_sequence(c):
            anims.append((clip_name(a.asset_name), obj_path(a)))
    return mesh, anims


def _refresh_registry(folder):
    try:
        reg = unreal.AssetRegistryHelpers.get_asset_registry()
        if hasattr(reg, "scan_paths_synchronous"):
            reg.scan_paths_synchronous([folder], True)
        elif hasattr(reg, "search_all_assets"):
            reg.search_all_assets(True)
    except Exception:
        pass


def _folder_asset_summary(folder, limit=24):
    bits = []
    try:
        for a in assets_in(folder):
            bits.append("%s:%s" % (_cls(a), a.asset_name))
            if len(bits) >= limit:
                bits.append("...")
                break
    except Exception:
        return "(could not list)"
    return ", ".join(bits) or "(empty)"


def _scan_rig(folder):
    """Find the skeletal mesh and AnimSequences under folder. Refresh the registry if clips are late."""
    _refresh_registry(folder)
    mesh, anims = _scan_rig_once(folder)
    if mesh and not anims:
        _refresh_registry(folder)
        mesh, anims = _scan_rig_once(folder)
        if not anims:
            unreal.log_warning("[Aerondel] %s: skeletal mesh imported but no AnimSequence assets found (idle/walk/run will hold the reference pose). Assets: %s" % (folder, _folder_asset_summary(folder)))
    return mesh, anims


def _core_states_mapped(anims):
    """True if idle or walk would be written to rigs.json (C++ falls back to idle, then holds ref pose)."""
    return bool(pick(anims, "idle") or pick(anims, "walk"))


def _log_rig_clips(key, folder, mesh, anims, states, rates=None):
    """Always list the clips found for a rig and which states they mapped to."""
    names = [n for n, _ in anims]
    mapped = ["%s=%s" % (s, (p or "").rsplit(".", 1)[-1]) for s, p in states.items() if p]
    missing = [s for s in ("idle", "walk", "run", "attack") if not states.get(s)]
    unreal.log("[Aerondel] %s: %d clips [%s] -> states %s (mesh %s)" % (
        key, len(anims), ", ".join(names) if names else "none",
        ", ".join(mapped) if mapped else "none", mesh or "none"))
    frozen = [s for s, r in (rates or {}).items() if r == 0]
    if frozen:
        unreal.log("[Aerondel] %s: frozen fallback (play rate 0) for %s" % (key, ",".join(frozen)))
    if missing:
        extra = ""
        if not anims:
            extra = "; folder assets: " + _folder_asset_summary(folder)
        unreal.log_warning("[Aerondel] %s: no clip mapped for %s — Play will hold the reference pose for those states%s" % (
            key, ",".join(missing), extra))


def _static_names(folder):
    return {str(a.asset_name).lower() for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False)}


def _write_rigs(root, rigs):
    rmap = json.load(open(os.path.join(root, "rig_map.json"), encoding="utf-8"))
    rigs = dict(rigs)
    for alias, target in rmap.get("aliases", {}).items():
        if target in rigs:
            rigs[alias] = rigs[target]
    out = {
        "yaw": rmap.get("yaw", 0),
        "pitch": rmap.get("pitch", 0),
        "roll": rmap.get("roll", 0),
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
                folder = "%s/%s/%s" % (DEST, safe_asset_name(d), safe_asset_name(key))
                mesh, anims = _scan_rig(folder)
                # A mesh without idle/walk (failed anim import, e.g. NodeUidPtr) must be reimported,
                # not skipped, or rigs.json stays empty and Play holds the reference pose.
                if mesh and _core_states_mapped(anims):
                    skipped += 1  # already imported (by an earlier, maybe crashed, run): keep it, just map it
                    task.frame("%s (already imported)" % f)
                else:
                    if mesh and anims is not None and not _core_states_mapped(anims):
                        unreal.log_warning("[Aerondel] %s: skeletal mesh is present but idle/walk clips are missing — reimporting" % f)
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
                if mesh:
                    tune_anims(folder, mesh)
                    # Clips may appear only after tune/save; scan once more before writing rigs.json.
                    if not _core_states_mapped(anims):
                        mesh, anims = _scan_rig(folder)
                done.add("%s/%s" % (g, f))
                _save_done(done)
                states = {s: pick(anims, s) for s in CLIP}
                states, rates = apply_state_fallbacks(anims, states)
                entry = {"mesh": mesh, "anims": {s: p for s, p in states.items() if p}}
                if rates:
                    entry["animRates"] = rates
                rigs[key] = entry
                _write_rigs(root, rigs)  # partial rigs.json after every model, so a crash loses nothing
                _log_rig_clips(key, folder, mesh, anims, states, rates)
            if not stopped:
                for g, d, f in statics:
                    key = os.path.splitext(f)[0].lower()
                    folder = "%s/%s" % (DEST, safe_asset_name(d))
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
