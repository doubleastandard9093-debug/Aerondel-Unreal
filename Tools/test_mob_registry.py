#!/usr/bin/env python3
"""Offline checks for the live-mob registry (Combat/AerondelMobRegistry): mobs register in BeginPlay, leave in EndPlay,
and the periodic queries read the registry instead of TActorIterator<AAerondelMob>."""
import re, sys, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
S = lambda p: (root / "Source/Aerondel" / p).read_text(encoding="utf-8", errors="replace")
errs = []
mob = S("Combat/AerondelMob.cpp")
if "RegisterMob(this)" not in mob[mob.find("AAerondelMob::BeginPlay"):][:600]:
    errs.append("AAerondelMob::BeginPlay must RegisterMob(this)")
ep = mob[mob.find("AAerondelMob::EndPlay"):][:400]
if "UnregisterMob(this)" not in ep or "Super::EndPlay" not in ep:
    errs.append("AAerondelMob::EndPlay must UnregisterMob(this) and call Super::EndPlay")
if "virtual void EndPlay(" not in S("Combat/AerondelMob.h"):
    errs.append("AAerondelMob.h must declare EndPlay")
def body(src, sig):
    i = src.find(sig)
    if i < 0: return None
    j = src.find("{", i); d = 0
    for k in range(j, len(src)):
        d += src[k] == "{"; d -= src[k] == "}"
        if d == 0: return src[j:k]
for f, sig in [("World/AerondelRelevance.cpp", "UAerondelRelevanceSubsystem::Tick("), ("Features/VenturesFeature.cpp", "UVenturesFeature::MobsNear("),
               ("Features/Ventures_Shadework.cpp", "UVenturesFeature::ShadeFoesNear("), ("Features/Ventures_Oathframe.cpp", "UVenturesFeature::OathNearest("),
               ("Features/BossMechFeature.cpp", "UBossMechFeature::OnGameTick(")]:
    b = body(S(f), sig)
    if b is None: errs.append(f"{sig} not found in {f}"); continue
    if "TActorIterator<AAerondelMob>" in b: errs.append(f"{sig} still walks TActorIterator<AAerondelMob>")
    if "AerondelMobs::Live(" not in b: errs.append(f"{sig} should read AerondelMobs::Live")
    if '#include "Combat/AerondelMobRegistry.h"' not in S(f): errs.append(f"{f} must include Combat/AerondelMobRegistry.h")
if errs:
    print("FAIL " + "\nFAIL ".join(errs)); sys.exit(1)
print("OK: mob registry wiring in place (5 periodic queries off TActorIterator).")
