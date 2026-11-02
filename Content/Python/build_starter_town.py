"""
Aerondel > Build Starter Town
Greybox of Larkhaven from basic shapes (same layout as the Unity StarterTownBuilder, metres -> cm):
bank, smithy (furnace + anvil) and general store (range) with interiors, solid walls and door-only gaps
with hinged doors; trees, oaks, copper/tin/iron rocks, a pond with a fishing spot, NPCs, cows, goblins,
NavMeshBoundsVolume, sun/sky/skylight/fog, PlayerStart. Saved to /Game/Gielinor/Maps/StarterTown.
Run "Generate Starter Data" first.
"""
import unreal
from aerondel_util import ROOT, mat, eal
import aerondel_safe as safe

MAP = ROOT + "/Maps/StarterTown"
M = 100.0           # metres -> cm
WALL_T = 0.3        # wall thickness (m)
DOOR_W, DOOR_H = 1.4, 2.4

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

CUBE = "/Engine/BasicShapes/Cube.Cube"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CONE = "/Engine/BasicShapes/Cone.Cone"
PLANE = "/Engine/BasicShapes/Plane.Plane"
_meshes = {}


BEVEL_MAX = 8.0     # bevelled cube only up to 8 m (scaled bevel stays a few cm, never a rounded slab)


def mesh(path, smooth=True):
    """Engine BasicShape, or its rounder /Game/Gielinor/Meshes/Smooth twin when build_smooth_shapes has run."""
    key = (path, smooth)
    if key not in _meshes:
        sp = None
        if smooth:
            try:
                import build_smooth_shapes
                sp = build_smooth_shapes.smooth_path(path)
            except Exception:
                sp = None
        _meshes[key] = unreal.load_asset(sp or path)
    return _meshes[key]


def _smooth_ok(shape, dims):
    return shape != CUBE or max(abs(v) for v in dims) <= BEVEL_MAX


def V(x, y, z):
    """Unity-style (x right, y up, z forward) metres -> UE (X, Y, Z) cm."""
    return unreal.Vector(x * M, z * M, y * M)


def data(kind, name):
    return eal.load_asset(f"{ROOT}/Data/{kind}/{name}")


def label(actor, text, folder):
    actor.set_actor_label(text)
    actor.set_folder_path(folder)
    return actor


def spawn(cls, loc, rot=None):
    """spawn_actor_from_class + low-GPU breather every few spawns (aerondel_safe)."""
    safe.tick()
    return actors.spawn_actor_from_class(cls, loc, rot or unreal.Rotator(0, 0, 0))


def box(name, folder, center, size, material, shape=CUBE, yaw=0.0, collide=True, decor=False):
    """center/size in metres (Unity axes). Basic shapes are 100 cm, centred (cylinder/sphere/cone too).
    decor=True: optional dressing, skipped (returns None) once its 128 m cell hits aerondel_safe.CELL_CAP."""
    if decor and not safe.allow(center[0], center[2]):
        return None
    a = spawn(unreal.StaticMeshActor, V(*center), unreal.Rotator(0, 0, yaw))
    c = a.static_mesh_component
    c.set_static_mesh(mesh(shape, _smooth_ok(shape, size)))
    c.set_world_scale3d(unreal.Vector(size[0], size[2], size[1]))
    c.set_material(0, material)
    if not collide:
        c.set_collision_profile_name("NoCollision")
    return label(a, name, folder)


_ism = {}


def instanced(name, folder, shape, material, transforms_m, collide=False):
    """Repeated pieces as ONE hierarchical-instanced actor (AAerondelPropInstances): transforms_m = [(center, size, yaw)].
    Falls back to individual decor boxes if the C++ class is missing (old build)."""
    cls = getattr(unreal, "AerondelPropInstances", None)
    if cls is None:
        return [box("%s_%02d" % (name, i), folder, c, s, material, shape=shape, yaw=y, collide=collide, decor=True)
                for i, (c, s, y) in enumerate(transforms_m)]
    kept = [(c, s, y) for c, s, y in transforms_m if safe.allow(c[0], c[2])]
    if not kept:
        return None
    cx = sum(c[0] for c, _, _ in kept) / len(kept)
    cz = sum(c[2] for c, _, _ in kept) / len(kept)
    a = spawn(cls, V(cx, 0, cz))
    a.setup(mesh(shape, True), material, collide)
    xf = [unreal.Transform(V(*c), unreal.Rotator(0, 0, y), unreal.Vector(s[0], s[2], s[1])) for c, s, y in kept]
    a.add_instances(xf)
    return label(a, name, folder)


def set_mesh(comp, shape, scale, material, rel=(0, 0, 0)):
    comp.set_static_mesh(mesh(shape, _smooth_ok(shape, scale)))
    comp.set_relative_scale3d(unreal.Vector(*scale))
    comp.set_relative_location(unreal.Vector(*rel), False, False)
    comp.set_material(0, material)


# ---------------------------------------------------------------- buildings
def building(name, cx, cz, w, d, h, wall_mat):
    """Door gap centred in the south (-z) wall. Returns a helper to place things in local metres."""
    f = f"Town/{name}"
    floor = mat("Planks", (0.42, 0.3, 0.18))
    roof = mat("Roof", (0.45, 0.2, 0.15))
    box(f"{name}_Floor", f, (cx, 0.05, cz), (w, 0.1, d), floor)
    box(f"{name}_Wall_Back", f, (cx, h / 2, cz + d / 2 - WALL_T / 2), (w, h, WALL_T), wall_mat)
    box(f"{name}_Wall_Left", f, (cx - w / 2 + WALL_T / 2, h / 2, cz), (WALL_T, h, d), wall_mat)
    box(f"{name}_Wall_Right", f, (cx + w / 2 - WALL_T / 2, h / 2, cz), (WALL_T, h, d), wall_mat)
    fz = cz - d / 2 + WALL_T / 2
    side = (w - DOOR_W) / 2
    # The two front pieces overlap the side walls fully, so the only opening is the doorway.
    box(f"{name}_Wall_Front_L", f, (cx - w / 2 + side / 2, h / 2, fz), (side, h, WALL_T), wall_mat)
    box(f"{name}_Wall_Front_R", f, (cx + w / 2 - side / 2, h / 2, fz), (side, h, WALL_T), wall_mat)
    box(f"{name}_Lintel", f, (cx, DOOR_H + (h - DOOR_H) / 2, fz), (DOOR_W, h - DOOR_H, WALL_T), wall_mat)
    box(f"{name}_Roof", f, (cx, h + 0.15, cz), (w + 0.8, 0.3, d + 0.8), roof)
    box(f"{name}_Ridge", f, (cx, h + 0.8, cz), (w * 0.6, 1.0, d * 0.5), roof)

    # Hinged door: actor origin = hinge at the left jamb, leaf offset +X by half its width.
    door = spawn(unreal.Door, V(cx - DOOR_W / 2, 0, fz), unreal.Rotator(0, 0, 0))
    leaf = door.get_editor_property("mesh")
    set_mesh(leaf, CUBE, ((DOOR_W - 0.04), 0.08, (DOOR_H - 0.02)), mat("Door", (0.4, 0.25, 0.12)),
             rel=(DOOR_W / 2 * M, 0, DOOR_H / 2 * M))
    leaf.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
    leaf.set_editor_property("can_ever_affect_navigation", False)  # NavMesh paths through doorways; the leaf still blocks bodies
    door.set_editor_property("display_name", "Door")
    label(door, f"{name}_Door", f)

    lamp = spawn(unreal.PointLight, V(cx, h - 0.6, cz), unreal.Rotator(0, 0, 0))
    lc = lamp.get_component_by_class(unreal.PointLightComponent)
    lc.set_editor_property("intensity", 3000.0)
    lc.set_editor_property("light_color", unreal.Color(255, 214, 160, 255))
    lc.set_editor_property("attenuation_radius", max(w, d) * M)
    lc.set_editor_property("cast_shadows", False)  # cheap interiors (Quest budget)
    label(lamp, f"{name}_Lamp", f)
    return lambda lx, ly, lz: (cx + lx, ly, cz + lz)


def interactable(cls, name, folder, pos, shape, size, material, yaw=0.0):
    a = spawn(cls, V(*pos), unreal.Rotator(0, 0, yaw))
    set_mesh(a.get_editor_property("mesh"), shape, (size[0], size[2], size[1]), material)
    a.set_editor_property("display_name", name)
    return label(a, name, folder)


def station(name, folder, pos, size, material, stype):
    a = interactable(unreal.Station, name, folder, pos, CUBE, size, material)
    a.set_editor_property("station_type", stype)
    return a


# ---------------------------------------------------------------- resources
def tree(node_def, x, z, s=1.0, oak=False):
    a = spawn(unreal.ResourceNode, V(x, 0, z), unreal.Rotator(0, 0, 0))
    a.set_editor_property("definition", node_def)
    crown = mat("Oak" if oak else "Leaves", (0.3, 0.45, 0.15) if oak else (0.25, 0.5, 0.2))
    # Alive: a cone "tree" 4.5 m tall (pivot at centre, so lift by half).
    set_mesh(a.get_editor_property("mesh"), CONE, (2.6 * s, 2.6 * s, 4.5 * s), crown, rel=(0, 0, 225 * s))
    set_mesh(a.get_editor_property("depleted_mesh"), CYL, (0.5 * s, 0.5 * s, 0.4 * s), mat("Bark", (0.35, 0.24, 0.14)), rel=(0, 0, 20 * s))
    a.set_editor_property("range", 220.0)
    return label(a, "Oak tree" if oak else "Tree", "Resources/Trees")


def rock(node_def, x, z, rgb, name):
    a = spawn(unreal.ResourceNode, V(x, 0, z), unreal.Rotator(0, 0, 0))
    a.set_editor_property("definition", node_def)
    set_mesh(a.get_editor_property("mesh"), SPHERE, (1.4, 1.2, 0.9), mat(f"Ore_{name}", rgb, 0.6), rel=(0, 0, 45))
    set_mesh(a.get_editor_property("depleted_mesh"), SPHERE, (1.3, 1.1, 0.8), mat("RockEmpty", (0.4, 0.38, 0.36)), rel=(0, 0, 40))
    a.set_editor_property("range", 200.0)
    return label(a, name, "Resources/Rocks")


def pond(node_def, x, z):
    water = box("Pond", "Resources/Pond", (x, 0.02, z), (14, 0.04, 10), mat("Water", (0.15, 0.35, 0.6), 0.05), shape=CYL, collide=False)
    water.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
    for i, (ox, oz) in enumerate(((-4, 3.4), (3, 3.6))):  # near the north (town-side) shore
        a = spawn(unreal.ResourceNode, V(x + ox, 0.05, z + oz), unreal.Rotator(0, 0, 0))
        a.set_editor_property("definition", node_def)
        set_mesh(a.get_editor_property("mesh"), CYL, (1.2, 1.2, 0.05), mat("FishSpot", (0.5, 0.75, 1.0), 0.1))
        a.get_editor_property("mesh").set_collision_profile_name("OverlapAllDynamic")
        a.get_editor_property("mesh").set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
        a.set_editor_property("range", 450.0)  # cast from the shore
        label(a, f"Fishing spot {i + 1}", "Resources/Pond")


# ---------------------------------------------------------------- characters
def npc(npc_id, x, z, yaw=0.0):
    d = data("Npcs", f"Npc_{npc_id}")
    a = spawn(unreal.NpcTalk, V(x, 0, z), unreal.Rotator(0, 0, yaw))
    a.set_editor_property("definition", d)
    tint = d.get_editor_property("tint")
    set_mesh(a.get_editor_property("mesh"), CYL, (0.55, 0.55, 1.75), mat(f"Npc_{npc_id}", (tint.r, tint.g, tint.b)), rel=(0, 0, 87.5))
    a.set_editor_property("range", 220.0)
    return label(a, str(d.get_editor_property("display_name")), "NPCs")


def mob(mob_def, x, z, name):
    a = spawn(unreal.AerondelMob, V(x, 1.0, z), unreal.Rotator(0, 0, 0))
    a.set_editor_property("definition", mob_def)
    a.rerun_construction_scripts()
    return label(a, name, "Mobs")


# ---------------------------------------------------------------- environment
def environment():
    f = "Environment"
    g = box("Ground", f, (0, -0.05, 0), (140, 0.1, 140), mat("Grass", (0.3, 0.45, 0.2), 0.95))
    box("Town square", f, (0, 0.01, 4), (16, 0.02, 14), mat("Cobble", (0.55, 0.53, 0.5), 0.85))
    box("Fence N", f, (-16, 0.5, -18), (14, 1, 0.15), mat("Wood", (0.5, 0.35, 0.2)))
    box("Fence S", f, (-16, 0.5, -30), (14, 1, 0.15), mat("Wood", (0.5, 0.35, 0.2)))
    box("Fence W", f, (-23, 0.5, -24), (0.15, 1, 12), mat("Wood", (0.5, 0.35, 0.2)))

    sun = spawn(unreal.DirectionalLight, V(0, 30, 0), unreal.Rotator(0, -45, 35))
    sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
    sc.set_editor_property("intensity", 8.0)
    sc.set_editor_property("atmosphere_sun_light", True)
    sc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sc.set_editor_property("dynamic_shadow_distance_movable_light", 6000.0)  # one CSM-friendly distance for Quest
    label(sun, "Sun", f)
    label(spawn(unreal.SkyAtmosphere, V(0, 0, 0)), "SkyAtmosphere", f)
    sky = spawn(unreal.SkyLight, V(0, 5, 0))
    skc = sky.get_component_by_class(unreal.SkyLightComponent)
    skc.set_editor_property("real_time_capture", True)
    skc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    label(sky, "SkyLight", f)
    fog = spawn(unreal.ExponentialHeightFog, V(0, 0, 0))
    fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.01)
    label(fog, "HeightFog", f)
    pp = spawn(unreal.PostProcessVolume, V(0, 0, 0))
    pp.set_editor_property("unbound", True)
    label(pp, "PostProcess (unbound)", f)

    nav = spawn(unreal.NavMeshBoundsVolume, V(0, 2, 0))
    nav.set_actor_scale3d(unreal.Vector(75, 75, 10))  # default brush is 200 cm -> 150 x 150 x 20 m
    label(nav, "NavMeshBounds", f)

    start = spawn(unreal.PlayerStart, V(0, 1.0, -4), unreal.Rotator(0, 0, 90))
    label(start, "PlayerStart", f)


def run(small=False):
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    if not levels.new_level(MAP):
        unreal.log_error("[Aerondel] Could not create " + MAP)
        return

    with safe.SafeRun("Building Larkhaven", 6, small=small, save_fn=levels.save_current_level) as task:
        task.frame("Environment")
        environment()

        task.frame("Buildings")
        bank = building("Bank", -14, 10, 10, 8, 4, mat("StoneLight", (0.78, 0.75, 0.68)))
        smithy = building("Smithy", 14, 10, 10, 8, 4, mat("StoneDark", (0.45, 0.42, 0.4)))
        store = building("GeneralStore", 0, 18, 10, 8, 4, mat("Plaster", (0.85, 0.8, 0.65)))

        task.frame("Interiors")
        wood_dark = mat("WoodDark", (0.35, 0.22, 0.12))
        box("Bank counter", "Town/Bank", bank(0, 0.5, 2), (6, 1, 0.8), wood_dark)
        chest = interactable(unreal.BankChest, "Bank chest", "Town/Bank", bank(-3.5, 0.45, 3), CUBE, (1.2, 0.9, 0.8), mat("Chest", (0.5, 0.3, 0.15)))
        booth = interactable(unreal.BankChest, "Bank booth", "Town/Bank", bank(2.5, 0.45, 3), CUBE, (1.2, 0.9, 0.8), mat("Chest", (0.5, 0.3, 0.15)))
        station("Furnace", "Town/Smithy", smithy(-3, 1.0, 2.5), (2, 2, 1.6), mat("Brick", (0.55, 0.25, 0.18)), unreal.StationType.FURNACE)
        glow = spawn(unreal.PointLight, V(*smithy(-3, 1.0, 1.5)))
        gc = glow.get_component_by_class(unreal.PointLightComponent)
        gc.set_editor_property("light_color", unreal.Color(255, 120, 40, 255))
        gc.set_editor_property("intensity", 2500.0)
        gc.set_editor_property("cast_shadows", False)
        label(glow, "Furnace glow", "Town/Smithy")
        station("Anvil", "Town/Smithy", smithy(2, 0.4, 1), (1.2, 0.8, 0.5), mat("Iron", (0.25, 0.25, 0.28), 0.4, 0.9), unreal.StationType.ANVIL)
        station("Cooking range", "Town/GeneralStore", store(-3.5, 0.5, 2.8), (1.6, 1, 1), mat("IronBlack", (0.12, 0.12, 0.12), 0.5, 0.6), unreal.StationType.RANGE)
        box("Shop counter", "Town/GeneralStore", store(1.5, 0.5, 1), (4, 1, 0.8), wood_dark)
        for i in range(3):
            box(f"Shelf {i + 1}", "Town/GeneralStore", store(-1 + i * 2.2, 1.2, 3.6), (1.8, 2.4, 0.4), mat("Wood", (0.5, 0.35, 0.2)))

        task.frame("Resources")
        t, oak = data("Resources", "Node_tree"), data("Resources", "Node_oak_tree")
        for x, z in ((-30, -10), (-34, -4), (-27, -2), (-36, -14), (-31, 4)):
            tree(t, x, z)
        tree(oak, -40, 8, 1.4, oak=True)
        tree(oak, -42, -6, 1.4, oak=True)
        cu, sn, fe = data("Resources", "Node_copper_rock"), data("Resources", "Node_tin_rock"), data("Resources", "Node_iron_rock")
        rock(cu, 30, -12, (0.75, 0.45, 0.3), "Copper rocks")
        rock(cu, 32, -9, (0.75, 0.45, 0.3), "Copper rocks")
        rock(sn, 35, -13, (0.7, 0.7, 0.68), "Tin rocks")
        rock(sn, 33, -16, (0.7, 0.7, 0.68), "Tin rocks")
        rock(fe, 38, -10, (0.5, 0.3, 0.25), "Iron rocks")
        pond(data("Resources", "Node_shrimp_spot"), 0, -30)

        task.frame("NPCs and mobs")
        npc("hilda", -8, -2, 90)
        npc("osric", *bank(0, 0, 3.2)[::2], yaw=-90)
        npc("doric", *smithy(3, 0, 3)[::2], yaw=-90)
        cow, gob = data("Mobs", "Mob_cow"), data("Mobs", "Mob_goblin")
        for i in range(4):
            mob(cow, -20 + i * 2.5, -24 + (i % 2) * 3, f"Cow {i + 1}")
        for i, (x, z) in enumerate(((28, 20), (31, 23), (26, 25))):
            mob(gob, x, z, f"Goblin {i + 1}")

        task.frame("World settings, NavMesh, save")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        ws = world.get_world_settings()
        ws.set_editor_property("default_game_mode", unreal.AerondelGameMode)
        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        levels.save_current_level()

    unreal.log(f"[Aerondel] Built {MAP}. Press Play (desktop) or VR Preview (headset).")


if __name__ == "__main__":
    run()
