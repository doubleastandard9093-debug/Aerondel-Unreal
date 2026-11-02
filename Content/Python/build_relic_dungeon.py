"""
Aerondel > Build Relic Dungeon: a separate map (/Game/Gielinor/Maps/RelicDungeon) with a sample relic dungeon laid out
by the same room-grid generator as the web js/dungeon.js: 16 m rooms on a 22 m grid, corridors with gate slabs,
a side chest room and a boss room. Each gate is labelled with the skill it tests (relicry.json "gates").
Not World Partition: it is small and loads as one level (stream it in as a sublevel or open it directly).
Re-run with a different seed for a different layout.
"""
import json
import os
import random
import unreal
from aerondel_util import ROOT, mat, eal
from build_starter_town import box, label, actors, levels, V

MAP = ROOT + "/Maps/RelicDungeon"
ROOM, STEP, WALL_H = 16.0, 22.0, 3.4
DIRS = ((0, 1), (1, 0), (-1, 0), (0, -1))


def layout(rng, n, side):
    rooms, used, links = [(0, 0, True)], {(0, 0)}, []
    while len(rooms) < n:
        gx, gz, _ = rooms[-1]
        opts = [d for d in DIRS if (gx + d[0], gz + d[1]) not in used and gz + d[1] >= 0]
        if not opts:
            return layout(rng, n, side)
        dx, dz = (0, 1) if rng.random() < 0.5 and (0, 1) in opts else rng.choice(opts)
        rooms.append((gx + dx, gz + dz, True)); used.add((gx + dx, gz + dz)); links.append((len(rooms) - 2, len(rooms) - 1))
    for _ in range(side):
        for _t in range(20):
            p = rng.randrange(1, max(2, n - 1)); dx, dz = rng.choice(DIRS); g = (rooms[p][0] + dx, rooms[p][1] + dz)
            if g[1] >= 0 and g not in used:
                rooms.append((g[0], g[1], False)); used.add(g); links.append((p, len(rooms) - 1)); break
    return rooms, links


def run(seed=2026, site="Frozen Barrow"):
    data = json.load(open(os.path.join(unreal.Paths.project_content_dir(), "Data", "relicry.json"), encoding="utf-8"))
    theme = data["sites"][site]["theme"]
    rgb = lambda h: ((h >> 16 & 255) / 255.0, (h >> 8 & 255) / 255.0, (h & 255) / 255.0)
    floor, wall, accent = mat("Dungeon_Floor", rgb(theme["floor"])), mat("Dungeon_Wall", rgb(theme["wall"])), mat("Dungeon_Gate", rgb(theme["accent"]), 0.5, 0.3)
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    levels.new_level(MAP)
    rng = random.Random(seed)
    rooms, links = layout(rng, 8, 1)
    gates = list(data["gates"].keys()); rng.shuffle(gates)
    pos = [(gx * STEP, gz * STEP) for gx, gz, _ in rooms]
    opened = [set() for _ in rooms]
    main_links = [l for l in links if rooms[l[1]][2]]
    for k, (a, b) in enumerate(links):
        (ax, az), (bx, bz) = pos[a], pos[b]
        dx, dz = (rooms[b][0] - rooms[a][0]), (rooms[b][1] - rooms[a][1])
        opened[a].add((dx, dz)); opened[b].add((-dx, -dz))
        cx, cz, along = (ax + bx) / 2, (az + bz) / 2, dx != 0
        box("Corridor_%d" % k, "Dungeon/Corridors", (cx, -0.1, cz), (6.2 if along else 3.4, 0.2, 3.4 if along else 6.2), floor)
        for s in (-1, 1):
            box("CorridorWall_%d_%d" % (k, s), "Dungeon/Corridors", (cx if along else cx + 2 * s, WALL_H / 2, cz + 2 * s if along else cz),
                (6 if along else 0.6, WALL_H, 0.6 if along else 6), wall)
        mi = main_links.index((a, b)) if (a, b) in main_links else -1
        if 0 <= mi < len(main_links) - 1 and mi < len(gates):
            g = gates[mi]
            box("Gate_%d_%s" % (mi, g), "Dungeon/Gates", (cx, 1.3, cz), (0.5 if along else 3.4, 2.6, 3.4 if along else 0.5), accent)
            label(actors.spawn_actor_from_class(unreal.TextRenderActor, V(cx, 3.2, cz)), "GateLabel_%d %s (%s)" % (mi, data["gates"][g]["name"], g), "Dungeon/Gates")
    h = ROOM / 2
    for i, (x, z) in enumerate(pos):
        box("Room_%d" % i, "Dungeon/Rooms", (x, -0.1, z), (ROOM, 0.2, ROOM), floor)
        for dx, dz in DIRS:
            wx, wz, alongx = x + dx * h, z + dz * h, dz != 0
            if (dx, dz) in opened[i]:
                seg = (ROOM - 3.4) / 2
                for s in (-1, 1):
                    o = s * (1.7 + seg / 2)
                    box("Wall_%d_%d_%d_%d" % (i, dx, dz, s), "Dungeon/Rooms", (wx + (o if alongx else 0), WALL_H / 2, wz + (0 if alongx else o)),
                        (seg if alongx else 0.6, WALL_H, 0.6 if alongx else seg), wall)
            else:
                box("Wall_%d_%d_%d" % (i, dx, dz), "Dungeon/Rooms", (wx, WALL_H / 2, wz), (ROOM + 0.6 if alongx else 0.6, WALL_H, 0.6 if alongx else ROOM + 0.6), wall)
    mains = [i for i, r in enumerate(rooms) if r[2]]
    bx, bz = pos[mains[-1]]
    box("Boss_Relic_Colossus_Marker", "Dungeon/Boss", (bx, 1.8, bz), (2.4, 3.6, 2.4), mat("Boss", (0.35, 0.31, 0.28)))
    side = [i for i, r in enumerate(rooms) if not r[2]]
    cx, cz = pos[side[0]] if side else (bx + 4, bz)
    box("Reliquary_Chest", "Dungeon/Loot", (cx + 4, 0.5, cz), (1.4, 1, 1), mat("Chest", (0.69, 0.54, 0.23)))
    sx, sz = pos[0]
    label(actors.spawn_actor_from_class(unreal.PlayerStart, V(sx, 1.0, sz - 3)), "PlayerStart", "Dungeon")
    label(actors.spawn_actor_from_class(unreal.PointLight, V(sx, 3.0, sz)), "DungeonLight", "Dungeon")
    sun = actors.spawn_actor_from_class(unreal.SkyLight, V(0, 20, 0)); label(sun, "SkyLight", "Dungeon")
    levels.save_current_level()
    unreal.log("[Aerondel] Relic dungeon (%s, seed %d, %d rooms) saved to %s" % (site, seed, len(rooms), MAP))
