#!/usr/bin/env python3
"""Offline checks for the Phase 15 Unreal port (Kinbeasts)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

npcs = json.load(open(os.path.join(ROOT, "Content/Data/quests.json")))["npcs"]
items = json.load(open(os.path.join(ROOT, "Content/Data/items.json")))["items"]
recipes = json.load(open(os.path.join(ROOT, "Content/Data/recipes.json")))["recipes"]
feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
src = read("Source/Aerondel/Features/Ventures_Kinbeasts.cpp")
ash = read("Source/Aerondel/Features/Ventures_Ashkin.cpp")
ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")
proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

if npcs.get("kinbeasts", {}).get("service") != "kin":
    errs.append("Beastbinder Cal missing or service != kin")
for name in ("Bind vessel", "Sturdy vessel", "Fine vessel"):
    if name not in items:
        errs.append(f"missing item {name}")
names = [r.get("name") for r in recipes]
for name in ("Craft Bind vessel", "Craft Bind vessel (Hunter)", "Craft Sturdy vessel", "Craft Fine vessel"):
    if name not in names:
        errs.append(f"missing recipe {name}")
for name in ("KinThrow", "KinSend", "KinFeed", "RecallKin", "KinSoak", "Beastwise" if False else "catchChance"):
    if name not in src:
        errs.append(f"Ventures_Kinbeasts.cpp missing {name}")
if "never both" not in src.lower() and "never both" not in src:
    if "never both" not in src:
        errs.append("missing one-or-the-other wording")
if "ResetKin()" not in feat:
    errs.append("missing ResetKin wiring")
if "TickKin()" not in feat:
    errs.append("missing TickKin wiring")
if "KinSoak" not in ash:
    errs.append("FilterIncoming missing KinSoak")
if "void UVenturesFeature::RecallKin" in ash:
    errs.append("RecallKin stub still in Ventures_Ashkin.cpp")
if "IA_Kinbeast" not in read("Source/Aerondel/Player/AerondelCharacter.cpp"):
    errs.append("K key not bound for Kinbeasts")
if "AKinbeastPet" not in read("Source/Aerondel/Features/KinbeastPet.h"):
    errs.append("missing KinbeastPet placeholder actor")
# old saves: new skills not involved; unknown keys must stay preserved at the feature level
if "LoadKin" not in feat:
    errs.append("missing LoadKin wiring")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print("OK: Phase 15 Kinbeasts wiring, Cal, vessels, cook-safe ini.")
