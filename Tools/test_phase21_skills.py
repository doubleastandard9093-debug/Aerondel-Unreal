#!/usr/bin/env python3
"""Offline checks for the Phase 21 Unreal port (Oathframe, Hardroad)."""
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

if exists("Source/Aerondel/Features/Ventures_Oathframe.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Oathframe.cpp")
    for name in ("OathAssemble", "OathStepIn", "OathStepOut", "OathSlam", "hearth-core", "damage percent"):
        if name not in src:
            errs.append(f"Oathframe missing {name}")
    if "never persist" not in src.lower() and "never persist" not in src and 'bOathOn = false; // never persist' not in src:
        if "never persist" not in src and "bOathOn = false" not in src:
            errs.append("Oathframe must not persist being inside the suit")
    if npcs.get("oathframe", {}).get("service") != "oath":
        errs.append("Framewright Orel missing or service != oath")
    if "ResetOath()" not in feat:
        errs.append("missing ResetOath wiring")
    if "OathSlam" not in read("Source/Aerondel/Features/Ventures_Season.cpp"):
        errs.append("Dash (C) does not become an Oathframe slam")
    if "OathOn" not in read("Source/Aerondel/Features/Ventures_Freeclimb.cpp"):
        errs.append("Freeclimb does not refuse the Oathframe")
    if "OathOn" not in read("Source/Aerondel/Features/SkywingFeature.cpp"):
        errs.append("Skywing does not refuse the Oathframe")

if exists("Source/Aerondel/Features/Ventures_Hardroad.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Hardroad.cpp")
    for name in ("RoadToggle", "RoadEat", "RoadDrink", "RoadRest", "RoadRegenMul"):
        if name not in src:
            errs.append(f"Hardroad missing {name}")
    if "never kill" not in src.lower() and "never lethal" not in src.lower() and "do not damage" not in src.lower():
        if "never kill" not in src and "They never kill you" not in src:
            errs.append("Hardroad should say it is never lethal")
    if "bRoadOn = false" not in src and "on: false" not in src:
        # default off in Reset/Load
        if "bRoadOn" not in read("Source/Aerondel/Features/VenturesFeature.h"):
            errs.append("Hardroad missing off-by-default flag")
    if npcs.get("hardroad", {}).get("service") != "road":
        errs.append("Roadwise Marla missing or service != road")
    if "ResetRoad()" not in feat:
        errs.append("missing ResetRoad wiring")
    if "RoadRegenMul" not in read("Source/Aerondel/Features/Ventures_Deepmarks.cpp"):
        errs.append("Prayer regen does not honor Hardroad")
    if "RoadRegenMul" not in read("Source/Aerondel/Core/PlayerStatsComponent.cpp"):
        errs.append("Health/run regen does not honor Hardroad")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 21 wiring and cook-safe ini.")
