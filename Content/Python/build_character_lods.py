"""
Aerondel > 9. Build Character LODs: adds auto-reduced LODs (LOD0 untouched) to the shared rigged NPC / monster
skeletal meshes under /Game/Aerondel/Models, so the same few human meshes are reused by every NPC and distant ones
cost less. The player's rig (rigs.json "player") is skipped so its appearance cannot change. Meshes that already
have >= TARGET_LODS LODs are left alone, so re-running is safe. Run after 7/7b Import Lite Models; save afterwards.
"""
import json
import os

try:
    import unreal
except ImportError:  # offline tests
    unreal = None

DEST = "/Game/Aerondel/Models"
TARGET_LODS = 3  # LOD0 (original) + 2 reductions


def player_mesh_paths(rigs):
    """Mesh object paths used by the player rig (never touched)."""
    key = rigs.get("player")
    rig = rigs.get("rigs", {}).get(key) if key else None
    m = rig.get("mesh") if isinstance(rig, dict) else None
    return {m.split(".")[0]} if m else set()


def plan(mesh_paths, lod_counts, skip):
    """-> list of mesh paths that need LODs (not the player, fewer than TARGET_LODS)."""
    return [p for p in mesh_paths if p.split(".")[0] not in skip and lod_counts.get(p, 1) < TARGET_LODS]


def run():
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "rigs.json")
    rigs = json.load(open(path, encoding="utf-8")) if os.path.exists(path) else {}
    skip = player_mesh_paths(rigs)
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = reg.get_assets_by_path(DEST, recursive=True)
    meshes = [a.get_asset().get_path_name() for a in found if str(a.asset_class_path.asset_name) == "SkeletalMesh"]
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    counts = {}
    for p in meshes:
        sm = unreal.load_asset(p)
        counts[p] = sub.get_lod_count(sm) if hasattr(sub, "get_lod_count") else sm.get_editor_property("lod_info").__len__()
    todo = plan(meshes, counts, skip)
    done = 0
    for p in todo:
        sm = unreal.load_asset(p)
        try:
            if sub.regenerate_lod(sm, TARGET_LODS, False, False):
                unreal.EditorAssetLibrary.save_loaded_asset(sm)
                done += 1
        except Exception as e:  # keep going; one bad mesh must not stop the rest
            unreal.log_warning("[Aerondel] LODs skipped for %s (%s)" % (p, e))
    unreal.log("[Aerondel] Character LODs: %d/%d meshes updated, %d skeletal meshes total, player rig skipped (%s)"
               % (done, len(todo), len(meshes), ", ".join(sorted(skip)) or "none mapped"))
    return done
