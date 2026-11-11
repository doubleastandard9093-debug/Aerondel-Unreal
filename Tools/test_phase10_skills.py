#!/usr/bin/env python3
"""Offline checks for the Phase 10 Hunter / Huntwright port (no engine needed)."""
import json, os, re, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

skills = json.load(open(os.path.join(ROOT, "Content/Data/skills.json")))
if "Hunter" not in skills["skills"]:
    errs.append("Hunter missing from skills.json skills list")
if "Construction" in skills["skills"] or "Farming" in skills["skills"]:
    pass  # allowed once later phases land
hp = skills["perks"].get("Hunter") or []
if len(hp) < 57:
    errs.append(f"Hunter perks {len(hp)}, expected 57 (to 1000)")
if not any(p.get("name") == "Tracker" and p.get("level") == 25 for p in hp):
    errs.append("Hunter milestone Tracker @ 25 missing")
if not any(p.get("name") == "Hunter Ascendant" for p in hp):
    errs.append("Hunter Ascendant missing")

src = open(os.path.join(ROOT, "Source/Aerondel/Data/SkillDef.cpp"), encoding="utf-8").read()
if 'TEXT("hunter")' not in src:
    errs.append("MaxLevelFor does not list hunter")

items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
for k in ("Hunt remnant", "Cinder horn", "Brandhide helm", "Brandhide cleaver", "Ash plate", "Ember tail"):
    if k not in items:
        errs.append(f"missing item {k}")

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
sable = npcs.get("huntwright") or {}
if sable.get("name") != "Huntwright Sable" or sable.get("service") != "hunt":
    errs.append("Huntwright Sable NPC missing or wrong service")

hunt = open(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Huntwright.cpp"), encoding="utf-8").read()
for name in ("Cinder Warden", "Sunderlord Vexmar", "Stonehorn", "HuntCarve", "HuntTrack", "HuntCraft"):
    if name not in hunt:
        errs.append(f"Ventures_Huntwright.cpp missing {name}")

feat = open(os.path.join(ROOT, "Source/Aerondel/Features/VenturesFeature.cpp"), encoding="utf-8").read()
for s in ("ResetHunt()", "SaveHunt(Out)", "LoadHunt(In)", "TickHunt()", "HitHunt", "DiedHunt", "GoTab(TEXT(\"hunt\"))"):
    if s not in feat:
        errs.append(f"VenturesFeature.cpp missing {s}")

ini = open(os.path.join(ROOT, "Config/DefaultScalability.ini"), encoding="utf-8").read()
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")

proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print(f"OK: Hunter {len(hp)} perks, hunt items present, Sable service=hunt, wiring in place, cook-safe ini.")
