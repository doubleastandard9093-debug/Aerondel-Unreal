#!/usr/bin/env python3
"""Offline checks for the Phase 12 Unreal port (Farming, Town Ties, Greenstead)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

skills = json.load(open(os.path.join(ROOT, "Content/Data/skills.json")))
if "Farming" not in skills["skills"]:
    errs.append("Farming missing from skills.json skills list")
fp = skills["perks"].get("Farming") or []
if len(fp) < 57:
    errs.append(f"Farming perks {len(fp)}, expected 57 (to 1000)")
if not any(p.get("name") == "Cropwise" and p.get("level") == 25 for p in fp):
    errs.append("Farming milestone Cropwise @ 25 missing")
if not any(p.get("name") == "Farming Ascendant" for p in fp):
    errs.append("Farming Ascendant missing")

src = read("Source/Aerondel/Data/SkillDef.cpp")
if 'TEXT("farming")' not in src:
    errs.append("MaxLevelFor does not list farming")

loader = read("Source/Aerondel/Data/JsonContentLoader.cpp")
if "Farming" not in loader or "MaxLevel = 1000" not in loader:
    errs.append("JsonContentLoader does not cap Farming at 1000")

ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")

proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

feat = read("Source/Aerondel/Features/VenturesFeature.cpp")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_TownTies.cpp")):
    ties = read("Source/Aerondel/Features/Ventures_TownTies.cpp")
    for name in ("TieTalk", "TieGift", "Kindred", "TieDiscount"):
        if name not in ties:
            errs.append(f"Ventures_TownTies.cpp missing {name}")
    if "ResetTie()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetTie wiring")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Greenstead.cpp")):
    green = read("Source/Aerondel/Features/Ventures_Greenstead.cpp")
    for name in ("GreenBuy", "GreenPlant", "GreenHarvest", "Potato", "Farming"):
        if name not in green:
            errs.append(f"Ventures_Greenstead.cpp missing {name}")
    if "ResetGreen()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetGreen wiring")
    items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
    for k in ("Potato seed", "Potato", "Farm preserves", "Wheat seed", "Egg"):
        if k not in items:
            errs.append(f"missing farm item {k}")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print(f"OK: Farming {len(fp)} perks, cap wiring in place, cook-safe ini.")
