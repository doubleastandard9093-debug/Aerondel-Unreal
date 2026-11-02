"""
Aerondel > 8. Build Realism Materials: Unreal twin of the web js/realism.js character pass (High/Ultra only).

  /Game/Gielinor/Materials/Real/M_RealSkin      Subsurface Profile shading (SP_AerondelSkin), soft spec, micro detail normal
  /Game/Gielinor/Materials/Real/M_RealCloth     Cloth shading (sheen/fuzz) for fur, cloth and hair
  /Game/Gielinor/Materials/Real/M_RealSurface   Default lit for leather, metal, horn, wood, bone, hide
  /Game/Gielinor/Materials/Real/MI_Real_<kind>  one instance per surface kind with the web's roughness/metal/sheen values

UAerondelModelLibrary::ApplyRealism (C++) swaps rigged monsters / NPCs / player onto these at runtime when the graphics
tier is High or Ultra, keeping each slot's original colour/texture. Low/Quest never load them (the i3/iGPU tier stays light).
All masters are cheap: no extra textures beyond the model's own, the detail normal is two procedural noise taps on UV0.
"""
import unreal
from aerondel_util import ROOT, asset_tools, eal, color, save

DIR = ROOT + "/Materials/Real"
MEL = unreal.MaterialEditingLibrary
WHITE = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"

# kind: master, roughness, metallic, specular, sheen(cloth), detail strength, detail scale  (web K table)
KINDS = {
    "skin":    ("Skin", 0.52, 0.0, 0.40, 0.00, 0.10, 38),
    "hide":    ("Surface", 0.62, 0.0, 0.45, 0.00, 0.20, 22),
    "fur":     ("Cloth", 0.85, 0.0, 0.30, 1.00, 0.35, 55),
    "hair":    ("Cloth", 0.55, 0.0, 0.40, 0.80, 0.25, 70),
    "cloth":   ("Cloth", 0.82, 0.0, 0.30, 0.70, 0.22, 48),
    "leather": ("Surface", 0.58, 0.0, 0.45, 0.00, 0.28, 30),
    "wood":    ("Surface", 0.78, 0.0, 0.35, 0.00, 0.35, 18),
    "metal":   ("Surface", 0.32, 0.9, 0.50, 0.00, 0.10, 60),
    "horn":    ("Surface", 0.40, 0.0, 0.55, 0.00, 0.15, 30),
    "bone":    ("Surface", 0.68, 0.0, 0.40, 0.00, 0.30, 26),
}


def _e(m, cls, x, y, **props):
    n = MEL.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def _link(a, b, pin, ao=""):
    MEL.connect_material_expressions(a, ao, b, pin)


def _skin_profile():
    path = DIR + "/SP_AerondelSkin"
    if eal.does_asset_exist(path):
        return eal.load_asset(path)
    fac = getattr(unreal, "SubsurfaceProfileFactory", None)
    if fac is None:
        return None
    sp = asset_tools.create_asset("SP_AerondelSkin", DIR, unreal.SubsurfaceProfile, fac())
    save(sp)
    return sp


def _master(kind):
    path = DIR + "/M_Real" + kind
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = asset_tools.create_asset("M_Real" + kind, DIR, unreal.Material, unreal.MaterialFactoryNew())
    E = unreal
    tint = _e(m, E.MaterialExpressionVectorParameter, -1100, -300, parameter_name="Tint", default_value=color(1, 1, 1))
    tex = _e(m, E.MaterialExpressionTextureSampleParameter2D, -1100, -120, parameter_name="BaseTex", texture=unreal.load_asset(WHITE))
    use = _e(m, E.MaterialExpressionScalarParameter, -1100, 120, parameter_name="UseTex", default_value=0.0)
    textint = _e(m, E.MaterialExpressionMultiply, -800, -200)
    _link(tint, textint, "A"); _link(tex, textint, "B", "RGB")
    base = _e(m, E.MaterialExpressionLinearInterpolate, -600, -250)
    _link(tint, base, "A"); _link(textint, base, "B"); _link(use, base, "Alpha")
    rough = _e(m, E.MaterialExpressionScalarParameter, -600, 0, parameter_name="Roughness", default_value=0.7)
    metal = _e(m, E.MaterialExpressionScalarParameter, -600, 100, parameter_name="Metallic", default_value=0.0)
    spec = _e(m, E.MaterialExpressionScalarParameter, -600, 200, parameter_name="Specular", default_value=0.4)
    # Micro detail normal (web: bump map): two noise taps on UV0 * DetailScale -> tangent-space XY tilt.
    uv = _e(m, E.MaterialExpressionTextureCoordinate, -1500, 400)
    dscale = _e(m, E.MaterialExpressionScalarParameter, -1500, 500, parameter_name="DetailScale", default_value=40.0)
    uvs = _e(m, E.MaterialExpressionMultiply, -1300, 420); _link(uv, uvs, "A"); _link(dscale, uvs, "B")
    p3 = _e(m, E.MaterialExpressionAppendVector, -1150, 420, ); _link(uvs, p3, "A")
    zero = _e(m, E.MaterialExpressionConstant, -1300, 520, r=0.0); _link(zero, p3, "B")
    n1 = _e(m, E.MaterialExpressionNoise, -1000, 380, scale=1.0, levels=2, output_min=-1.0, output_max=1.0)
    _link(p3, n1, "Position")
    off = _e(m, E.MaterialExpressionAdd, -1150, 560, const_b=17.3); _link(p3, off, "A")
    n2 = _e(m, E.MaterialExpressionNoise, -1000, 560, scale=1.0, levels=2, output_min=-1.0, output_max=1.0)
    _link(off, n2, "Position")
    xy = _e(m, E.MaterialExpressionAppendVector, -800, 450); _link(n1, xy, "A"); _link(n2, xy, "B")
    dstr = _e(m, E.MaterialExpressionScalarParameter, -800, 560, parameter_name="DetailStrength", default_value=0.2)
    xys = _e(m, E.MaterialExpressionMultiply, -650, 450); _link(xy, xys, "A"); _link(dstr, xys, "B")
    one = _e(m, E.MaterialExpressionConstant, -650, 560, r=1.0)
    nrm = _e(m, E.MaterialExpressionAppendVector, -500, 450); _link(xys, nrm, "A"); _link(one, nrm, "B")

    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    if kind == "Skin":
        sp = _skin_profile()
        if sp:
            m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE_PROFILE)
            m.set_editor_property("subsurface_profile", sp)
        else:  # older Python API: plain subsurface with a warm scatter colour
            m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SUBSURFACE)
            sss = _e(m, E.MaterialExpressionConstant3Vector, -500, 650, constant=color(0.8, 0.25, 0.15))
            MEL.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    elif kind == "Cloth":
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_CLOTH)
        sheen = _e(m, E.MaterialExpressionScalarParameter, -500, 650, parameter_name="Sheen", default_value=0.7)
        fuzz = _e(m, E.MaterialExpressionMultiply, -350, 650, const_b=1.2); _link(base, fuzz, "A")
        MEL.connect_material_property(fuzz, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)   # Fuzz Color
        MEL.connect_material_property(sheen, "", unreal.MaterialProperty.MP_CUSTOM_DATA0)      # Cloth amount
    m.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(m)
    save(m)
    return m


def run():
    import aerondel_safe as safe
    with safe.SafeRun("Realism materials", 4) as task:
        masters = {}
        for k in ("Skin", "Cloth", "Surface"):
            task.frame("Master " + k)
            masters[k] = _master(k)
        task.frame("Surface instances")
        for kind, (mk, r, me, sp, sh, ds, sc) in KINDS.items():
            path = DIR + "/MI_Real_" + kind
            if eal.does_asset_exist(path):
                eal.delete_asset(path)
            mi = asset_tools.create_asset("MI_Real_" + kind, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property("parent", masters[mk])
            for name, v in (("Roughness", r), ("Metallic", me), ("Specular", sp), ("DetailStrength", ds), ("DetailScale", sc), ("Sheen", sh)):
                MEL.set_material_instance_scalar_parameter_value(mi, name, v)
            MEL.update_material_instance(mi)
            save(mi)
    unreal.log("[Aerondel] Realism materials built in %s (%d surface kinds). Used on High/Ultra only." % (DIR, len(KINDS)))


if __name__ == "__main__":
    run()
