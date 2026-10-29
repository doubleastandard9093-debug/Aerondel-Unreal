"""
Gielinor > Build World Blockout
Greybox of the whole web-build world from Content/Data/world.json (same file the JSON importer reads):
ground, lake/ponds, paths, every town building (wall + roof colours from the data), guilds with their
stations and bank chest, landmark markers, the cave, an AQuestPlaceVolume per town/landmark/guild
(PlaceId = its name) plus one per quest place in world.json "places" (PlaceId = quest place id), NavMeshBoundsVolume, lighting and a PlayerStart at world.spawn.
Web units are metres with x east, z south-ish; mapped the same way as build_starter_town (V()).
Saved to /Game/Gielinor/Maps/World. Data-driven: edit world.json and re-run.
"""
import json
import math
import os
import unreal
from gielinor_util import ROOT, mat, eal
import build_starter_town as bst
from build_starter_town import V, box, label, actors, levels, CUBE, CYL, CONE

MAP = ROOT + "/Maps/World"
FEATURE_NPCS = [("Brannoch", 38, 88), ("Ysmay", 42, 92), ("Warcaptain Hesk", -1000, -620),
                ("Shipwright Halvard", -80, 76), ("Tide Dealer", 78, 1028), ("Anchor Dealer", 82, 1028)]
FEATURE_SITES = [("wellmere", 40, 90, 12), ("siege_fort", -1000, -640, 16), ("wilds", 122, -122, 20),
                 ("port_sarim_dock", -80, 76, 8), ("brineholt", 80, 1030, 14)]
STATION = {"range": "RANGE", "furnace": "FURNACE", "anvil": "ANVIL", "fire": "FIRE"}


def load_world():
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "world.json")
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def hex_rgb(c):
    return (((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0)


def hex_mat(c):
    return mat("Hex%06X" % c, hex_rgb(c))


def place_volume(place_id, x, z, size=12.0, display=None):
    v = actors.spawn_actor_from_class(unreal.QuestPlaceVolume, V(x, 2, z))
    v.set_editor_property("place_id", unreal.Name(place_id))
    if display:
        v.set_editor_property("display_name", display)
    v.set_actor_scale3d(unreal.Vector(size / 8.0, size / 8.0, 1.0))  # default box extent is 8 m wide
    return label(v, "Place_" + place_id, "Places")


def house(name, x, z, w, d, wall, roof, folder):
    h = 3.5
    box(name + "_Walls", folder, (x, h / 2, z), (w, h, d), hex_mat(wall))
    box(name + "_Roof", folder, (x, h + 0.9, z), (w * 0.85, 1.8, d * 0.85), hex_mat(roof), shape=CONE)


def run():
    w = load_world()
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    if not levels.new_level(MAP):
        unreal.log_error("[Gielinor] Could not create " + MAP)
        return
    half = float(w.get("worldHalf", 300))
    with unreal.ScopedSlowTask(8, "Gielinor: world blockout") as task:
        task.make_dialog(True)

        task.enter_progress_frame(1, "Ground and lighting")
        bst.environment()
        for a in actors.get_all_level_actors():  # replace the starter-town ground/fences with a world-sized one
            if a.get_folder_path() == "Environment" and a.get_actor_label() in ("Ground", "Town square", "Fence N", "Fence S", "Fence W", "NavMeshBounds", "PlayerStart"):
                actors.destroy_actor(a)
        box("Ground", "Environment", (0, -0.05, 0), (half * 2, 0.1, half * 2), mat("Grass", (0.3, 0.45, 0.2), 0.95))
        nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, V(0, 2, 0))
        nav.set_actor_scale3d(unreal.Vector(half, half, 10))
        label(nav, "NavMeshBounds", "Environment")
        sp = w.get("spawn", {"x": 0, "z": 0})
        label(actors.spawn_actor_from_class(unreal.PlayerStart, V(sp["x"], 1.0, sp["z"])), "PlayerStart", "Environment")

        task.enter_progress_frame(1, "Water")
        water = mat("Water", (0.15, 0.35, 0.55), 0.1)
        lk = w.get("lake")
        if lk:
            box("Lake", "Water", (lk["x"], 0.02, lk["z"]), (lk["r"] * 2, 0.04, lk["r"] * 2), water, shape=CYL, collide=False)
        for i, p in enumerate(w.get("ponds", [])):
            box("Pond_%d" % i, "Water", (p["x"], 0.02, p["z"]), (p["r"] * 2, 0.04, p["r"] * 2), water, shape=CYL, collide=False)

        task.enter_progress_frame(1, "Paths")
        dirt = mat("Path", (0.55, 0.45, 0.3), 0.9)
        for i, (x1, z1, x2, z2) in enumerate(w.get("paths", [])):
            length = math.hypot(x2 - x1, z2 - z1)
            yaw = math.degrees(math.atan2(z2 - z1, x2 - x1))
            box("Path_%02d" % i, "Paths", ((x1 + x2) / 2, 0.015, (z1 + z2) / 2), (length + 2, 0.03, 2.4), dirt, yaw=yaw, collide=False)

        task.enter_progress_frame(1, "Buildings")
        for i, b in enumerate(w.get("buildings", [])):
            x, z, bw, bd, wall, roof, name = b[0], b[1], b[2], b[3], b[4], b[5], b[6]
            house(name.replace(" ", "_"), x, z, bw, bd, wall, roof, "Buildings/" + name)

        task.enter_progress_frame(1, "Guilds")
        for g in w.get("guilds", []):
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

        task.enter_progress_frame(1, "Landmarks and towns")
        stone = mat("StoneDark", (0.45, 0.42, 0.4))
        for lm in w.get("landmarks", []):
            box("Landmark_" + lm["n"], "Landmarks", (lm["x"], 2, lm["z"]), (1.2, 4, 1.2), stone)
            place_volume(lm["n"], lm["x"], lm["z"], 16)
        for town, t in w.get("towns", {}).items():
            place_volume(town, t["x"], t["z"], 40)
        # Quest visit targets (quests.json "place" ids -> world.json "places", exported from the web PLACES table).
        for pid, p in w.get("places", {}).items():
            place_volume(pid, p["x"], p["z"], p["r"] * 2, p.get("name"))
        cave = w.get("cave")
        if cave:
            e = cave["entrance"]
            box("Cave entrance", "Cave", (e["x"], 2, e["z"]), (6, 4, 2), stone)
            box("Cave floor", "Cave", (cave["x"], -0.04, cave["z"]), (cave["r"] * 2, 0.1, cave["r"] * 2), stone, shape=CYL)
            place_volume("Cave", cave["x"], cave["z"], cave["r"] * 2)

        # Feature sites from the v18 port (C++ features own the logic; these are greybox anchors + place volumes).
        # Their NPCs (Brannoch, Ysmay, Warcaptain Hesk, Shipwright Halvard, card dealers) have no NpcDef yet, so
        # they are tinted marker posts tagged "Interactable" for the pad context menu; swap for NpcTalk once defined.
        post = mat("FeatureNpc", (0.75, 0.55, 0.3))
        for name, x, z in FEATURE_NPCS:
            a = box("Npc_" + name.replace(" ", "_"), "FeatureSites/NPCs", (x, 0.9, z), (0.6, 1.8, 0.6), post, shape=CYL)
            a.set_editor_property("tags", [unreal.Name("Interactable")])
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
        felt = mat("CardFelt", (0.1, 0.4, 0.2))
        for i in range(2):
            box("Card_Table_%d" % i, "FeatureSites/Brineholt", (80 + i * 4 - 2, 0.45, 1030), (2, 0.9, 1.2), felt)
        box("Port_Sarim_Dock", "FeatureSites/Docks", (-80, 0.2, 80), (6, 0.4, 14), mat("Planks", (0.45, 0.32, 0.2)))

        task.enter_progress_frame(1, "Save")
        levels.save_current_level()
    unreal.log("[Gielinor] World blockout saved to " + MAP)
