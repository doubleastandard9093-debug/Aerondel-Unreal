#!/usr/bin/env python3
"""Offline checks for the Phase 22 Unreal port (Seasonboard, Wildmarks, Ashfall)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

def exists(rel):
    return os.path.exists(os.path.join(ROOT, rel))

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
hdr = read("Source/Aerondel/Features/VenturesFeature.h")
ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")
proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

if exists("Source/Aerondel/Features/Ventures_Seasonboard.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Seasonboard.cpp")
    for name in ("ScoreOn", "ScoreClaim", "ScoreBuy", "kit-ashthread", "dye-ashveil", "ashen-paths"):
        if name not in src:
            errs.append(f"Seasonboard missing {name}")
    if "Standing" not in src:
        errs.append("Seasonboard should say score is not Standing")
    if "ScoreHas" not in src:
        errs.append("ScoreHas must live with Seasonboard so the Ash-thread kit can unlock")
    if "bool UVenturesFeature::ScoreHas" in read("Source/Aerondel/Features/Ventures_Kitwright.cpp"):
        errs.append("ScoreHas is still defined in Kitwright; Seasonboard should own it")
    if "ScoreOn(TEXT(\"mine\"))" not in read("Source/Aerondel/Interaction/ResourceNode.cpp"):
        errs.append("mining does not award season score")
    if "ScoreOn(TEXT(\"hunt\"))" not in read("Source/Aerondel/Features/Ventures_Huntwright.cpp"):
        errs.append("hunt carve does not award season score")
    if "ScoreOn(TEXT(\"climb\"))" not in read("Source/Aerondel/Features/Ventures_Freeclimb.cpp"):
        errs.append("climb begin does not award season score")
    if "ClimbDrainMul" not in read("Source/Aerondel/Features/Ventures_Freeclimb.cpp"):
        errs.append("climb drain does not honor Ridgewalk soles")
    if "ScoreOn(TEXT(\"tide\"))" not in read("Source/Aerondel/Features/Ventures_Zone.cpp"):
        errs.append("Tide end does not award season score")
    if "ScoreOn(TEXT(\"camp\"))" not in read("Source/Aerondel/Features/Ventures_Waycamp.cpp"):
        errs.append("Waycamp place does not award season score")
    if "ScoreOn(TEXT(\"trail\"))" not in read("Source/Aerondel/Features/Ventures_Trailhand.cpp"):
        errs.append("Trailhand win does not award season score")
    if "WardrobeDyeName" not in feat:
        errs.append("Lookplates do not include the Ashveil season dye")
    if npcs.get("seasonboard", {}).get("service") != "score":
        errs.append("Boardclerk Edda missing or service != score")
    if npcs.get("scorevendor", {}).get("service") != "score":
        errs.append("Waresmith Ryn missing or service != score")
    if "Ridgewalk soles" not in items or "Season flask" not in items:
        errs.append("missing Seasonboard vendor items")
    if "ResetScore()" not in feat:
        errs.append("missing ResetScore wiring")
    if "kit-ashthread" not in read("Source/Aerondel/Features/Ventures_Kitwright.cpp"):
        errs.append("Kitwright has no Ash-thread ScoreHas gate")

if exists("Source/Aerondel/Features/Ventures_Wildmarks.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Wildmarks.cpp")
    for name in ("MarkGrant", "MarkHush", "MarkClear", "nighteye", "kindled", "deeplung"):
        if name not in src:
            errs.append(f"Wildmarks missing {name}")
    if "damage percent" not in src.lower() and "not a damage" not in src.lower() and "Not a damage" not in src:
        errs.append("Wildmarks should say they are not a damage percent")
    if "Quirk" not in src and "quirk" not in src:
        errs.append("Wildmarks should say they are not Quirks")
    quirks = read("Source/Aerondel/Features/QuirksFeature.cpp") if exists("Source/Aerondel/Features/QuirksFeature.cpp") else ""
    if "blight" in quirks or "Wildmark" in quirks or "MarkClear" in quirks:
        errs.append("Ysolde's quirk remedy must not clear Wildmarks")
    if npcs.get("wildmarks", {}).get("service") != "blight":
        errs.append("Markwise Calla missing or service != blight")
    for it in ("Wildmark tonic", "Mark-hush tonic", "Mark-clear tonic"):
        if it not in items:
            errs.append(f"missing {it}")
    if "ResetMark()" not in feat:
        errs.append("missing ResetMark wiring")
    if "TEXT(\"ash\")" in src and "SetObjectField(TEXT(\"ash\")" in src:
        errs.append("Wildmarks must not use the Ashfall save key")

if exists("Source/Aerondel/Features/Ventures_Ashfall.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Ashfall.cpp")
    for name in ("AshBegin", "AshPass", "AshEnd", "AshCall", "AshGather"):
        if name not in src:
            errs.append(f"Ashfall missing {name}")
    if 'TEXT("ash")' not in src and 'TEXT("ash")' not in src:
        errs.append("Ashfall must save under key ash")
    if "SetObjectField(TEXT(\"flare\")" in src or "JObj(A, TEXT(\"flare\")" in src:
        errs.append("Ashfall must not use save key flare (Overflare owns adapt.flare)")
    if "AshEnd" in src and "load" not in src.lower() and "LoadAsh" in src:
        # Load must end in-progress flares
        if "AshEnd" not in src[src.find("LoadAsh"):src.find("LoadAsh")+800] and "started" not in src[src.find("LoadAsh"):src.find("LoadAsh")+800]:
            errs.append("Ashfall Load should end an in-progress flare safely")
    if "Greenstead" not in src or "Claimridge" not in src:
        errs.append("Ashfall must refuse Greenstead and Claimridge")
    if "Townstead" not in src and "NearTownHouse" not in src:
        errs.append("Ashfall must refuse Townsteads")
    if npcs.get("ashfall", {}).get("service") != "ash":
        errs.append("Ashwatcher Bren missing or service != ash")
    if "Ash-totem" not in items or "Ash crystal" not in items:
        errs.append("missing Ashfall items")
    if "ResetAsh()" not in feat:
        errs.append("missing ResetAsh wiring")
    if "AshRedirectTide" not in read("Source/Aerondel/Features/Ventures_Zone.cpp") and "AshBusyRegion" not in read("Source/Aerondel/Features/Ventures_Zone.cpp"):
        if "AshActiveIn" not in read("Source/Aerondel/Features/Ventures_Zone.cpp") and "AshBlocksTide" not in read("Source/Aerondel/Features/Ventures_Zone.cpp"):
            errs.append("Burst Tide does not redirect away from an active Ashfall region")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 22 wiring and cook-safe ini.")
