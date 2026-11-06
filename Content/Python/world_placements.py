"""
Gameplay placements for the World map: trees, rocks, fishing spots, herb patches and monsters exported from the web
build by Tools/export_webgpu_data.mjs into Content/Data/placements.json, plus shore blockers so the lake and ponds
can't be walked on. Called by build_world_blockout.run().

Actors carry a DefinitionId (e.g. "tree_oak", "fish_net", "giant_spider") that AResourceNode / AAerondelMob resolve
from the game database at BeginPlay, because the JSON defs only exist at runtime. The pure planning functions
(load/plan/shore_blockers) have no editor dependency and are covered by Tools/test_world_placements.py.
"""
import json
import math
import os

try:
    import unreal  # noqa: F401  (absent in the offline tests)
except ImportError:  # pragma: no cover
    unreal = None

# Look per resource type: (shape, scale (x, y, z), rel z cm, colour, range cm, label folder)
NODE_LOOK = {
    "tree": ("CONE", (2.6, 2.6, 4.5), 225, (0.25, 0.5, 0.2), 220.0, "Resources/Trees"),
    "rock": ("SPHERE", (1.4, 1.2, 0.9), 45, (0.5, 0.45, 0.4), 200.0, "Resources/Rocks"),
    "herb": ("CYL", (0.8, 0.8, 0.3), 15, (0.3, 0.6, 0.25), 200.0, "Resources/Herbs"),
    "fish": ("CYL", (1.2, 1.2, 0.05), 3, (0.5, 0.75, 1.0), 600.0, "Resources/Fishing"),
}
SHORE_HEIGHT_M = 3.0      # tall enough that nobody steps onto the water; fishing is cast from the shore
SHORE_SHRINK_M = 0.4      # blocker sits just inside the water edge so the shoreline stays walkable
FISH_RANGE_CM = 600.0     # web spots sit up to ~4 m in from the edge; the player casts from the bank


def load(content_dir):
    with open(os.path.join(content_dir, "Data", "placements.json"), encoding="utf-8") as f:
        return json.load(f)


def plan(placements, small=False, limit=10):
    """-> (nodes, mobs) lists; small mode keeps only the first `limit` of each kind so the test map stays a test."""
    nodes, mobs = list(placements.get("nodes", [])), list(placements.get("mobs", []))
    if small:
        def cap(seq, key):
            seen, out = {}, []
            for e in seq:
                k = e[key]
                if seen.get(k, 0) < limit:
                    seen[k] = seen.get(k, 0) + 1
                    out.append(e)
            return out
        nodes, mobs = cap(nodes, "type"), cap(mobs, "def")
    return nodes, mobs


def shore_blockers(world):
    """One invisible pawn-blocking cylinder per lake/pond: (name, x, z, diameter_m, height_m)."""
    out = []
    lk = world.get("lake")
    if lk:
        out.append(("Lake shore", lk["x"], lk["z"], max(0.5, (lk["r"] - SHORE_SHRINK_M) * 2), SHORE_HEIGHT_M))
    for i, p in enumerate(world.get("ponds", [])):
        out.append(("Pond_%d shore" % i, p["x"], p["z"], max(0.5, (p["r"] - SHORE_SHRINK_M) * 2), SHORE_HEIGHT_M))
    return out


def wander_cm(roam_m):
    return max(300.0, float(roam_m or 2) * 100.0)


# ---------------------------------------------------------------- editor side
def build(world, small=False):
    import build_starter_town as bst
    from aerondel_util import mat
    data = load(unreal.Paths.project_content_dir())
    nodes, mobs = plan(data, small)
    shapes = {"CONE": bst.CONE, "SPHERE": bst.SPHERE, "CYL": bst.CYL}

    for name, x, z, dia, h in shore_blockers(world):
        a = bst.spawn(unreal.StaticMeshActor, bst.V(x, h / 2, z))
        c = a.static_mesh_component
        c.set_static_mesh(bst.mesh(bst.CYL, False))
        c.set_world_scale3d(unreal.Vector(dia, dia, h))
        c.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
        c.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
        c.set_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN, unreal.CollisionResponseType.ECR_BLOCK)
        c.set_editor_property("can_ever_affect_navigation", True)  # carves the navmesh: click-to-move never paths across water
        a.set_actor_hidden_in_game(True)
        bst.label(a, name, "Water")

    made_nodes = 0
    for n in nodes:
        look = NODE_LOOK.get(n["type"])
        if not look:
            continue
        shape, scale, relz, rgb, rng, folder = look
        a = bst.spawn(unreal.ResourceNode, bst.V(n["x"], 0.05 if n["type"] == "fish" else 0, n["z"]))
        a.set_editor_property("definition_id", n["def"])
        m = a.get_editor_property("mesh")
        bst.set_mesh(m, shapes[shape], scale, mat("Node_" + n["type"], rgb, 0.3 if n["type"] == "fish" else 0.8), rel=(0, 0, relz))
        if n["type"] == "fish":
            m.set_collision_profile_name("OverlapAllDynamic")  # never blocks walking; still clickable
            m.set_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY, unreal.CollisionResponseType.ECR_BLOCK)
            m.set_editor_property("can_ever_affect_navigation", False)
        a.set_editor_property("range", FISH_RANGE_CM if n["type"] == "fish" else rng)
        bst.label(a, "%s (%s)" % ("Fishing spot" if n["type"] == "fish" else n["kind"], n["def"]), folder)
        made_nodes += 1

    for mb in mobs:
        a = bst.spawn(unreal.AerondelMob, bst.V(mb["x"], 1.0, mb["z"]))
        a.set_editor_property("definition_id", mb["def"])
        a.set_editor_property("wander_radius", wander_cm(mb.get("roam")))
        bst.label(a, mb["type"], "Mobs")
    unreal.log("[Aerondel] World placements: %d resource nodes, %d mobs, %d shore blockers%s"
               % (made_nodes, len(mobs), len(shore_blockers(world)), " (SMALL TEST)" if small else ""))
    return made_nodes, len(mobs)
