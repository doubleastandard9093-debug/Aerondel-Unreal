#!/usr/bin/env python3
"""Offline checks for the Phase 20 Unreal port (Shadework, Wardfine)."""
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

if exists("Source/Aerondel/Features/Ventures_Shadework.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Shadework.cpp")
    for name in ("ShadeEnter", "ShadeNotice", "ShadeArmed", "HitShade", "0.2", "Guardbreak", "Stillwatch", "True Aim"):
        if name not in src:
            errs.append(f"Shadework missing {name}")
    if "lock" not in src.lower() and "cannot lock" not in src:
        errs.append("Shadework should say bosses cannot be locked")
    if npcs.get("shadework", {}).get("service") != "shade":
        errs.append("Shadeguide Vess missing or service != shade")
    if "ResetShade()" not in feat:
        errs.append("missing ResetShade wiring")
    if "HitShade" not in feat:
        errs.append("HookPlayerHit does not apply Shade hits")
    if "ShadeBreakLine" not in read("Source/Aerondel/Features/FlowFeature.cpp"):
        errs.append("Dodge roll does not break a foe's notice")
    if "ShadeNotice" not in read("Source/Aerondel/Features/Ventures_Guardbreak.cpp"):
        errs.append("Incoming hits do not notice the player")

if exists("Source/Aerondel/Features/Ventures_Wardfine.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Wardfine.cpp")
    for name in ("FineOnFail", "FinePay", "FineWait", "FineResist", "Larkhaven", "unique"):
        if name not in src and name != "unique":
            errs.append(f"Wardfine missing {name}")
        elif name == "unique" and "unique" not in src.lower() and "Unique" not in src:
            errs.append("Wardfine should never take unique items")
    if "prison" in src.lower() and "not a prison" not in src.lower() and "This is not a prison" not in src:
        pass
    if npcs.get("wardfine", {}).get("service") != "keep":
        errs.append("Wardclerk Penn missing or service != keep")
    if "ResetKeep()" not in feat:
        errs.append("missing ResetKeep wiring")
    thieve = read("Source/Aerondel/Activities/ActivityActors.cpp")
    if "FineOnFail" not in thieve:
        errs.append("Pickpocket fail does not raise a Wardfine")
    if "StunUntil" not in thieve:
        errs.append("Pickpocket fail must still stun")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 20 wiring and cook-safe ini.")
