"""Aerondel > Build Smooth Shapes
Rounder replacements for the engine BasicShapes used by the greybox (Low-friendly: plain static meshes, no Nanite,
smooth vertex normals so curved surfaces shade round instead of faceted). Same 100 cm size and centred pivot as
/Engine/BasicShapes, so the town/world builders can swap them in 1:1. Written to /Game/Gielinor/Meshes/Smooth.
Needs the GeometryScripting plugin (editor only). If anything fails the builders keep using the engine shapes.

Triangle budget (per mesh): cylinder 48 sides ~ 0.4k, sphere 32x20 ~ 1.2k, cone 48 sides ~ 0.2k,
bevelled cube ~ 0.3k. Each mesh also gets 2 auto-reduced LODs so far objects stay cheap on an iGPU.
"""
import unreal
from aerondel_util import ROOT, eal

FOLDER = ROOT + "/Meshes/Smooth"
SHAPES = {
    "/Engine/BasicShapes/Cylinder.Cylinder": FOLDER + "/SM_SmoothCylinder",
    "/Engine/BasicShapes/Sphere.Sphere": FOLDER + "/SM_SmoothSphere",
    "/Engine/BasicShapes/Cone.Cone": FOLDER + "/SM_SmoothCone",
    "/Engine/BasicShapes/Cube.Cube": FOLDER + "/SM_BevelCube",
}


def smooth_path(engine_path):
    """Smooth replacement for an engine BasicShape if it has been built, else None."""
    p = SHAPES.get(engine_path)
    return p if p and eal.does_asset_exist(p) else None


def _build(name, append):
    G = unreal
    dm = G.DynamicMesh()
    opts = G.GeometryScriptPrimitiveOptions()
    xf = G.Transform()
    append(dm, opts, xf)
    # Smooth (angle-threshold) normals: curved sides shade round, caps/hard edges stay crisp.
    try:
        nopts = G.GeometryScriptCalculateNormalsOptions()
        G.GeometryScript_Normals.compute_split_normals(dm, G.GeometryScriptSplitNormalsOptions(split_by_opening_angle=True, opening_angle_deg=40.0), nopts)
    except Exception as e:  # older API names: keep the primitive's own normals
        unreal.log_warning(f"[Aerondel] {name}: split normals skipped ({e})")
    path = FOLDER + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    copts = G.GeometryScriptCreateNewStaticMeshAssetOptions()
    copts.set_editor_property("enable_recompute_normals", False)
    copts.set_editor_property("enable_recompute_tangents", True)
    copts.set_editor_property("enable_nanite", False)
    copts.set_editor_property("enable_collision", True)
    res = G.GeometryScript_AssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, copts)
    sm = res[0] if isinstance(res, (tuple, list)) else res
    if not isinstance(sm, unreal.StaticMesh):
        raise RuntimeError("asset not created")
    # Auto LODs (100% / 50% / 25% triangles) so distant props cost little on Low.
    try:
        esl = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        o = unreal.EditorScriptingMeshReductionOptions()
        o.set_editor_property("reduction_settings", [
            unreal.EditorScriptingMeshReductionSettings(1.0, 1.0),
            unreal.EditorScriptingMeshReductionSettings(0.5, 0.35),
            unreal.EditorScriptingMeshReductionSettings(0.25, 0.12)])
        esl.set_lods(sm, o)
    except Exception as e:
        unreal.log_warning(f"[Aerondel] {name}: LODs skipped ({e})")
    eal.save_loaded_asset(sm, only_if_is_dirty=False)
    return sm


def run():
    P = unreal.GeometryScript_Primitives
    C = unreal.GeometryScriptPrimitiveOriginMode.CENTER
    jobs = [
        ("SM_SmoothCylinder", lambda d, o, x: P.append_cylinder(d, o, x, 50.0, 100.0, 48, 1, True, C)),
        ("SM_SmoothSphere", lambda d, o, x: P.append_sphere_lat_long(d, o, x, 50.0, 20, 32, C)),
        ("SM_SmoothCone", lambda d, o, x: P.append_cone(d, o, x, 50.0, 0.0, 100.0, 48, 4, True, C)),
        # 3 cm bevel with 3 rounding steps; builders only use it for props/walls up to ~8 m so the bevel never balloons.
        ("SM_BevelCube", lambda d, o, x: P.append_rounded_rectangle_box(d, o, x, 100.0, 100.0, 100.0, 3.0, 0, 0, 0, 3, C)),
    ]
    ok = 0
    for name, fn in jobs:
        try:
            _build(name, fn)
            ok += 1
        except Exception as e:
            unreal.log_warning(f"[Aerondel] {name} not built, engine shape stays in use: {e}")
    unreal.log(f"[Aerondel] Smooth shapes built: {ok}/{len(jobs)} -> {FOLDER}. Re-run Build Starter Town / Build World Blockout.")
