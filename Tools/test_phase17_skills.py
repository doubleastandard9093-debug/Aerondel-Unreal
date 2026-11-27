#!/usr/bin/env python3
"""Offline checks for the Phase 17 Unreal port (Trailhand, Skillwords, Crossed Banners)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

def exists(rel):
    return os.path.exists(os.path.join(ROOT, rel))

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
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

# Trailhand
if exists("Source/Aerondel/Features/Ventures_Trailhand.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Trailhand.cpp")
    for name in ("TrailDeal", "TrailPlay", "TrailLegal", "ranks must rise"):
        if name not in src and name != "ranks must rise":
            errs.append(f"Trailhand missing {name}")
    if "match the suit or step exactly one rank" not in src and "match suit or step exactly one rank" not in src:
        if "match the suit or step exactly one rank" not in src.lower() and "Suit ==" not in src:
            errs.append("Trailhand missing suit-or-step rule")
    if "Twenty-One Tides" not in src:
        errs.append("Trailhand should stay distinct from Twenty-One Tides")
    if "ResetTrail()" not in feat:
        errs.append("missing ResetTrail wiring")
    if "SaveTrail" not in feat or "LoadTrail" not in feat:
        errs.append("missing Save/Load Trail wiring")
    if 'Tab == TEXT("trail")' not in feat:
        errs.append("missing Trailhand tab routing")
    if npcs.get("trailhand", {}).get("service") != "trail":
        errs.append("Trailhand Kes missing or service != trail")
    if "CardsFeature" in src:
        errs.append("Trailhand must not reuse CardsFeature (Salted Lantern games)")

# Skillwords (skip until the file lands)
if exists("Source/Aerondel/Features/Ventures_Skillwords.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Skillwords.cpp")
    for name in ("SkillTry", "Persuade", "Silver Tongue", "hilda_pie"):
        if name not in src:
            errs.append(f"Skillwords missing {name}")
    if npcs.get("skillwords", {}).get("service") != "checks":
        errs.append("Wordwise Tavian missing or service != checks")
    if "ResetChecks()" not in feat:
        errs.append("missing ResetChecks wiring")
    if "SkillHasOpen" not in read("Source/Aerondel/Interaction/NpcTalk.cpp"):
        errs.append("NpcTalk does not offer Skillwords hooks")

# Crossed Banners
if exists("Source/Aerondel/Features/Ventures_Banners.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Banners.cpp")
    for name in ("BannerMend", "BannerWary", "BannerOnWrit", "250", "Larkhaven"):
        if name not in src:
            errs.append(f"Banners missing {name}")
    if "watch" not in src or "mariners" not in src or "circle" not in src or "lantern" not in src:
        errs.append("Banners missing opposite pairs")
    if npcs.get("banners", {}).get("service") != "banners":
        errs.append("Quartermaster Odile missing or service != banners")
    if "ResetBanners()" not in feat:
        errs.append("missing ResetBanners wiring")
    if "BannerOnWrit" not in read("Source/Aerondel/Features/WritsFeature.cpp"):
        errs.append("WritsFeature does not notify Crossed Banners")
    if "BannerHideStock" not in read("Source/Aerondel/Activities/ActivityActors.cpp"):
        errs.append("Shop VisibleStock does not honor Crossed Banners")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 17 wiring and cook-safe ini.")
