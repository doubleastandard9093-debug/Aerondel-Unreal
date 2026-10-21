#!/usr/bin/env python3
"""Validates the .uproject JSON, ini syntax and the XP curve (independent recomputation)."""
import json, math, os, re, sys, configparser

ROOT = os.path.join(os.path.dirname(__file__), "..")
errs = []

proj = json.load(open(os.path.join(ROOT, "Gielinor.uproject")))
plugins = {p["Name"]: p["Enabled"] for p in proj["Plugins"]}
for need in ("OpenXR", "OpenXRHandTracking", "EnhancedInput", "CommonUI", "ModelingToolsEditorMode", "PythonScriptPlugin", "EditorScriptingUtilities"):
    if not plugins.get(need):
        errs.append(f"plugin {need} not enabled")
if proj["Modules"][0]["Name"] != "Gielinor":
    errs.append("module name mismatch")
print("uproject OK:", proj["EngineAssociation"], sorted(k for k, v in plugins.items() if v))

for d, _, fs in os.walk(os.path.join(ROOT, "Config")):
    for f in fs:
        if not f.endswith(".ini"):
            continue
        p = os.path.join(d, f)
        section = None
        for n, line in enumerate(open(p, encoding="utf-8"), 1):
            s = line.strip()
            if not s or s.startswith(";"):
                continue
            if s.startswith("["):
                if not re.fullmatch(r"\[[^\]]+\]", s):
                    errs.append(f"{f}:{n} bad section {s}")
                section = s
                continue
            if section is None:
                errs.append(f"{f}:{n} key outside a section")
            if not re.match(r"^[+\-.!]?[\w.\[\]@:]+\s*=", s):
                errs.append(f"{f}:{n} not key=value: {s[:60]}")
            if s.count("(") != s.count(")"):
                errs.append(f"{f}:{n} unbalanced parentheses")
        print("ini OK:", os.path.relpath(p, ROOT))

pts, xp = 0, {1: 0}
for l in range(1, 99):
    pts += math.floor(l + 300 * 2 ** (l / 7))
    xp[l + 1] = pts // 4
assert xp[2] == 83 and xp[10] == 1154 and xp[99] == 13034431, xp[99]
print("XP curve OK: L2=83 L10=1154 L99=13,034,431")

for e in errs:
    print("ERROR", e)
sys.exit(1 if errs else 0)
