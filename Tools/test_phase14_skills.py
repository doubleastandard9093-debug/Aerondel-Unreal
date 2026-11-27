#!/usr/bin/env python3
"""Offline checks for the Phase 14 Unreal port (Hordewake, Deepmarks, Ashspire)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
zone = read("Source/Aerondel/Features/Ventures_Zone.cpp")
ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")
proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

if npcs.get("hordewake", {}).get("service") != "horde":
    errs.append("Wakekeeper Brin missing or service != horde")
if "HordeStart" not in read("Source/Aerondel/Features/Ventures_Hordewake.cpp"):
    errs.append("Ventures_Hordewake.cpp missing HordeStart")
if "ResetHorde()" not in feat:
    errs.append("missing ResetHorde wiring")
if 'ZoneId == TEXT("horde")' not in zone:
    errs.append("ZoneC missing horde")
if "TickHordeFire" not in read("Source/Aerondel/Combat/PlayerCombatComponent.cpp"):
    errs.append("combat tick missing TickHordeFire")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Deepmarks.cpp")):
    src = read("Source/Aerondel/Features/Ventures_Deepmarks.cpp")
    for name in ("MarkStart", "stash", "hush", "Pinnacle"):
        if name not in src:
            errs.append(f"Ventures_Deepmarks.cpp missing {name}")
    if npcs.get("deepmarks", {}).get("service") != "marks":
        errs.append("Markwarden Sera missing or service != marks")
    if "ResetMark()" not in feat:
        errs.append("missing ResetMark wiring")
    if "MarkFinish" not in zone:
        errs.append("RunEnd missing MarkFinish")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Ashspire.cpp")):
    src = read("Source/Aerondel/Features/Ventures_Ashspire.cpp")
    for name in ("SpireStart", "Hearthmother", "firstbind", "SpireBuy"):
        if name not in src:
            errs.append(f"Ventures_Ashspire.cpp missing {name}")
    if npcs.get("ashspire", {}).get("service") != "spire":
        errs.append("Spirekeeper Nahl missing or service != spire")
    if "ResetSpire()" not in feat:
        errs.append("missing ResetSpire wiring")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 14 Hordewake wiring, Brin, cook-safe ini.")
