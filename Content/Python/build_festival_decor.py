"""
Aerondel > Build Festival Decor: places every seasonal event's decorations (web js/events.js) around Larkhaven square in the
OPEN level (open /Game/Gielinor/Maps/World first). Each piece is tagged "festival:<id>"; pieces 7-14 of a ring also carry
"festival_full". At runtime USeasonalEventsFeature hides decor for events that are not running, and hides "festival_full"
pieces on Low / Quest so Low keeps 6 pieces per event like the web. Re-running replaces the old decor (folder Festival/).
"""
import json
import math
import os
import unreal
import aerondel_safe as safe
from aerondel_util import mat
from build_starter_town import box, actors, CUBE, CYL, SPHERE, CONE

FULL, LOW = 14, 6


def rgb(h):
    return ((h >> 16 & 255) / 255.0, (h >> 8 & 255) / 255.0, (h & 255) / 255.0)


def piece(E, i, x, z, folder, mats):
    c = mats[i % len(mats)]
    parts = []
    n = "%s_%02d" % (E["id"], i)
    if E["deco"] == "pumpkins":
        parts.append(box(n + "_Pumpkin", folder, (x, 0.32, z), (0.9, 0.68, 0.9), c, SPHERE, collide=False))
        parts.append(box(n + "_Stem", folder, (x, 0.72, z), (0.12, 0.2, 0.12), mat("Festival_Stem", (0.23, 0.35, 0.1)), CYL, collide=False))
    elif E["deco"] == "snowmen":
        white = mat("Festival_Snow", (0.96, 0.97, 1.0))
        for k, (r, h) in enumerate(((0.5, 0.45), (0.36, 1.1), (0.25, 1.6))):
            parts.append(box("%s_Snow%d" % (n, k), folder, (x, h, z), (r * 2, r * 2, r * 2), white, SPHERE, collide=False))
    elif E["deco"] == "flowers":
        parts.append(box(n + "_Bed", folder, (x, 0.1, z), (1.4, 0.2, 1.4), mat("Festival_Soil", (0.35, 0.23, 0.13)), CYL, collide=False))
        for k in range(3 if i >= LOW else 5):
            parts.append(box("%s_Bloom%d" % (n, k), folder, (x + math.cos(k * 1.3) * 0.4, 0.3, z + math.sin(k * 1.3) * 0.4), (0.32, 0.32, 0.32), mats[(i + k) % len(mats)], SPHERE, collide=False))
    else:  # lanterns / bunting poles
        parts.append(box(n + "_Pole", folder, (x, 1.3, z), (0.16, 2.6, 0.16), mat("Festival_Pole", (0.29, 0.21, 0.15)), CYL))
        parts.append(box(n + "_Top", folder, (x, 2.7, z), (0.48, 0.48, 0.48) if E["deco"] == "lanterns" else (0.6, 0.5, 0.6), c, SPHERE if E["deco"] == "lanterns" else CONE, collide=False))
    for a in [p for p in parts if p]:
        tags = [unreal.Name("festival:" + E["id"])] + ([unreal.Name("festival_full")] if i >= LOW else [])
        a.set_editor_property("tags", tags)
        a.static_mesh_component.set_editor_property("cast_shadow", False)


def run():
    path = os.path.join(unreal.Paths.project_content_dir(), "Data", "events.json")
    events = json.load(open(path, encoding="utf-8"))["events"]
    for a in actors.get_all_level_actors():
        if str(a.get_folder_path()).startswith("Festival"):
            actors.destroy_actor(a)
    with safe.SafeRun("Festival decor", len(events), save_fn=unreal.EditorLevelLibrary.save_current_level) as task:
        for k, E in enumerate(events):
            task.frame(E["name"])
            _event(k, E)
    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[Aerondel] Festival decor placed for %d events (hidden at runtime unless the event is running)." % len(events))


def _event(k, E):
    mats = [mat("Festival_%s_%d" % (E["id"], j), rgb(c), 0.6) for j, c in enumerate(E["colors"])]
    folder = "Festival/" + E["name"]
    r = 8.2 + (k % 3) * 1.1  # web rings at 8.2 m + 1.1 m per concurrent event
    for i in range(FULL):
        a = (i / FULL) * math.pi * 2 + k * 0.22
        piece(E, i, math.cos(a) * r, math.sin(a) * r, folder, mats)
