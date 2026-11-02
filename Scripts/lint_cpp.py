#!/usr/bin/env python3
"""Static sanity checks for the Aerondel C++ (no engine headers needed).
Checks: brace/paren balance, .generated.h include name + position, reflection macro/GENERATED_BODY pairing,
own-header-first in .cpp, project-relative includes resolve, UCLASS name prefixes, #pragma once."""
import os, re, sys

ROOT = os.path.join(os.path.dirname(__file__), "..", "Source", "Aerondel")
errors = []


def strip(src):
    src = re.sub(r"//[^\n]*", "", src)
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r'"(\\.|[^"\\])*"', '""', src)
    src = re.sub(r"'(\\.|[^'\\])'", "''", src)
    return src


files = []
for d, _, fs in os.walk(ROOT):
    for f in fs:
        if f.endswith((".h", ".cpp")):
            files.append(os.path.join(d, f))

for path in sorted(files):
    rel = os.path.relpath(path, ROOT)
    raw = open(path, encoding="utf-8").read()
    code = strip(raw)
    for o, c in ("{}", "()", "[]"):
        if code.count(o) != code.count(c):
            errors.append(f"{rel}: unbalanced {o}{c} ({code.count(o)} vs {code.count(c)})")
    base = os.path.splitext(os.path.basename(path))[0]
    includes = re.findall(r'^\s*#include\s+"([^"]+)"', raw, re.M)
    if path.endswith(".h"):
        if "#pragma once" not in raw:
            errors.append(f"{rel}: missing #pragma once")
        reflected = re.findall(r"^\s*(UCLASS|USTRUCT|UINTERFACE|UENUM)\s*\(", raw, re.M)
        gen = [i for i in includes if i.endswith(".generated.h")]
        if reflected and not any(k in ("UCLASS", "USTRUCT", "UINTERFACE") for k in reflected) and not gen:
            errors.append(f"{rel}: UENUM-only header without .generated.h")
        if reflected:
            if gen != [f"{base}.generated.h"]:
                errors.append(f"{rel}: expected #include \"{base}.generated.h\", found {gen}")
            elif includes[-1] != gen[0]:
                errors.append(f"{rel}: .generated.h must be the last #include")
        n_types = len(re.findall(r"^\s*(UCLASS|USTRUCT|UINTERFACE)\s*\(", raw, re.M))
        n_body = len(re.findall(r"GENERATED_(BODY|UCLASS_BODY|USTRUCT_BODY)\s*\(\)", raw))
        # each UINTERFACE needs two GENERATED_BODY (U and I class)
        expected = n_types + len(re.findall(r"^\s*UINTERFACE\s*\(", raw, re.M))
        if n_body != expected:
            errors.append(f"{rel}: {n_types} reflected types but {n_body} GENERATED_BODY (expected {expected})")
        for m in re.finditer(r"UCLASS\([^)]*\)\s*class\s+(?:\w+_API\s+)?(\w+)\s*:\s*public\s+(\w+)", raw):
            name, parent = m.groups()
            if parent.startswith("A") and not name.startswith("A"):
                errors.append(f"{rel}: actor class {name} should start with A")
            if parent.startswith("U") and not name.startswith("U"):
                errors.append(f"{rel}: object class {name} should start with U")
        for m in re.finditer(r"USTRUCT\([^)]*\)\s*struct\s+(?:\w+_API\s+)?(\w+)", raw):
            if not m.group(1).startswith("F"):
                errors.append(f"{rel}: struct {m.group(1)} should start with F")
        for m in re.finditer(r"UENUM\([^)]*\)\s*enum\s+class\s+(\w+)", raw):
            if not m.group(1).startswith("E"):
                errors.append(f"{rel}: enum {m.group(1)} should start with E")
        if re.search(r"UPROPERTY\([^)]*\)\s*\w+\s*\*", code):
            errors.append(f"{rel}: raw UObject* UPROPERTY (use TObjectPtr)")
    else:
        own = os.path.relpath(path, ROOT)[:-4] + ".h"
        if os.path.exists(os.path.join(ROOT, own)) and (not includes or includes[0] != own.replace(os.sep, "/")):
            if not rel.startswith("Aerondel.cpp"):
                errors.append(f"{rel}: first include should be \"{own}\"")
    for inc in includes:
        if inc.endswith(".generated.h"):
            continue
        top = inc.split("/")[0]
        if top in ("Core", "Data", "Skills", "Inventory", "Bank", "Quests", "Interaction", "Player", "XR", "Combat", "World", "UI", "Save", "Tests") or inc == "Aerondel.h":
            if not os.path.exists(os.path.join(ROOT, inc)):
                errors.append(f"{rel}: include not found: {inc}")

print(f"Checked {len(files)} files.")
for e in errors:
    print("ERROR", e)
sys.exit(1 if errors else 0)
