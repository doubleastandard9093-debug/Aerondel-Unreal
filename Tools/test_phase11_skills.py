#!/usr/bin/env python3
"""Offline checks for the Phase 11 Unreal port (Construction, Lookplates, Vocations, Wordcries, Townsteads)."""
import json, os, sys
ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

def read(rel):
    return open(os.path.join(ROOT, rel), encoding="utf-8").read()

skills = json.load(open(os.path.join(ROOT, "Content/Data/skills.json")))
if "Construction" not in skills["skills"]:
    errs.append("Construction missing from skills.json skills list")
cp = skills["perks"].get("Construction") or []
if len(cp) < 57:
    errs.append(f"Construction perks {len(cp)}, expected 57 (to 1000)")
if not any(p.get("name") == "Square Frame" and p.get("level") == 25 for p in cp):
    errs.append("Construction milestone Square Frame @ 25 missing")
if not any(p.get("name") == "Construction Ascendant" for p in cp):
    errs.append("Construction Ascendant missing")

src = read("Source/Aerondel/Data/SkillDef.cpp")
if 'TEXT("construction")' not in src:
    errs.append("MaxLevelFor does not list construction")

loader = read("Source/Aerondel/Data/JsonContentLoader.cpp")
if "Construction" not in loader or "MaxLevel = 1000" not in loader:
    errs.append("JsonContentLoader does not cap Construction at 1000")

ini = read("Config/DefaultScalability.ini")
for k in ("r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod", "r.AntiAliasingMethod", "r.Nanite"):
    if k in ini:
        errs.append(f"forbidden cvar {k} in DefaultScalability.ini")

proj = json.load(open(os.path.join(ROOT, "Aerondel.uproject")))
for p in proj["Plugins"]:
    if p["Name"] == "VisualStudioTools" and p.get("Enabled"):
        errs.append("VisualStudioTools is enabled")

feat = read("Source/Aerondel/Features/VenturesFeature.cpp")
hdr = read("Source/Aerondel/Features/VenturesFeature.h")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Vocations.cpp")):
    voc = read("Source/Aerondel/Features/Ventures_Vocations.cpp")
    for name in ("VocSwap", "warder", "longbow", "leysage", "hearthcant", "swash", "huntward"):
        if name not in voc:
            errs.append(f"Ventures_Vocations.cpp missing {name}")
    if "ResetVoc()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetVoc wiring")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Wordcries.cpp")):
    cries = read("Source/Aerondel/Features/Ventures_Wordcries.cpp")
    for name in ("CryLearn", "CryUse", "Stormheave", "Rimehold", "Windstride", "Heartencry"):
        if name not in cries:
            errs.append(f"Ventures_Wordcries.cpp missing {name}")
    if "ResetCry()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetCry wiring")

if os.path.exists(os.path.join(ROOT, "Source/Aerondel/Features/Ventures_Townsteads.cpp")):
    steads = read("Source/Aerondel/Features/Ventures_Townsteads.cpp")
    for name in ("SteadBuy", "SteadPlace", "SteadRest", "lark-e", "Construction"):
        if name not in steads:
            errs.append(f"Ventures_Townsteads.cpp missing {name}")
    if "ResetStead()" not in feat:
        errs.append("VenturesFeature.cpp missing ResetStead wiring")

if "Lookplates" in feat or "LookPlateCount" in hdr:
    if "LookPlateCount" not in hdr and "i < 8" not in feat:
        errs.append("Wardrobe/Lookplates does not iterate 8 plates")
    if 'TEXT("weapon")' not in feat:
        errs.append("Lookplates/wardrobe missing weapon slot")

if errs:
    for e in errs:
        print("FAIL", e)
    sys.exit(1)
print(f"OK: Construction {len(cp)} perks, cap wiring in place, cook-safe ini.")
