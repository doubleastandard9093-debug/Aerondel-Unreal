"""
World Partition helpers for the Gielinor builders (UE 5.8).

The World map (Build World Blockout) is created as a partitioned world so its actors
stream in cells around the player instead of all loading at once. That lets the map
grow (new regions in Content/Data/world.json) without raising memory or draw calls
on the 8 GB laptop or on Android.

- Runtime grid "MainGrid": 128 m cells, 384 m loading range (desktop High/Ultra).
  Lower tiers shrink the range at runtime (UGielinorScalability: Low 256 m, Quest/Android 160 m).
- Every actor is spatially loaded except the essentials, which stay always loaded:
  PlayerStart, lights/sky/fog/post-process, NavMeshBounds, the world-sized ground and sea,
  and anything whose folder starts with an ALWAYS_LOADED_FOLDERS entry.
- NavMesh switches to dynamic generation on this map so tiles rebuild as cells stream in
  (a static navmesh can't follow streamed geometry). StarterTown is unchanged.

Every engine call that differs between versions is wrapped: if one isn't available the
builder logs exactly what to set by hand instead of failing the whole build.
"""
import unreal

CELL_SIZE_CM = 12800.0       # 128 m cells
LOADING_RANGE_CM = 38400.0   # 384 m (3 cells) on High/Ultra
ALWAYS_LOADED_FOLDERS = ("Environment", "Sea")
ALWAYS_LOADED_LABELS = ("PlayerStart", "Ground", "NavMeshBounds", "Sarim_Sound")
ALWAYS_LOADED_CLASSES = ("PlayerStart", "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog",
                         "PostProcessVolume", "NavMeshBoundsVolume", "RecastNavMesh", "VolumetricCloud", "WorldDataLayers",
                         "WorldPartitionMiniMap", "AbstractNavData")


def new_partitioned_level(path, levels):
    """Create `path` as a World Partition map. Returns True on success."""
    try:
        if levels.new_level(path, True):  # LevelEditorSubsystem.new_level(asset_path, is_partitioned_world)
            unreal.log("[Gielinor] Created World Partition map " + path)
            return True
    except TypeError:
        pass
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Gielinor] new_level(partitioned) failed: %s" % e)
    for tmpl in ("/Engine/Maps/Templates/OpenWorld", "/Engine/Maps/Templates/Template_Default"):
        try:
            if unreal.EditorAssetLibrary.does_asset_exist(tmpl) and levels.new_level_from_template(path, tmpl):
                unreal.log_warning("[Gielinor] Created %s from %s. If it isn't partitioned, use Tools > Convert Level after the build." % (path, tmpl))
                return True
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("[Gielinor] new_level_from_template(%s) failed: %s" % (tmpl, e))
    if levels.new_level(path):
        unreal.log_warning("[Gielinor] Created %s WITHOUT World Partition. Convert it: Tools > Convert Level (see README)." % path)
        return True
    return False


def _world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:  # noqa: BLE001
        return unreal.EditorLevelLibrary.get_editor_world()


def is_partitioned():
    w = _world()
    try:
        ws = w.get_world_settings()
        return ws.get_editor_property("world_partition") is not None
    except Exception:  # noqa: BLE001
        return False


def configure_runtime_grid(cell=CELL_SIZE_CM, loading_range=LOADING_RANGE_CM):
    """Best effort: set the main runtime grid's cell size and loading range."""
    manual = "World Settings > World Partition Setup > Runtime Hash: MainGrid Cell Size %d, Loading Range %d" % (cell, loading_range)
    try:
        wp = _world().get_world_settings().get_editor_property("world_partition")
        rh = wp.get_editor_property("runtime_hash")
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Gielinor] Could not read the World Partition runtime hash (%s). Set by hand: %s" % (e, manual))
        return False
    # Classic spatial hash: Grids[]. Runtime hash set (5.4+): RuntimePartitions[].MainLayer.
    for prop in ("grids", "runtime_partitions"):
        try:
            grids = rh.get_editor_property(prop)
        except Exception:  # noqa: BLE001
            continue
        if not grids:
            continue
        g = grids[0]
        try:
            target = g
            if prop == "runtime_partitions":
                target = g.get_editor_property("main_layer")
            target.set_editor_property("cell_size", int(cell))
            target.set_editor_property("loading_range", float(loading_range))
            if prop == "runtime_partitions":
                g.set_editor_property("main_layer", target)
            grids[0] = g
            rh.set_editor_property(prop, grids)
            unreal.log("[Gielinor] Runtime grid: %d cm cells, %d cm loading range" % (cell, loading_range))
            return True
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("[Gielinor] Runtime grid edit via %s failed (%s)" % (prop, e))
    unreal.log_warning("[Gielinor] Runtime grid not changed automatically. Set by hand: " + manual)
    return False


def _always_loaded(a):
    cls = a.get_class().get_name()
    if cls in ALWAYS_LOADED_CLASSES:
        return True
    if a.get_actor_label() in ALWAYS_LOADED_LABELS:
        return True
    folder = str(a.get_folder_path())
    return any(folder == f or folder.startswith(f + "/") for f in ALWAYS_LOADED_FOLDERS)


def apply_streaming_flags(actors):
    """Spatially load everything except the essentials. Returns (streamed, always)."""
    streamed = always = 0
    for a in actors.get_all_level_actors():
        keep = _always_loaded(a)
        try:
            a.set_editor_property("is_spatially_loaded", not keep)
        except Exception:  # noqa: BLE001
            continue
        if keep:
            always += 1
        else:
            streamed += 1
    unreal.log("[Gielinor] World Partition: %d actors stream by cell, %d always loaded" % (streamed, always))
    return streamed, always


def dynamic_navmesh(actors):
    """Static navmesh can't follow streamed cells; regenerate tiles at runtime on this map only."""
    n = 0
    for a in actors.get_all_level_actors():
        if a.get_class().get_name() == "RecastNavMesh":
            try:
                a.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
                n += 1
            except Exception as e:  # noqa: BLE001
                unreal.log_warning("[Gielinor] RecastNavMesh runtime generation not set (%s); set Runtime Generation = Dynamic by hand." % e)
    return n


def save_all():
    """Partitioned maps keep one file per actor; save those packages too."""
    try:
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Gielinor] save_dirty_packages failed (%s). Use File > Save All." % e)
