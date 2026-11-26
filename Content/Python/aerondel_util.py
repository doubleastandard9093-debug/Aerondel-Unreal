"""Shared helpers for the Aerondel editor scripts."""
import unreal

ROOT = "/Game/Gielinor"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def load_or_create(path, cls, factory=None):
    """Return the asset at path (/Game/Dir/Name), creating it with `factory` if missing."""
    if eal.does_asset_exist(path):
        asset = eal.load_asset(path)
        if asset and isinstance(asset, cls):
            return asset
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return asset_tools.create_asset(name, folder, cls, factory or unreal.DataAssetFactory())


def set_props(obj, **props):
    for k, v in props.items():
        obj.set_editor_property(k, v)
    return obj


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


def save(asset):
    eal.save_loaded_asset(asset, only_if_is_dirty=False)


# ---------- materials ----------
_MAT_CACHE = {}


def tint_material():
    """M_Tint: Vector param Color -> BaseColor, Scalar Roughness/Metallic. Used by greybox and mob bodies."""
    path = ROOT + "/Materials/M_Tint"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    mel = unreal.MaterialEditingLibrary
    mat = asset_tools.create_asset("M_Tint", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    col = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, -100)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", color(0.8, 0.8, 0.8))
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 120)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.8)
    metal = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 220)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 0.0)
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(mat)
    save(mat)
    return mat


def mat(name, rgb, rough=0.8, metal=0.0):
    """Material instance of M_Tint, cached by name: /Game/Gielinor/Materials/MI_<name>."""
    if name in _MAT_CACHE:
        return _MAT_CACHE[name]
    path = ROOT + "/Materials/MI_" + name
    mi = load_or_create(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mi.set_editor_property("parent", tint_material())
    mel.set_material_instance_vector_parameter_value(mi, "Color", color(*rgb))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.update_material_instance(mi)
    save(mi)
    _MAT_CACHE[name] = mi
    return mi


# ---------- maps ----------
_KEEP_ACTOR_CLASSES = (
    "WorldSettings", "WorldPartition", "WorldDataLayers", "AbstractNavData",
    "WorldPartitionMiniMap", "LevelInstance",
)


def _class_name(actor):
    try:
        return actor.get_class().get_name()
    except Exception:
        return ""


def clear_generated_level_actors():
    """Destroy previously generated actors in the current level, keeping World Settings / partition bookkeeping."""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in list(actors.get_all_level_actors() or []):
        cls = _class_name(a)
        if any(s in cls for s in _KEEP_ACTOR_CLASSES):
            continue
        try:
            actors.destroy_actor(a)
        except Exception:
            pass


def _try_load_level(levels, path):
    """Best-effort load of an existing map. LevelEditorSubsystem.load_level is UE 5; fall back if the name differs."""
    errors = []
    for fn in (
        lambda: levels.load_level(path),
        lambda: getattr(levels, "load_map")(path) if hasattr(levels, "load_map") else None,
        lambda: unreal.EditorLevelLibrary.load_level(path) if hasattr(unreal, "EditorLevelLibrary") else None,
    ):
        try:
            result = fn()
        except Exception as e:  # noqa: BLE001
            errors.append(str(e))
            continue
        if result:
            return True
    if errors:
        unreal.log_warning("[Aerondel] load_level(%s) failed: %s" % (path, "; ".join(errors[-2:])))
    return False


def open_or_create_map(path, levels, partitioned=False, create_partitioned=None):
    """Open `path` if it already exists (and clear generated actors); otherwise create it.

    Delete-then-NewLevel fails in UE once /Game/Gielinor/Maps/StarterTown (or World, RelicDungeon)
    is already on disk. Loading and rebuilding actors is the idempotent path.
    Returns True if the editor's current level is `path` and ready to spawn into.
    """
    if eal.does_asset_exist(path):
        if _try_load_level(levels, path):
            clear_generated_level_actors()
            unreal.log("[Aerondel] Loaded existing " + path + " (rebuilding actors)")
            return True
        unreal.log_warning("[Aerondel] Could not load existing " + path + "; trying delete and recreate")
        try:
            eal.delete_asset(path)
        except Exception as e:  # noqa: BLE001
            unreal.log_warning("[Aerondel] delete_asset(%s) failed: %s" % (path, e))
            if eal.does_asset_exist(path) and _try_load_level(levels, path):
                clear_generated_level_actors()
                return True
            return False
    if partitioned and create_partitioned:
        return bool(create_partitioned(path, levels))
    try:
        ok = levels.new_level(path, True) if partitioned else levels.new_level(path)
    except TypeError:
        ok = levels.new_level(path)
    if ok:
        return True
    unreal.log_error("[Aerondel] Could not create " + path)
    return False
