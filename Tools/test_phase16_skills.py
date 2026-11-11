#!/usr/bin/env python3
"""Offline checks for the Phase 16 Unreal port (Freeclimb, Leychemistry, Freeframe, Holdraids, polish)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

def exists(rel):
    return os.path.exists(os.path.join(ROOT, rel))

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")
proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

# Freeclimb
if exists("Source/Aerondel/Features/Ventures_Freeclimb.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Freeclimb.cpp")
    for name in ("ClimbBegin", "ClimbLaunch", "Press ,"):
        if name not in src:
            errs.append(f"Freeclimb missing {name}")
    if "ResetClimb()" not in feat:
        errs.append("missing ResetClimb wiring")
    if "Climbing()" not in read("Source/Aerondel/Features/Ventures_Season.cpp"):
        errs.append("Dash missing climb handoff")
    if "IA_Climb" not in read("Source/Aerondel/Player/AerondelCharacter.cpp"):
        errs.append("comma key not bound for Freeclimb")
    if "TickClimb" not in read("Source/Aerondel/Player/AerondelCharacter.cpp"):
        errs.append("Character Tick missing TickClimb")

# Leychemistry
if exists("Source/Aerondel/Features/Ventures_Leychem.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Leychem.cpp")
    for name in ("LeyIgnite", "LeyPushWind", "LeyProtected", "rain"):
        if name not in src and name != "Rowan":
            errs.append(f"Leychem missing {name}")
    if npcs.get("leychem", {}).get("service") != "chem":
        errs.append("Leychemist Rowan missing or service != chem")
    if "ResetChem()" not in feat:
        errs.append("missing ResetChem wiring")

# Freeframe
if exists("Source/Aerondel/Features/Ventures_Freeframe.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Freeframe.cpp")
    for name in ("FrameBuy", "Claimridge", "foundation", "frameSettle"):
        if name not in src:
            errs.append(f"Freeframe missing {name}")
    if npcs.get("freeframe", {}).get("service") != "frame":
        errs.append("Plotwarden Tess missing or service != frame")
    items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
    recs = json.load(open(os.path.join(ROOT, "Content/Data/recipes.json")))["recipes"]
    if "Stone slab" not in items or "Limestone" not in items:
        errs.append("missing Limestone / Stone slab items")
    if not any(r.get("name") == "Cut Stone slab" for r in recs):
        errs.append("missing Cut Stone slab recipe")

# Holdraids
if exists("Source/Aerondel/Features/Ventures_Holdraids.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Holdraids.cpp")
    for name in ("RaidStart", "RaidDelay", "RaidSkip", "lastChip", "8000"):
        if name not in src:
            errs.append(f"Holdraids missing {name}")
    if npcs.get("holdraids", {}).get("service") != "raids":
        errs.append("Ward-captain Rhee missing or service != raids")
    if "ResetRaids()" not in feat:
        errs.append("missing ResetRaids wiring")

# Polish: mining side drop
if exists("Source/Aerondel/Rules/AerondelRules.cpp") and "MineSideDrop" in read("Source/Aerondel/Rules/AerondelRules.cpp"):
    if "MineSideDrop" not in read("Source/Aerondel/Interaction/ResourceNode.cpp"):
        errs.append("ResourceNode does not apply MineSideDrop")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 16 wiring and cook-safe ini.")
