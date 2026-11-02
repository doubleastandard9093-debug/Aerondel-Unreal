"""
Aerondel climate regions (web js/regiondata.js -> world.json "regions"/"biomes"): one tinted ground disc per region
(biome material; camp lodges and ponds come from world.json buildings/ponds), scattered low-poly biome props, Relicry dig sites (trenches, restoration bench,
vault arch) and the Delver's Descent. Called by build_world_blockout.run() before streaming flags are applied, so
everything here is spatially loaded by World Partition like the rest of the map. Greybox only: gameplay for the
Relicry / Delving skills is data (skills.json, items.json, relicry.json); dungeons are built by build_relic_dungeon.py.
"""
import math
import random
import unreal
from aerondel_util import mat
from build_starter_town import box, CUBE, CYL, CONE

# Per-biome prop kit: (name, shape, (sx, sy, sz) metres, rgb). Kept to a few cheap meshes per region.
PROPS = {
    "tundra":   [("SnowPine", CONE, (2.2, 5.5, 2.2), (0.28, 0.4, 0.34)), ("IceRock", CUBE, (1.6, 1.0, 1.4), (0.75, 0.85, 0.95))],
    "alpine":   [("Crag", CUBE, (2.6, 3.2, 2.2), (0.5, 0.5, 0.52)), ("AlpinePine", CONE, (1.8, 6.0, 1.8), (0.2, 0.36, 0.26))],
    "volcanic": [("BasaltSpire", CONE, (1.6, 4.0, 1.6), (0.18, 0.15, 0.14)), ("EmberVent", CYL, (1.2, 0.4, 1.2), (0.9, 0.35, 0.1))],
    "desert":   [("Cactus", CYL, (0.6, 3.0, 0.6), (0.35, 0.55, 0.3)), ("Dune", CYL, (7.0, 1.2, 4.0), (0.85, 0.74, 0.5))],
    "jungle":   [("Kapok", CYL, (1.2, 8.0, 1.2), (0.36, 0.28, 0.18)), ("Fern", CONE, (2.4, 1.4, 2.4), (0.2, 0.55, 0.2))],
    "swamp":    [("Cypress", CYL, (0.9, 6.0, 0.9), (0.3, 0.32, 0.22)), ("Reeds", CONE, (1.0, 1.6, 1.0), (0.5, 0.55, 0.3))],
}


def _rgb(hexv):
    h = int(hexv) if not isinstance(hexv, str) else int(hexv.lstrip("#"), 16)
    return ((h >> 16 & 255) / 255.0, (h >> 8 & 255) / 255.0, (h & 255) / 255.0)


def build(w, density=1.0):
    regions, biomes = w.get("regions", []), w.get("biomes", {})
    for R in regions:
        b = biomes.get(R["biome"], {})
        rid, folder = R["name"].replace(" ", "_"), "Regions/" + R["name"]
        tint = b.get("map", b.get("tint", 0x808080))
        ground = mat("Biome_" + R["biome"], _rgb(tint), 0.95)
        box(rid + "_Ground", folder, (R["x"], -0.04, R["z"]), (R["r"] * 2, 0.06, R["r"] * 2), ground, shape=CYL, collide=False)
        rng = random.Random(hash(R["name"]) & 0xFFFF)
        kit = PROPS.get(R["biome"], [])
        for i in range(int(18 * density)):
            if not kit:
                break
            n, shape, size, rgb = kit[i % len(kit)]
            a, d = rng.random() * math.tau, R["r"] * (0.25 + 0.7 * rng.random())
            x, z = R["x"] + math.cos(a) * d, R["z"] + math.sin(a) * d
            s = 0.8 + rng.random() * 0.5
            box("%s_%s_%02d" % (rid, n, i), folder + "/Props", (x, size[1] * s / 2, z), tuple(v * s for v in size), mat(n, rgb, 0.9),
                shape=shape, yaw=rng.random() * 360)
        dig = R.get("dig")
        if dig:
            sx, sz = R["x"] + dig["dx"], R["z"] + dig["dz"]
            f = folder + "/DigSite"
            soil = mat("DigSoil", (0.4, 0.32, 0.22))
            for j, (ox, oz) in enumerate(((-4, -2), (0, 3.5), (4, -1.5))):
                box("%s_Trench_%d" % (rid, j), f, (sx + ox, 0.4, sz + oz), (3, 0.9, 3), soil, shape=CONE)
            box(rid + "_RestorationBench", f, (sx + 5, 0.45, sz + 5), (2.2, 0.9, 1), mat("Bench", (0.48, 0.35, 0.22)))
            stone = mat("VaultStone", (0.5, 0.48, 0.44))
            for s in (-1, 1):
                box("%s_VaultPillar_%d" % (rid, s), f, (sx - 5 + s * 1.3, 1.5, sz + 5), (0.6, 3, 0.6), stone)
            box(rid + "_VaultLintel", f, (sx - 5, 3.1, sz + 5), (3.4, 0.5, 0.7), stone)
            box(rid + "_VaultDoor", f, (sx - 5, 1.4, sz + 5), (2, 2.8, 0.3), mat("VaultDoor", (0.6, 0.5, 0.3), 0.5, 0.4))
        hub = R.get("delveHub")
        if hub:
            hx, hz = R["x"] + hub["dx"], R["z"] + hub["dz"]
            box("Delvers_Descent_Ring", folder + "/DelveHub", (hx, 0.25, hz), (5, 0.5, 5), mat("HubStone", (0.54, 0.48, 0.35)), shape=CYL)
            box("Delvers_Descent_Pit", folder + "/DelveHub", (hx, 0.27, hz), (3.4, 0.52, 3.4), mat("HubPit", (0.07, 0.05, 0.1)), shape=CYL, collide=False)
    unreal.log("[Aerondel] %d climate regions built" % len(regions))
