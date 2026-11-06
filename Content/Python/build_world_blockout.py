"""
Aerondel > Build World Blockout
Greybox of the whole web-build world from Content/Data/world.json (same file the JSON importer reads):
ground, lake/ponds, paths, every town building (wall + roof colours from the data), guilds with their
stations and bank chest, landmark markers, the cave, an AQuestPlaceVolume per town/landmark/guild
(PlaceId = its name), per-town banners/signposts/bank chests/waystones, plus one per quest place in world.json "places" (PlaceId = quest place id), NavMeshBoundsVolume, lighting and a PlayerStart at world.spawn.
Web units are metres with x east, z south-ish; mapped the same way as build_starter_town (V()).
Saved to /Game/Gielinor/Maps/World as a World Partition map (see world_partition.py): everything streams in
128 m cells except PlayerStart, lighting, ground/sea and nav bounds. Data-driven: edit world.json and re-run;
new regions are just new coordinates in world.json and stream automatically.
"""
import json
import math
import os
import unreal
from aerondel_util import ROOT, mat, eal
import aerondel_safe as safe
import build_starter_town as bst
import world_partition as wpart
from build_starter_town import V, box, label, actors, levels, spawn, CUBE, CYL, CONE

MAP = ROOT + "/Maps/World"
FEATURE_SITES = [("wellmere", 40, 90, 12), ("siege_fort", -1000, -640, 16), ("wilds", 122, -122, 20),
                 ("port_sarim_dock", -80, 76, 8), ("brineholt", 80, 1030, 14),
                 ("venture_vigil", -1000, 135, 14), ("venture_breach", -1000, 205, 14), ("venture_hold", -1000, 270, 14)]
STATION = {"range": "RANGE", "furnace": "FURNACE", "anvil": "ANVIL", "fire": "FIRE"}


def load_world():
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "world.json")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def hex_rgb(c):
    return (((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0)


def _q(v, step=0x20):
    """Quantise a 0-255 channel so near-identical colours share one material instance (fewer shaders/MIs on weak GPUs)."""
    return min(255, int(round(v / step)) * step)


def hex_mat(c):
    c = (_q(c >> 16 & 255) << 16) | (_q(c >> 8 & 255) << 8) | _q(c & 255)
    return mat("Hex%06X" % c, hex_rgb(c))


def tone_mat(rgb):
    """Shared palette material for NPC bodies instead of one per NPC."""
    r, g, b = (_q(int(v * 255)) for v in rgb)
    return mat("Tone%02X%02X%02X" % (r, g, b), (r / 255.0, g / 255.0, b / 255.0))


def place_volume(place_id, x, z, size=12.0, display=None):
    v = spawn(unreal.QuestPlaceVolume, V(x, 2, z))
    v.set_editor_property("place_id", unreal.Name(place_id))
    if display:
        v.set_editor_property("display_name", display)
    v.set_actor_scale3d(unreal.Vector(size / 8.0, size / 8.0, 1.0))  # default box extent is 8 m wide
    return label(v, "Place_" + place_id, "Places")


def house(name, x, z, w, d, wall, roof, folder):
    h = 3.5
    box(name + "_Walls", folder, (x, h / 2, z), (w, h, d), hex_mat(wall))
    box(name + "_Roof", folder, (x, h + 0.9, z), (w * 0.85, 1.8, d * 0.85), hex_mat(roof), shape=CONE)


def text3d(name, folder, x, y, z, text, size=60.0, yaw=0.0, rgb=(255, 236, 180)):
    """World-space label (TextRenderActor): cheap on every graphics tier, two-sided by spawning a back-to-back pair."""
    for i, yy in enumerate((yaw, yaw + 180.0)):
        a = spawn(unreal.TextRenderActor, V(x, y, z), unreal.Rotator(0, 0, yy))
        c = a.get_editor_property("text_render")
        try:
            c.set_text(unreal.Text(text))
        except Exception:
            c.set_editor_property("text", unreal.Text(text))
        c.set_editor_property("world_size", size)
        try:
            c.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
        except Exception:
            pass  # left-aligned is fine
        c.set_editor_property("text_render_color", unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255))
        c.set_collision_profile_name("NoCollision")
        label(a, "%s_%d" % (name, i), folder)


def bearing(dx, dz):
    """Compass word for a web-space offset (web z grows south)."""
    names = ("east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east")
    return names[int(round(math.atan2(dz, dx) / (math.pi / 4))) % 8]


def build_towns(w):
    """Per town (world.json towns): gate banner with name + motto, a signpost with one arm per other town,
    a bank chest at the bank's door, and a stone plinth under the waystone (the waystone itself is an NPC, service travel)."""
    towns = w.get("towns", {})
    post = mat("SignPost", (0.35, 0.24, 0.14))
    plank = mat("SignPlank", (0.62, 0.48, 0.3))
    cloth = mat("BannerCloth", (0.45, 0.12, 0.12))
    chest = mat("Chest", (0.5, 0.3, 0.15))
    plinth = mat("WaystonePlinth", (0.5, 0.52, 0.56))
    for name, t in safe.limit(towns):
        f = "Towns/" + name
        x, z = t["x"], t["z"]
        # Gate banner on the main approach, 14 m south of the square (or north for towns on the south coast).
        gz = z + 14 if name != "Saltreach" else z - 14
        for side in (-1, 1):
            box("%s_GatePost_%d" % (name, side), f, (x + side * 3.2, 2.5, gz), (0.4, 5, 0.4), post)
        box(name + "_GateBeam", f, (x, 5.1, gz), (7.0, 0.35, 0.35), post)
        box(name + "_GateCloth", f, (x, 4.3, gz), (6.0, 1.4, 0.08), cloth, collide=False)
        text3d(name + "_Banner", f, x, 4.55, gz, t.get("banner", name), 70.0, 90.0)
        if t.get("motto"):
            text3d(name + "_Motto", f, x, 3.85, gz, t["motto"], 34.0, 90.0, (230, 230, 230))
        # Signpost in the square: one arm per other town, pointing at it, with distance.
        sx, sz = x + 2.5, z + 2.5
        box(name + "_SignPost", f, (sx, 1.6, sz), (0.25, 3.2, 0.25), post)
        for i, (other, o) in enumerate((k, v) for k, v in towns.items() if k != name):
            dx, dz = o["x"] - x, o["z"] - z
            yaw = math.degrees(math.atan2(dz, dx))
            h = 2.9 - i * 0.38
            ax, az = sx + math.cos(math.radians(yaw)) * 0.9, sz + math.sin(math.radians(yaw)) * 0.9
            box("%s_Sign_%s" % (name, other), f, (ax, h, az), (1.8, 0.3, 0.06), plank, yaw=yaw, collide=False)
            if not safe.small(): text3d("%s_SignText_%s" % (name, other), f, ax, h - 0.1, az, "%s %dm" % (other, round(math.hypot(dx, dz))), 16.0, yaw + 90.0, (40, 25, 10))
        # Town square volume (quest/visit + "You enter X" toast) already placed from towns; add bank chest + plinth.
        bank = t.get("bank")
        if bank:
            bx, bz = bank["at"]
            bst.interactable(unreal.BankChest, name + " bank chest", f, (bx + 1.2, 0.45, bz), CUBE, (1.2, 0.9, 0.8), chest)
        tp = t.get("travel")
        if tp:
            box(name + "_WaystonePlinth", f, (tp[0], 0.12, tp[1]), (2.2, 0.24, 2.2), plinth, shape=CYL)


def run(small=False):
    """small=True: quick 'small test' (first few of each category, no signpost text) to check the machine copes."""
    w = load_world()
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    if not wpart.new_partitioned_level(MAP, levels):  # World Partition: actors stream in 128 m cells
        unreal.log_error("[Aerondel] Could not create " + MAP)
        return
    half = float(w.get("worldHalf", 300))
    n_nodes = n_mobs = 0  # stays 0 if the run is cancelled -> the packager refuses this World
    with safe.SafeRun("World blockout", 10, small=small, save_fn=levels.save_current_level) as task:

        task.frame("Ground and lighting")
        bst.environment()
        for a in actors.get_all_level_actors():  # replace the starter-town ground/fences with a world-sized one
            if a.get_folder_path() == "Environment" and a.get_actor_label() in ("Ground", "Town square", "Fence N", "Fence S", "Fence W", "NavMeshBounds", "PlayerStart"):
                actors.destroy_actor(a)
        box("Ground", "Environment", (0, -0.05, 0), (half * 2, 0.1, half * 2), mat("Grass", (0.3, 0.45, 0.2), 0.95))
        nav = spawn(unreal.NavMeshBoundsVolume, V(0, 2, 0))
        nav.set_actor_scale3d(unreal.Vector(half, half, 10))
        label(nav, "NavMeshBounds", "Environment")
        sp = w.get("spawn", {"x": 0, "z": 0})
        label(spawn(unreal.PlayerStart, V(sp["x"], 1.0, sp["z"])), "PlayerStart", "Environment")

        task.frame("Water")
        water = mat("Water", (0.15, 0.35, 0.55), 0.1)
        lk = w.get("lake")
        if lk:
            a = box("Lake", "Water", (lk["x"], 0.02, lk["z"]), (lk["r"] * 2, 0.04, lk["r"] * 2), water, shape=CYL, collide=False)
            a.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
        for i, p in enumerate(w.get("ponds", [])):
            a = box("Pond_%d" % i, "Water", (p["x"], 0.02, p["z"]), (p["r"] * 2, 0.04, p["r"] * 2), water, shape=CYL, collide=False)
            a.static_mesh_component.set_editor_property("can_ever_affect_navigation", False)
        # shore blockers (invisible, pawn-only) are added with the gameplay placements below

        task.frame("Paths")
        dirt = mat("Path", (0.55, 0.45, 0.3), 0.9)
        for i, (x1, z1, x2, z2) in enumerate(safe.limit(w.get("paths", []))):
            length = math.hypot(x2 - x1, z2 - z1)
            yaw = math.degrees(math.atan2(z2 - z1, x2 - x1))
            box("Path_%02d" % i, "Paths", ((x1 + x2) / 2, 0.015, (z1 + z2) / 2), (length + 2, 0.03, 2.4), dirt, yaw=yaw, collide=False)

        task.frame("Buildings")
        for i, b in enumerate(safe.limit(w.get("buildings", []))):
            x, z, bw, bd, wall, roof, name = b[0], b[1], b[2], b[3], b[4], b[5], b[6]
            house(name.replace(" ", "_"), x, z, bw, bd, wall, roof, "Buildings/" + name)

        task.frame("Guilds")
        for g in safe.limit(w.get("guilds", [])):
            name, f = g["name"], "Guilds/" + g["name"]
            house(name.replace(" ", "_"), g["x"], g["z"], g["w"], g["d"], g["wall"], g["roof"], f)
            for j, s in enumerate(g.get("stations", [])):
                st = STATION.get(s)
                if st:
                    bst.station("%s %s %d" % (name, s, j), f, (g["x"] - g["w"] / 2 + 1.5 + j * 2, 0.5, g["z"] + g["d"] / 2 + 1.5),
                                (1.4, 1, 1), mat("Iron", (0.25, 0.25, 0.28), 0.4, 0.9), getattr(unreal.StationType, st))
            if g.get("bank"):
                bst.interactable(unreal.BankChest, name + " bank", f, (g["x"] + g["w"] / 2 - 1, 0.45, g["z"] + g["d"] / 2 + 1.5),
                                 CUBE, (1.2, 0.9, 0.8), mat("Chest", (0.5, 0.3, 0.15)))
            place_volume(name, g["x"], g["z"], max(g["w"], g["d"]) + 4)  # extra ids; quests use the "places" table

        task.frame("Landmarks and towns")
        stone = mat("StoneDark", (0.45, 0.42, 0.4))
        for lm in safe.limit(w.get("landmarks", [])):
            box("Landmark_" + lm["n"], "Landmarks", (lm["x"], 2, lm["z"]), (1.2, 4, 1.2), stone)
            place_volume(lm["n"], lm["x"], lm["z"], 16)
        for town, t in safe.limit(w.get("towns", {})):
            place_volume(town, t["x"], t["z"], 40, t.get("banner", town))  # "You arrive at Royal City of Highcrest."
        # Quest visit targets (quests.json "place" ids -> world.json "places", exported from the web PLACES table).
        for pid, p in safe.limit(w.get("places", {})):
            place_volume(pid, p["x"], p["z"], p["r"] * 2, p.get("name"))
        cave = w.get("cave")
        if cave:
            e = cave["entrance"]
            box("Cave entrance", "Cave", (e["x"], 2, e["z"]), (6, 4, 2), stone)
            box("Cave floor", "Cave", (cave["x"], -0.04, cave["z"]), (cave["r"] * 2, 0.1, cave["r"] * 2), stone, shape=CYL)
            place_volume("Cave", cave["x"], cave["z"], cave["r"] * 2)

        # Feature sites from the v18 port (C++ features own the logic; these are greybox anchors + place volumes).
        # Feature NPCs: quests.json "npcs" entries with a service and a home spot (web world.js SARIM/ISLE lists).
        quests_path = os.path.join(unreal.Paths.project_content_dir(), "Data", "quests.json")
        with open(quests_path, "r", encoding="utf-8") as f:
            npcs = json.load(f).get("npcs", {})
        counter = mat("Counter", (0.45, 0.32, 0.2))
        for nid, n in safe.limit(npcs):
            if "x" not in n:
                continue
            # JSON-only NpcDefs: the actor resolves npc_id at BeginPlay (no Npc_ asset needed).
            a = spawn(unreal.NpcTalk, V(n["x"], 0, n["z"]))
            a.set_editor_property("npc_id", unreal.Name(nid))
            a.set_editor_property("display_name", n["name"])
            t = n.get("tint", "#a08060").lstrip("#")
            rgb = tuple(int(t[i:i + 2], 16) / 255.0 for i in (0, 2, 4))
            if nid.startswith("waystone_"):  # carved standing stone instead of a person
                bst.set_mesh(a.get_editor_property("mesh"), CUBE, (0.7, 0.5, 2.2), tone_mat(rgb), rel=(0, 0, 110.0))
            else:
                bst.set_mesh(a.get_editor_property("mesh"), CYL, (0.55, 0.55, 1.75), tone_mat(rgb), rel=(0, 0, 87.5))
            a.set_editor_property("range", 220.0)
            label(a, n["name"], ("Towns/" + n["town"] + "/NPCs") if n.get("town") else "FeatureSites/NPCs")
            svc = n.get("service", "")
            shop = svc[5:] if svc.startswith("shop:") else ("hexer" if svc == "hexer" else None)
            if shop:  # trade happens at a counter beside them
                c = bst.interactable(unreal.ShopCounter, n["name"] + " stall", "FeatureSites/Shops", (n["x"] + 1.5, 0.5, n["z"]),
                                     CUBE, (1.2, 1.0, 0.7), counter)
                c.set_editor_property("shop_id", unreal.Name(shop))
        for pid, x, z, r in FEATURE_SITES:
            place_volume(pid, x, z, r * 2)
        fort = mat("FortStone", (0.4, 0.38, 0.36))
        for i in range(16):  # siege fort palisade ring, radius 16
            a = i * math.tau / 16
            box("Fort_Wall_%02d" % i, "FeatureSites/Siege", (-1000 + 16 * math.cos(a), 2, -640 + 16 * math.sin(a)), (6.4, 4, 1), fort,
                yaw=math.degrees(a) + 90)
        banner = mat("WildsBanner", (0.6, 0.1, 0.1))
        for i, (dx, dz) in enumerate(((-6, -6), (6, -6), (-6, 6), (6, 6))):
            box("Wilds_Banner_%d" % i, "FeatureSites/Wilds", (122 + dx, 2.5, -122 + dz), (0.2, 5, 1.2), banner)
        # Ventures pads (web adapt/zone.js): flat 28x28 floors with low walls at x = -1000.
        for zid, zz, fc, ec in (("Vigil", 135, (0.23, 0.2, 0.27), (0.5, 0.35, 0.55)), ("Breach", 205, (0.23, 0.23, 0.27), (0.42, 0.35, 0.54)),
                                ("Hold", 270, (0.3, 0.26, 0.2), (0.5, 0.4, 0.28))):
            fl, ed = mat("Venture%sFloor" % zid, fc), mat("Venture%sEdge" % zid, ec)
            box("Venture_%s_Floor" % zid, "FeatureSites/Ventures", (-1000, 0.05, zz), (28, 0.1, 28), fl)
            for i, (w, d, dx, dz) in enumerate(((28, 0.6, 0, -14), (28, 0.6, 0, 14), (0.6, 28, -14, 0), (0.6, 28, 14, 0))):
                box("Venture_%s_Wall_%d" % (zid, i), "FeatureSites/Ventures", (-1000 + dx, 0.8, zz + dz), (w, 1.6, d), ed)
        node = mat("BreachNode", (0.8, 0.3, 1.0))
        for i, (dx, dz) in enumerate(((-8, -6), (8, -6), (0, -11))):
            box("Venture_Breach_Node_%d" % i, "FeatureSites/Ventures", (-1000 + dx, 0.5, 205 + dz), (1.2, 1, 1.2), node)
        for k, dx, dz, c in (("Forge", -7, -6, (0.35, 0.29, 0.29)), ("Garden", 0, -8, (0.23, 0.54, 0.23)), ("Kennel", 7, -6, (0.48, 0.35, 0.23))):
            box("Hearthhold_" + k, "FeatureSites/Ventures", (-1000 + dx, 1.2, 270 + dz), (3, 2.4, 3), mat("Hold" + k, c))
        box("Hearthhold_Boat", "FeatureSites/Ventures", (-1000, 0.3, 280), (2.2, 0.6, 2.2), mat("HoldBoat", (0.42, 0.82, 1.0)), shape=CYL)
        box("Venture_Breach_Ward", "FeatureSites/Ventures", (-1000, 1.1, 205), (1.6, 2.2, 1.6), mat("BreachWard", (0.4, 0.8, 1.0)))
        felt = mat("CardFelt", (0.1, 0.4, 0.2))
        for i in range(2):
            box("Card_Table_%d" % i, "FeatureSites/Brineholt", (80 + i * 4 - 2, 0.45, 1030), (2, 0.9, 1.2), felt)
        # Saltreach Sound (z > 760): open water, the three islands with docks. Ships, cannonballs and the deck are runtime
        # (ASailingVisuals); the sea surface is visual only (no collision) - the deck carries you.
        sea = mat("SeaWater", (0.08, 0.25, 0.38), 0.05)
        box("Sarim_Sound", "Sea", (0, -0.3, 980), (900, 0.05, 460), sea, collide=False)
        sand = mat("Sand", (0.76, 0.68, 0.48), 0.95)
        for iid, ix, iz, ir, dx, dz in (("Gullrest", -70, 960, 26, -70, 932), ("Brineholt", 80, 1030, 24, 54, 1030), ("Wreckers_Reef", 10, 1120, 22, 10, 1096)):
            box("Isle_" + iid, "Sea/Islands", (ix, -0.5, iz), (ir * 2, 1.2, ir * 2), sand, shape=CYL)
            yaw = math.degrees(math.atan2(dz - iz, dx - ix))
            box("Dock_" + iid, "Sea/Islands", ((ix + dx) / 2 + (dx - ix) * 0.25, 0.15, (iz + dz) / 2 + (dz - iz) * 0.25), (12, 0.3, 3),
                mat("Planks", (0.45, 0.32, 0.2)), yaw=yaw)
            place_volume(iid.lower(), ix, iz, ir * 2)
        box("Port_Sarim_Dock", "FeatureSites/Docks", (-80, 0.2, 80), (6, 0.4, 14), mat("Planks", (0.45, 0.32, 0.2)))

        task.frame("Towns, regions and sea")
        build_towns(w)  # banners, signposts, bank chests, waystone plinths
        import build_aerondel_regions, importlib
        importlib.reload(build_aerondel_regions)
        build_aerondel_regions.build(w, density=0.4 if small else 1.0)  # climate-region ground, biome props, dig sites, Delver's Descent

        task.frame("Gameplay placements (trees, rocks, fishing, herbs, monsters) + shore blockers")
        import world_placements
        importlib.reload(world_placements)
        n_nodes, n_mobs = world_placements.build(w, small=small)

        task.frame("World Partition streaming + save")
        if wpart.is_partitioned():
            wpart.configure_runtime_grid()
            wpart.apply_streaming_flags(actors)
            wpart.dynamic_navmesh(actors)
        else:
            unreal.log_warning("[Aerondel] World map is not partitioned; it will load all at once until converted (Tools > Convert Level).")
        levels.save_current_level()
        wpart.save_all()
    write_build_marker(small, n_nodes, n_mobs)
    unreal.log("[Aerondel] World blockout saved to " + MAP + (" -- SMALL TEST ONLY, not the whole world" if small else " (FULL world)"))


def write_build_marker(small, n_nodes, n_mobs):
    """Saved/AerondelWorldBuild.json: Tools/package_win64.bat refuses to package a small-test or placement-less World."""
    path = os.path.join(unreal.Paths.project_saved_dir(), "AerondelWorldBuild.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump({"mode": "small" if small else "full", "nodes": n_nodes, "mobs": n_mobs}, f)
    unreal.log("[Aerondel] World build marker: " + path)
