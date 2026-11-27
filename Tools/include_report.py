#!/usr/bin/env python3
"""Project include graph for Aerondel-Unreal: per .cpp TU, transitive project headers (count, lines) and which heavy engine headers
they pull in through project headers. Usage: python3 Tools/include_report.py . [--json out]"""
import re, sys, json, pathlib
repo = pathlib.Path(sys.argv[1]); src = repo / "Source/Aerondel"
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
HEAVY = ["Engine/GameInstance.h", "Dom/JsonObject.h", "Engine/HitResult.h", "Features/VenturesFeature.h", "Features/AerondelFeature.h"]
files = {p: p.read_text(encoding="utf-8", errors="replace") for p in src.rglob("*") if p.suffix in (".h", ".cpp")}
def resolve(frm, inc):
    for c in (src / inc, frm.parent / inc):
        if c in files: return c
    return None
def walk(p):
    seen, eng, stack = set(), set(), [p]
    while stack:
        f = stack.pop()
        for inc in INC.findall(files[f]):
            r = resolve(f, inc)
            if r is None:
                if f != p or True: eng.add(inc)
                continue
            if r not in seen: seen.add(r); stack.append(r)
    return seen, eng
out = {}
for p in sorted(files):
    if p.suffix != ".cpp": continue
    hs, eng = walk(p)
    rel = lambda x: str(x.relative_to(src)).replace("\\", "/")
    names = {rel(h) for h in hs} | eng
    out[rel(p)] = {"hdrs": len(hs), "lines": sum(files[h].count("\n") for h in hs), "heavy": [h for h in HEAVY if h in names]}
tus = len(out)
summ = {"tus": tus, "proj_hdr_lines_total": sum(v["lines"] for v in out.values()),
        **{h: sum(1 for v in out.values() if h in v["heavy"]) for h in HEAVY}}
print(json.dumps(summ))
if "--json" in sys.argv: json.dump(out, open(sys.argv[sys.argv.index("--json") + 1], "w"), indent=0)
