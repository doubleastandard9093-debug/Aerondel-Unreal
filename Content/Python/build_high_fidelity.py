"""High/Ultra-only material for the Aerondel world: /Game/Gielinor/Materials/High/M_TintHigh.

Same parameters as M_Tint (Color, Roughness, Metallic) plus Water (0/1). Adds world-space macro variation to colour and
roughness so flat greybox surfaces (grass, stone, wood, cloth, mob bodies) stop looking like single swatches, and an
animated shimmer on water. All procedural (engine Noise / Time nodes), no textures, original work.

Nothing on the Low or Quest path references this asset: AAerondelHighDetail soft-loads it only when the tier is High+.
Run from the editor: py build_high_fidelity.py (or import and call run()).
"""
import unreal
from aerondel_util import ROOT, asset_tools, eal, save, color

PATH = ROOT + "/Materials/High/M_TintHigh"


def _e(mat, cls, x, y, **props):
    n = unreal.MaterialEditingLibrary.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def run():
    mel = unreal.MaterialEditingLibrary
    if eal.does_asset_exist(PATH):
        eal.delete_asset(PATH)
    mat = asset_tools.create_asset("M_TintHigh", ROOT + "/Materials/High", unreal.Material, unreal.MaterialFactoryNew())
    E = unreal
    col = _e(mat, E.MaterialExpressionVectorParameter, -1200, -200, parameter_name="Color", default_value=color(0.8, 0.8, 0.8))
    rough = _e(mat, E.MaterialExpressionScalarParameter, -1200, 200, parameter_name="Roughness", default_value=0.8)
    metal = _e(mat, E.MaterialExpressionScalarParameter, -1200, 320, parameter_name="Metallic", default_value=0.0)
    water = _e(mat, E.MaterialExpressionScalarParameter, -1200, 440, parameter_name="Water", default_value=0.0)
    wpos = _e(mat, E.MaterialExpressionWorldPosition, -1600, 0)

    # Macro variation: low-frequency noise in world space -> 0.85..1.12 colour, 0.85..1.1 roughness.
    macro_scale = _e(mat, E.MaterialExpressionMultiply, -1400, 0, const_b=0.0015)
    mel.connect_material_expressions(wpos, "", macro_scale, "A")
    macro = _e(mat, E.MaterialExpressionNoise, -1200, 0, scale=1.0, levels=3, output_min=0.0, output_max=1.0)
    mel.connect_material_expressions(macro_scale, "", macro, "Position")
    tint = _e(mat, E.MaterialExpressionLinearInterpolate, -900, -100, const_a=0.85, const_b=1.12)
    mel.connect_material_expressions(macro, "", tint, "Alpha")
    base = _e(mat, E.MaterialExpressionMultiply, -700, -150)
    mel.connect_material_expressions(col, "", base, "A")
    mel.connect_material_expressions(tint, "", base, "B")
    rvar = _e(mat, E.MaterialExpressionLinearInterpolate, -900, 200, const_a=0.85, const_b=1.1)
    mel.connect_material_expressions(macro, "", rvar, "Alpha")
    rmul = _e(mat, E.MaterialExpressionMultiply, -700, 200)
    mel.connect_material_expressions(rough, "", rmul, "A")
    mel.connect_material_expressions(rvar, "", rmul, "B")

    # Water shimmer: faster noise panned by Time, added to colour and scaled by the Water switch.
    t = _e(mat, E.MaterialExpressionTime, -1600, 600)
    pan = _e(mat, E.MaterialExpressionMultiply, -1400, 600)
    mel.connect_material_expressions(t, "", pan, "A")
    drift = _e(mat, E.MaterialExpressionConstant3Vector, -1600, 700, constant=color(30.0, 20.0, 0.0))
    mel.connect_material_expressions(drift, "", pan, "B")
    wp = _e(mat, E.MaterialExpressionAdd, -1200, 600)
    mel.connect_material_expressions(wpos, "", wp, "A")
    mel.connect_material_expressions(pan, "", wp, "B")
    wscale = _e(mat, E.MaterialExpressionMultiply, -1000, 600, const_b=0.006)
    mel.connect_material_expressions(wp, "", wscale, "A")
    ripple = _e(mat, E.MaterialExpressionNoise, -800, 600, scale=1.0, levels=2, output_min=0.0, output_max=1.0)
    mel.connect_material_expressions(wscale, "", ripple, "Position")
    glint = _e(mat, E.MaterialExpressionMultiply, -600, 600, const_b=0.12)
    mel.connect_material_expressions(ripple, "", glint, "A")
    wglint = _e(mat, E.MaterialExpressionMultiply, -450, 500)
    mel.connect_material_expressions(glint, "", wglint, "A")
    mel.connect_material_expressions(water, "", wglint, "B")
    final = _e(mat, E.MaterialExpressionAdd, -300, -100)
    mel.connect_material_expressions(base, "", final, "A")
    mel.connect_material_expressions(wglint, "", final, "B")

    mel.connect_material_property(final, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rmul, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    mel.recompile_material(mat)
    save(mat)
    unreal.log("Aerondel: built " + PATH)


if __name__ == "__main__":
    run()
