"""Aerondel > 0L. BUILD FULL WORLD - LAPTOP (slim, low memory).

Same gameplay coverage as 0. BUILD FULL GAME WORLD (every town, region, tree, rock,
fishing spot and monster) but built in saved, resumable chunks for an 8 GB / Intel UHD
laptop. Skips optional heavy steps (high-fidelity and realism materials, festival decor,
HLODs, Lite model import unless already present). Re-run the same menu item after a crash
to resume from Saved/AerondelLaptopProgress.json.
"""
import json
import os
import importlib
import unreal
from aerondel_util import try_call, editor_world, save_level
import aerondel_safe as safe
import aerondel_all
import build_world_blockout as blockout
import build_aerondel_regions
import world_placements
import world_partition as wpart
from build_starter_town import actors, levels

PROGRESS_NAME = "AerondelLaptopProgress.json"
REGION_CHUNK = 3
PLACE_CHUNK = 50
LAPTOP_DENSITY = 0.4          # decorative biome props only; dig sites and hubs always placed
LAPTOP_CELL_CAP = 30          # decorative pieces per 128 m cell (gameplay actors are uncapped)
LAPTOP_LOADING_RANGE_CM = 12800.0  # 1 cell, editor-only during the build; shipped range restored at finalize
SKIPPED_OPTIONAL = (
    "high_fidelity",
    "realism_materials",
    "festival_decor",
    "hlod",
    "lite_import_unless_present",
)


def progress_path():
    return os.path.join(unreal.Paths.project_saved_dir(), PROGRESS_NAME)


def load_progress():
    path = progress_path()
    if not os.path.exists(path):
        return None
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if isinstance(data, dict):
            return data
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Aerondel] could not read %s: %s" % (path, e))
    return None


def save_progress(data):
    path = progress_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)


def chunk_ids(n_regions, n_nodes, n_mobs, region_chunk=REGION_CHUNK, place_chunk=PLACE_CHUNK):
    """Deterministic laptop chunk list covering every region and every placement."""
    ids = ["setup", "world_base"]
    for i in range(_nchunks(n_regions, region_chunk)):
        ids.append("regions_%02d" % i)
    ids.append("shores")
    for i in range(_nchunks(n_nodes + n_mobs, place_chunk)):
        ids.append("placements_%02d" % i)
    ids.append("finalize")
    return ids


def _nchunks(n, size):
    if n <= 0:
        return 0
    return (n + size - 1) // size


def region_slices(regions, region_chunk=REGION_CHUNK):
    return [regions[i:i + region_chunk] for i in range(0, len(regions), region_chunk)]


def placement_slices(nodes, mobs, place_chunk=PLACE_CHUNK):
    items = [("node", n) for n in nodes] + [("mob", m) for m in mobs]
    out = []
    for i in range(0, len(items), place_chunk):
        chunk = items[i:i + place_chunk]
        out.append((
            [x[1] for x in chunk if x[0] == "node"],
            [x[1] for x in chunk if x[0] == "mob"],
        ))
    return out


def lite_models_present():
    """True if 7 / 7b has already written rigs.json with at least one rig."""
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "rigs.json")
    if not os.path.exists(path):
        return False
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f) or {}
        return bool(data.get("rigs"))
    except Exception:  # noqa: BLE001
        return False


def _collect():
    try_call(unreal.SystemLibrary, "collect_garbage", warn=False)


def _breathe(msg=None):
    """Save the level and dirty packages, then release references."""
    if msg:
        unreal.log("[Aerondel] " + msg)
    try:
        save_level()
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Aerondel] laptop level save failed: %s" % e)
    safe.save_dirty()
    _collect()


def destroy_chunk_actors(tag):
    """Remove actors tagged laptop_chunk:<tag> (a partial chunk after a crash)."""
    needle = "laptop_chunk:" + tag
    n = 0
    for a in (try_call(actors, "get_all_level_actors", default=None) or []):
        tags = try_call(a, "get_editor_property", "tags", default=None) or []
        names = [str(t) for t in tags]
        if any(needle == s or needle in s for s in names):
            try_call(actors, "destroy_actor", a)
            n += 1
    if n:
        unreal.log("[Aerondel] removed %d actors from incomplete chunk %s" % (n, tag))
        _collect()
    return n


def _mark(progress, chunk, status="in_progress"):
    if status == "in_progress":
        progress["in_progress"] = chunk
    elif status == "done":
        done = list(progress.get("done") or [])
        if chunk not in done:
            done.append(chunk)
        progress["done"] = done
        progress["in_progress"] = None
    save_progress(progress)


def run():
    """Full-coverage, low-memory, resumable World build. Safe to re-run after a crash."""
    importlib.reload(blockout)
    importlib.reload(build_aerondel_regions)
    importlib.reload(world_placements)
    importlib.reload(wpart)

    w = blockout.load_world()
    if not isinstance(w, dict):
        unreal.log_error("[Aerondel] world.json did not load as an object")
        return
    nodes, mobs = world_placements.plan(
        world_placements.load(unreal.Paths.project_content_dir()), small=False)
    regions = list(w.get("regions") or [])
    ids = chunk_ids(len(regions), len(nodes), len(mobs))
    r_slices = region_slices(regions)
    p_slices = placement_slices(nodes, mobs)
    lite = lite_models_present()

    unreal.log("[Aerondel] ------------------------------------------------------------------")
    unreal.log("[Aerondel] LAPTOP WORLD BUILD (slim, low memory)")
    unreal.log("[Aerondel] Aimed at ~8 GB RAM / Intel UHD (e.g. ASUS Vivobook Go 15, i3-N305).")
    unreal.log("[Aerondel] Close other apps. Expect several GB of editor memory; do not raise")
    unreal.log("[Aerondel] the viewport to High/Ultra while this runs. Scalability is forced")
    unreal.log("[Aerondel] Low for the session and restored afterwards. No ini/cook changes.")
    unreal.log("[Aerondel] Coverage: %d towns, %d regions, %d resource nodes, %d mobs (complete)."
               % (len(w.get("towns") or {}), len(regions), len(nodes), len(mobs)))
    unreal.log("[Aerondel] %d resumable chunks (progress: Saved/%s)." % (len(ids), PROGRESS_NAME))
    unreal.log("[Aerondel] Skipping optional: high-fidelity & realism materials, festival decor, HLODs.")
    if lite:
        unreal.log("[Aerondel] Lite models already imported (rigs.json present); they will be used.")
    else:
        unreal.log("[Aerondel] Lite models not imported — greybox primitives will be used. "
                   "After this finishes, run Aerondel > 7b. Import Lite Models (10 per run) if you want CC0 rigs.")
    unreal.log("[Aerondel] If this crashes, just run 0L again; completed chunks are skipped.")
    unreal.log("[Aerondel] ------------------------------------------------------------------")

    progress = load_progress() or {"version": 1, "status": "in_progress", "done": [], "in_progress": None}
    if progress.get("status") == "complete" and set(progress.get("done") or []) >= set(ids):
        unreal.log("[Aerondel] Laptop world already complete. To rebuild from scratch, delete Saved/%s and run 0L again."
                   % PROGRESS_NAME)
        blockout.write_build_marker(False, len(nodes), len(mobs), laptop=True, skipped=SKIPPED_OPTIONAL)
        # Still ensure nav: a completed 0L can lack RecastNavMesh if paths were never built.
        if blockout.open_or_create_map(
            blockout.MAP, blockout.levels, partitioned=True,
            create_partitioned=wpart.new_partitioned_level, clear=False,
        ):
            blockout.ensure_world_navmesh(float(w.get("worldHalf", 300)))
            try:
                wpart.save_all()
            except Exception:
                pass
        return

    done = list(progress.get("done") or [])
    unfinished = progress.get("in_progress")
    resuming = bool(done or unfinished)
    if resuming:
        unreal.log("[Aerondel] Resuming laptop build: %d/%d chunks already done%s."
                   % (len(done), len(ids), (", will retry " + unfinished) if unfinished else ""))

    old_cap = safe.CELL_CAP
    safe.CELL_CAP = LAPTOP_CELL_CAP
    frames = max(len(ids), 1)
    try:
        with safe.SafeRun("Laptop world build", frames, save_fn=save_level,
                          cvar_overrides={"t.MaxFPS": 30}) as task:
            ok = _run_chunks(w, nodes, mobs, regions, ids, r_slices, p_slices, progress, task, lite, resuming)
            if task.cancelled or not ok:
                if task.cancelled:
                    unreal.log_warning("[Aerondel] Laptop build cancelled. Re-run 0L to resume from Saved/%s." % PROGRESS_NAME)
                return
    finally:
        safe.CELL_CAP = old_cap

    progress["status"] = "complete"
    progress["in_progress"] = None
    progress["done"] = ids
    save_progress(progress)
    blockout.write_build_marker(False, len(nodes), len(mobs), laptop=True, skipped=SKIPPED_OPTIONAL)
    unreal.log("[Aerondel] Laptop world build complete: %d nodes, %d mobs, mode=laptop. "
               "Packaging is allowed (full coverage); high-fidelity, decor and HLODs were skipped."
               % (len(nodes), len(mobs)))


def _run_chunks(w, nodes, mobs, regions, ids, r_slices, p_slices, progress, task, lite, resuming):  # noqa: ARG001  (lite/resuming logged by run)
    done = set(progress.get("done") or [])

    def should_run(chunk):
        if chunk in done:
            unreal.log("[Aerondel] laptop chunk %s already done — skipping" % chunk)
            return False
        if progress.get("in_progress") == chunk:
            destroy_chunk_actors(chunk)
        return True

    # setup (assets + starter town; no high-fidelity)
    task.frame("setup")
    if should_run("setup"):
        _mark(progress, "setup", "in_progress")
        importlib.reload(aerondel_all)
        aerondel_all.run(include_high_fidelity=False)
        _breathe("chunk setup saved (input, starter data, smooth shapes, Starter Town)")
        _mark(progress, "setup", "done")
        done.add("setup")

    # World map: clear only on a fresh world_base; resume keeps already-placed actors.
    world_base_done = "world_base" in done
    if not blockout.open_or_create_map(
        blockout.MAP, levels, partitioned=True,
        create_partitioned=wpart.new_partitioned_level, clear=not world_base_done,
    ):
        unreal.log_error("[Aerondel] Could not open or create " + blockout.MAP)
        return False
    if wpart.is_partitioned():
        # Editor-session only: shrink streamed range while placing. Finalize writes the shipped range.
        try:
            wpart.configure_runtime_grid(loading_range=LAPTOP_LOADING_RANGE_CM)
            unreal.log("[Aerondel] World Partition editor loading range set to %.0f cm for this session "
                       "(shipped range restored at finalize)." % LAPTOP_LOADING_RANGE_CM)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("[Aerondel] could not lower editor loading range: %s" % e)

    half = float(w.get("worldHalf", 300))

    task.frame("world_base")
    if should_run("world_base"):
        _mark(progress, "world_base", "in_progress")
        with safe.chunk_scope("world_base"):
            blockout.build_world_base(w, half, task)
        _breathe("chunk world_base saved (ground, towns, buildings, places, sea)")
        _mark(progress, "world_base", "done")
        done.add("world_base")

    for i, sl in enumerate(r_slices):
        cid = "regions_%02d" % i
        task.frame(cid)
        if not should_run(cid):
            continue
        _mark(progress, cid, "in_progress")
        with safe.chunk_scope(cid):
            build_aerondel_regions.build(w, density=LAPTOP_DENSITY, regions=sl)
        _breathe("chunk %s saved (%d regions, decorative density %.1f)" % (cid, len(sl), LAPTOP_DENSITY))
        _mark(progress, cid, "done")
        done.add(cid)

    task.frame("shores")
    if should_run("shores"):
        _mark(progress, "shores", "in_progress")
        with safe.chunk_scope("shores"):
            world_placements.build(w, small=False, nodes=[], mobs=[], shores=True)
        _breathe("chunk shores saved")
        _mark(progress, "shores", "done")
        done.add("shores")

    for i, (sl_nodes, sl_mobs) in enumerate(p_slices):
        cid = "placements_%02d" % i
        task.frame(cid)
        if not should_run(cid):
            continue
        _mark(progress, cid, "in_progress")
        with safe.chunk_scope(cid):
            world_placements.build(w, small=False, nodes=sl_nodes, mobs=sl_mobs, shores=False)
        _breathe("chunk %s saved (%d nodes, %d mobs)" % (cid, len(sl_nodes), len(sl_mobs)))
        _mark(progress, cid, "done")
        done.add(cid)
        sl_nodes = sl_mobs = None  # drop references before GC (already collected in _breathe)

    task.frame("finalize")
    if should_run("finalize"):
        _mark(progress, "finalize", "in_progress")
        # Restore the shipped loading range so the packaged game is unchanged.
        blockout.finalize_world(task)
        _breathe("chunk finalize saved (World Partition range restored, packages flushed)")
        _mark(progress, "finalize", "done")
        done.add("finalize")
    return True


