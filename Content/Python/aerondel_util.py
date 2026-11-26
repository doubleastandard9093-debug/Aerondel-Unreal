"""Shared helpers for the Aerondel editor scripts."""
import re
import unreal

ROOT = "/Game/Gielinor"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary

# UE object/asset names: ASCII letters, digits, underscore; no spaces or punctuation; cannot start with a digit.
_INVALID_ASSET_CHARS = re.compile(r"[^A-Za-z0-9_]+")


def safe_asset_name(name):
    """Turn a display name or raw token into a legal UE asset/object name (no spaces)."""
    raw = str(name or "").replace("\\", "/")
    if "/" in raw:
        raw = raw.rstrip("/").rsplit("/", 1)[-1]
    if "." in raw:
        a, b = raw.split(".", 1)
        return safe_asset_name(a) + "." + safe_asset_name(b)
    s = _INVALID_ASSET_CHARS.sub("_", raw)
    s = re.sub(r"_+", "_", s).strip("_")
    if not s:
        s = "Asset"
    if s[0].isdigit():
        s = "_" + s
    return s


def safe_asset_path(path):
    """Sanitize every path component after /Game or /Engine. Idempotent for already-safe paths."""
    p = str(path or "").replace("\\", "/")
    if not p:
        return p
    parts = p.split("/")
    out = []
    for part in parts:
        if part == "" or part in ("Game", "Engine"):
            out.append(part)
        else:
            out.append(safe_asset_name(part))
    return "/".join(out)


def is_valid_asset_path(path):
    """True if `path` is a /Game or /Engine path whose components are legal UE names."""
    if not path or not isinstance(path, str):
        return False
    p = path.replace("\\", "/")
    if " " in p:
        return False
    if not (p.startswith("/Game/") or p.startswith("/Engine/")):
        return False
    return p == safe_asset_path(p)


def create_asset(name, folder, cls, factory=None):
    """AssetTools.create_asset with a sanitized name/folder. Logs and returns None on failure."""
    name = safe_asset_name(name)
    folder = safe_asset_path(folder)
    try:
        asset = asset_tools.create_asset(name, folder, cls, factory or unreal.DataAssetFactory())
    except Exception as e:  # noqa: BLE001
        unreal.log_warning("[Aerondel] create_asset(%s/%s) failed: %s" % (folder, name, e))
        return None
    if not asset:
        unreal.log_warning("[Aerondel] create_asset(%s/%s) returned None" % (folder, name))
    return asset


def load_or_create(path, cls, factory=None):
    """Return the asset at path (/Game/Dir/Name), creating it with `factory` if missing.

    The leaf name is sanitized (spaces and other illegal characters become _). Returns None
    if creation fails (callers must handle that; mat() falls back to M_Tint).
    """
    path = safe_asset_path(path)
    if eal.does_asset_exist(path):
        asset = eal.load_asset(path)
        if asset and isinstance(asset, cls):
            return asset
        eal.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return create_asset(name, folder, cls, factory)


def set_props(obj, **props):
    if obj is None:
        unreal.log_warning("[Aerondel] set_props skipped: asset is None")
        return None
    for k, v in props.items():
        obj.set_editor_property(k, v)
    return obj


def color(r, g, b, a=1.0):
    return unreal.LinearColor(r, g, b, a)


def save(asset):
    if asset is None:
        unreal.log_warning("[Aerondel] save skipped: asset is None")
        return
    eal.save_loaded_asset(asset, only_if_is_dirty=False)


# ---------- materials ----------
_MAT_CACHE = {}
_FALLBACK_MAT = None


def tint_material():
    """M_Tint: Vector param Color -> BaseColor, Scalar Roughness/Metallic. Used by greybox and mob bodies."""
    path = ROOT + "/Materials/M_Tint"
    if eal.does_asset_exist(path):
        existing = eal.load_asset(path)
        if existing:
            return existing
    mel = unreal.MaterialEditingLibrary
    mat = create_asset("M_Tint", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        unreal.log_warning("[Aerondel] could not create M_Tint")
        return None
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


def _fallback_mat():
    """A material that is safe to assign when MI creation fails. Prefer a cached MI, then M_Tint."""
    global _FALLBACK_MAT
    if _FALLBACK_MAT:
        return _FALLBACK_MAT
    for v in _MAT_CACHE.values():
        if v is not None:
            _FALLBACK_MAT = v
            return v
    t = tint_material()
    if t:
        _FALLBACK_MAT = t
    return t


def mat(name, rgb, rough=0.8, metal=0.0):
    """Material instance of M_Tint, cached by sanitized name: /Game/Gielinor/Materials/MI_<name>."""
    key = safe_asset_name(name)
    if key in _MAT_CACHE:
        return _MAT_CACHE[key]
    path = ROOT + "/Materials/MI_" + key
    mi = load_or_create(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    parent = tint_material()
    if not mi or not parent:
        unreal.log_warning("[Aerondel] mat(%r) failed (path %s); using a fallback material" % (name, path))
        return _fallback_mat()
    mel = unreal.MaterialEditingLibrary
    mi.set_editor_property("parent", parent)
    mel.set_material_instance_vector_parameter_value(mi, "Color", color(*rgb))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.update_material_instance(mi)
    save(mi)
    _MAT_CACHE[key] = mi
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
    path = safe_asset_path(path)
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
