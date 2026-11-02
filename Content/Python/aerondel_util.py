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
