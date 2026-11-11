#!/usr/bin/env python3
"""Offline checks for the Phase 18 Unreal port (Kitwright, Wayfreight, Waycamp)."""
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
ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")
proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

if exists("Source/Aerondel/Features/Ventures_Kitwright.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Kitwright.cpp")
    for name in ("KitCraft", "KitAttach", "KitRemove", "longhaft", "hush", "not grafts", "Frontier uniques"):
        if name not in src and name != "not grafts":
            if name == "not grafts" and "not grafts" not in src.lower() and "They are not grafts" not in src:
                errs.append("Kitwright should say kits are not grafts")
            elif name != "not grafts":
                errs.append(f"Kitwright missing {name}")
    if "damage percent" not in src and "damage bonus" not in src:
        errs.append("Kitwright should say kits are not a damage percent")
    if npcs.get("kitwright", {}).get("service") != "kits":
        errs.append("Kitwright Osha missing or service != kits")
    for name in ("Longhaft kit", "Hush-string kit", "Lamp-rim kit", "Padded collar kit", "Watch-sight kit"):
        if name not in items:
            errs.append(f"items.json missing {name}")
    if "ResetKits()" not in feat:
        errs.append("missing ResetKits wiring")
    if "KitSpeedBonus" not in read("Source/Aerondel/Rules/AerondelRules.cpp"):
        errs.append("Attack speed does not honor Kitwright")
    if "KitRangeBonus" not in read("Source/Aerondel/Rules/AerondelRules.cpp"):
        errs.append("Attack range does not honor Kitwright")
    if "KitPoiseBonus" not in read("Source/Aerondel/Features/Ventures_Guardbreak.cpp"):
        errs.append("Player poise does not honor Kitwright")
    if "KitFocusMul" not in read("Source/Aerondel/Features/StillwatchFeature.cpp"):
        errs.append("Stillwatch focus does not honor Watch-sight")

if exists("Source/Aerondel/Features/Ventures_Wayfreight.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Wayfreight.cpp")
    for name in ("FreightLink", "FreightDeposit", "200", "Construction", "LINE_CAP_LOW", "50"):
        if name == "LINE_CAP_LOW":
            if "3" not in src or "Low" not in src:
                errs.append("Wayfreight missing Low line cap")
        elif name == "50":
            if "50" not in src:
                errs.append("Wayfreight missing stack cap 50")
        elif name not in src:
            errs.append(f"Wayfreight missing {name}")
    if "not a new house" not in src.lower() and "not a new house" not in src:
        # phrasing may vary
        if "This is not a new house" not in src and "not a new house" not in src:
            errs.append("Wayfreight should say it is not a new house")
    if npcs.get("wayfreight", {}).get("service") != "freight":
        errs.append("Freighter Wend missing or service != freight")
    if "ResetFreight()" not in feat:
        errs.append("missing ResetFreight wiring")
    if "FreightCanPull" not in read("Source/Aerondel/Features/Ventures_Raid.cpp"):
        errs.append("Hold forge does not pull from the freight pool")

if exists("Source/Aerondel/Features/Ventures_Waycamp.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Waycamp.cpp")
    for name in ("CampCraft", "CampPlace", "CampPack", "CampRest", "Larkhaven", "Claimridge", "Greenstead"):
        if name not in src:
            errs.append(f"Waycamp missing {name}")
    if "one at a time" not in src.lower() and "One at a time" not in src and "already have a camp" not in src:
        errs.append("Waycamp should allow only one camp")
    if npcs.get("waycamp", {}).get("service") != "camp":
        errs.append("Campwright Noll missing or service != camp")
    if "Waycamp kit" not in items:
        errs.append("items.json missing Waycamp kit")
    if "ResetCamp()" not in feat:
        errs.append("missing ResetCamp wiring")
    raids = read("Source/Aerondel/Features/Ventures_Holdraids.cpp")
    if "camp" in raids.lower() and ("CampChest" in raids or "s.chest" in raids or "CampPlaced" in raids):
        errs.append("Holdraids must not touch a Waycamp")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 18 wiring and cook-safe ini.")
