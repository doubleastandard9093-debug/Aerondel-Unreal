#!/usr/bin/env python3
"""Offline checks for the Phase 19 Unreal port (Skywyrms, Veilcraft)."""
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

if exists("Source/Aerondel/Features/Ventures_Skywyrms.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Skywyrms.cpp")
    for name in ("WyrmOffer", "WyrmPass", "WyrmFight", "WyrmGo", "Hunter", "Prayer", "Ashwyrm", "Colossus"):
        if name not in src:
            errs.append(f"Skywyrms missing {name}")
    if "never more than one" not in src and "at a time" not in src:
        errs.append("Skywyrms should allow only one sighting at a time")
    if npcs.get("skywyrms", {}).get("service") != "wyrms":
        errs.append("Wyrmwatcher Cald missing or service != wyrms")
    if "ResetWyrms()" not in feat:
        errs.append("missing ResetWyrms wiring")
    if "TickWyrm()" not in feat:
        errs.append("missing TickWyrm wiring")
    if "DiedWyrm" not in feat:
        errs.append("DiedMore does not notify Skywyrms")
    coli = read("Source/Aerondel/Features/ColossiFeature.cpp")
    if "ashwyrm" not in coli.lower() and "Ashwyrm" not in coli:
        errs.append("Ashwyrm missing from Colossi (must stay on its route)")

if exists("Source/Aerondel/Features/Ventures_Veilcraft.cpp"):
    src = read("Source/Aerondel/Features/Ventures_Veilcraft.cpp")
    for name in ("VeilUnmake", "VeilBind", "VeilUnbind", "frost", "Clear essence"):
        if name not in src:
            errs.append(f"Veilcraft missing {name}")
    if "Facet" not in src and "facet" not in src:
        errs.append("Veilcraft should say it is not a Facet")
    if "damage percent" not in src:
        errs.append("Veilcraft should say it is not a damage percent")
    if npcs.get("veilcraft", {}).get("service") != "veil":
        errs.append("Veilwright Ise missing or service != veil")
    if "ResetVeil()" not in feat:
        errs.append("missing ResetVeil wiring")
    hook = read("Source/Aerondel/Features/VenturesFeature.cpp")
    if "HitVeil" not in hook:
        errs.append("HookPlayerHit does not apply Veilcraft on-hit effects")
    if '"souls"' not in src and "souls" not in src:
        errs.append("Veilcraft should own the soul pouch (veil.souls)")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 19 wiring and cook-safe ini.")
