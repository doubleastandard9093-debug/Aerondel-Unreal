#!/usr/bin/env python3
"""Offline checks for the Phase 13 Unreal port (Facet Stones, Leyweave)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
for k in ("Dull facet", "Facet frame", "Twin-cut Facet", "Storm-glass Facet", "Ironroot Facet", "Wide-reap Facet", "Trail-disc Facet"):
    if k not in items:
        errs.append(f"missing item {k}")

recs = json.load(open(os.path.join(ROOT, "Content/Data/recipes.json")))["recipes"]
facet = [r for r in recs if r.get("group") == "Facet Stones"]
if len(facet) != 11:
    errs.append(f"Facet Stone recipes {len(facet)}, expected 11")
if not any(r.get("output") == "Facet frame" and r.get("level") == 30 for r in facet):
    errs.append("Craft Facet frame recipe missing")

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
if npcs.get("facetier", {}).get("service") != "facets":
    errs.append("Facetier Vesa missing or service != facets")

ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")

proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
combat = read("Source/Aerondel/Combat/PlayerCombatComponent.cpp")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Facets.cpp")):
    src = read("Source/Aerondel/Features/Ventures_Facets.cpp")
    for name in ("FacetSocket", "FacetOn", "echo", "whirl", "Dull facet"):
        if name not in src:
            errs.append(f"Ventures_Facets.cpp missing {name}")
    if "ResetFacet()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetFacet wiring")
    if "WhirlBonusM" not in combat or "StunBonusTicks" not in combat:
        errs.append("PlayerCombatComponent missing Facet whirl/stun hooks")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Leyweave.cpp")):
    src = read("Source/Aerondel/Features/Ventures_Leyweave.cpp")
    for name in ("LeyFill", "LeyKey", "seasonkeep", "GreenLeyGrowBonus", "SteadLeyBuildBonus"):
        if name not in src:
            errs.append(f"Ventures_Leyweave.cpp missing {name}")
    if "ResetLey()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetLey wiring")
    if npcs.get("leyweaver", {}).get("service") != "ley":
        errs.append("Leyweaver Ione missing or service != ley")
    # stubs must have been replaced
    if "float UVenturesFeature::SteadLeyBuildBonus() const { return 0.f; }" in read("Source/Aerondel/Features/Ventures_Townsteads.cpp"):
        errs.append("SteadLeyBuildBonus still the Phase-12 stub")
    if "float UVenturesFeature::GreenLeyGrowBonus() const { return 0.f; }" in read("Source/Aerondel/Features/Ventures_Greenstead.cpp"):
        errs.append("GreenLeyGrowBonus still the Phase-12 stub")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Facet items/recipes/Vesa and cook-safe ini in place.")
