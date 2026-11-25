#!/usr/bin/env python3
"""Catch UE 5.8 compile pitfalls that a Linux syntax pass will not see:

  * FMath::RoundToInt / FloorToInt / CeilToInt / TruncToInt (and *64) called
    on an integer-typed argument — MSVC C2668, ambiguous float vs double overload.
  * const TCHAR* / TCHAR[] compared to a TEXT() literal with == or != — MSVC C4130
    (pointer comparison against the address of a string constant). FString and FName
    comparisons are valid and are not flagged.
  * A local, parameter, or lambda in a method that hides a class member — MSVC C4458.
  * A .cpp that dereferences a type (UNpcDef, FJsonObject, UGameInstance, FHitResult,
    and project U/A/F types) without including the header that defines it.

Usage: python3 Tools/check_ue58_pitfalls.py [Source/Aerondel]
"""
import glob, os, re, sys
from collections import defaultdict

root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', 'Source', 'Aerondel')
root = os.path.normpath(root) + os.sep

INT_TYPES = {
    'int', 'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64',
    'INT', 'INT32', 'INT64', 'UINT', 'UINT32', 'UINT64', 'SIZE_T', 'size_t',
    'ANSICHAR', 'WIDECHAR', 'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t',
    'bool', 'UPARAM',
}
FLOAT_TYPES = {'float', 'double', 'FLOAT', 'DOUBLE', 'FVector', 'FVector2D', 'FVector4',
               'FRotator', 'FQuat', 'FLinearColor', 'FColor', 'FTransform'}
FLOAT_MARK = re.compile(
    r'\d+\.\d|\d+\.[fF]\b|\d+[fF]\b|\.\d+|\bPI\b|\bFRand(?:Range)?\s*\(|'
    r'\b(?:Atan2|Sin|Cos|Tan|Asin|Acos|Sqrt|Pow|Exp|Loge|Size(?:2D|Squared)?)\s*\('
)
ROUND_FN = re.compile(r'FMath::((?:Round|Floor|Ceil|Trunc)ToInt(?:64)?)\s*\(')
TYPE_DECL = re.compile(
    r'\b((?:const\s+)?(?:static\s+)?(?:constexpr\s+)?(?:mutable\s+)?'
    r'(?:class\s+|struct\s+|enum\s+class\s+|enum\s+)?'
    r'[\w:]+(?:\s*<[^;{}<>]*>)?)\s*(\*+|&)?\s*(?:const\s+)?(\w+)\s*'
    r'(?:\[[^\]]*\])?\s*(?:=|;|,|\)$|\()')
INC_RE = re.compile(r'^\s*#include\s+"([^"]+)"', re.M)
CLASS_RE = re.compile(r'\b(class|struct)\s+(?:\w+_API\s+)?(\w+)\b[^{;]*\{')
METH_DEF = re.compile(
    r'^[\w:<>,\s\*&]*?\b([UAF][A-Z]\w+|F[A-Z]\w+)\s*::\s*(~?\w+)\s*\(([^;{]*)\)\s*'
    r'(?:const\s*)?(?:override\s*)?\{', re.M)
# Free/static functions (for TCHAR/RoundToInt locals inside file-scope helpers).
FREE_DEF = re.compile(
    r'^(?:static\s+)?[\w:<>,\s\*&]*?\b(\w+)\s*\(([^;{]*)\)\s*(?:const\s*)?\{', re.M)
KW = {
    'return', 'if', 'else', 'for', 'while', 'switch', 'case', 'break', 'continue', 'goto',
    'sizeof', 'typedef', 'using', 'namespace', 'public', 'private', 'protected', 'class',
    'struct', 'enum', 'union', 'template', 'typename', 'const', 'static', 'virtual',
    'mutable', 'constexpr', 'volatile', 'throw', 'try', 'catch', 'this', 'true', 'false',
    'nullptr', 'new', 'delete', 'operator', 'sizeof', 'alignas', 'noexcept', 'override',
    'final', 'explicit', 'friend', 'void', 'int', 'float', 'double', 'bool', 'auto',
    'unsigned', 'signed', 'long', 'short', 'char', 'wchar_t',
}

ENGINE_HEADERS = {
    'FJsonObject': 'Dom/JsonObject.h',
    'FJsonValue': 'Dom/JsonValue.h',
    'UGameInstance': 'Engine/GameInstance.h',
    'FHitResult': 'Engine/HitResult.h',
}

JSON_USE = re.compile(
    r'(?:->|\\.)\s*(?:Get|Set|TryGet|Has|Remove)\w*Field\s*\(|'
    r'MakeShared\s*<\s*FJsonObject\s*>|'
    r'\bFJsonObject\s+\w+\s*[;{=]'
)
GI_USE = re.compile(
    r'GetGameInstance\s*\(\s*\)\s*->|'
    r'\b(?:GI|GameInstance)\s*->\s*(?:GetSubsystem|GetWorld|GetTimerManager|GetEngine)|'
    r'\bUGameInstance\s+\w+\s*[;{=]'
)
HIT_USE = re.compile(
    r'\bFHitResult\s+\w+\s*[;={]|'
    r'\b\w+\s*(?:\.|->)\s*(?:ImpactPoint|ImpactNormal|bBlockingHit|GetActor\s*\(|'
    r'GetComponent\s*\(|BoneName|TraceStart|TraceEnd|PenetrationDepth|PhysMaterial)\b'
)
NPC_USE = re.compile(
    r'Definition\s*->\s*(?:Service|IdleLines|Home|bHasHome|Category)\b'
)


def read(path):
    return open(path, encoding='utf-8', errors='ignore').read()


def rel(path):
    return path.replace(root, '').replace('\\', '/')


def strip_comments(s):
    out = []
    i, n = 0, len(s)
    while i < n:
        if s[i] == '/' and i + 1 < n and s[i + 1] == '/':
            while i < n and s[i] != '\n':
                i += 1
            continue
        if s[i] == '/' and i + 1 < n and s[i + 1] == '*':
            j = s.find('*/', i + 2)
            if j < 0:
                break
            # keep newlines so line numbers stay stable
            out.append('\n' * s[i:j].count('\n'))
            i = j + 2
            continue
        out.append(s[i]); i += 1
    return ''.join(out)


def extract_body(text, brace_idx):
    """Return the text inside the { } that starts at brace_idx, and the index after it."""
    i, d = brace_idx + 1, 1
    n = len(text)
    while d and i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == '\\':
                    i += 1
                i += 1
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            if j < 0:
                break
            i = j + 2
            continue
        if c == '{':
            d += 1
        elif c == '}':
            d -= 1
        i += 1
    return text[brace_idx + 1:i - 1], i


def flatten_depth0_strip_parens(body):
    """Class-body text at brace-depth 0, with function parameter lists removed
    so `int32 Count = 1` inside a signature is not taken as a data member."""
    depth = 0
    pdepth = 0
    out = []
    i, n = 0, len(body)
    while i < n:
        c = body[i]
        if c == '"':
            if depth == 0 and pdepth == 0:
                out.append(c)
            i += 1
            while i < n and body[i] != '"':
                if depth == 0 and pdepth == 0:
                    out.append(body[i])
                if body[i] == '\\':
                    i += 1
                    if i < n and depth == 0 and pdepth == 0:
                        out.append(body[i])
                i += 1
            if i < n:
                if depth == 0 and pdepth == 0:
                    out.append(body[i])
                i += 1
            continue
        if c == '{':
            depth += 1
            i += 1
            continue
        if c == '}':
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth == 0 and c == '(':
            pdepth += 1
            out.append(c)  # keep '(' so method names still match Type Name(
            i += 1
            continue
        if depth == 0 and c == ')' and pdepth:
            pdepth -= 1
            out.append(c)
            i += 1
            continue
        if depth == 0 and pdepth == 0:
            out.append(c)
        elif depth == 0 and pdepth:
            # keep newlines only
            if c == '\n':
                out.append(c)
        i += 1
    return ''.join(out)


def is_int_type(ty):
    t = re.sub(r'\b(?:const|mutable|static|constexpr|volatile|class|struct)\b', ' ', ty)
    t = re.sub(r'\s+', ' ', t).strip()
    t = t.split('<')[0].strip()
    t = t.replace('&', '').replace('*', '').strip()
    t = t.split('::')[-1]
    return t in INT_TYPES


def is_float_type(ty):
    t = re.sub(r'\b(?:const|mutable|static|constexpr|volatile|class|struct)\b', ' ', ty)
    t = re.sub(r'\s+', ' ', t).strip()
    t = t.split('<')[0].strip().replace('&', '').replace('*', '').strip().split('::')[-1]
    return t in FLOAT_TYPES or t in ('float', 'double')


def norm_type(ty):
    t = re.sub(r'\b(?:const|mutable|static|constexpr|volatile|class|struct)\b', ' ', ty)
    t = re.sub(r'\s+', ' ', t).strip()
    stars = t.count('*')
    t = t.replace('&', '').replace('*', '').strip()
    t = t.split('<')[0].strip().split('::')[-1]
    return t, stars


# --------------- load sources ---------------
cpp_files = sorted(glob.glob(root + '**/*.cpp', recursive=True))
hdr_files = sorted(glob.glob(root + '**/*.h', recursive=True))
all_files = hdr_files + cpp_files
texts = {f: read(f) for f in all_files}
clean = {f: strip_comments(t) for f, t in texts.items()}

# type -> defining header (relative, posix)
type_header = dict(ENGINE_HEADERS)
for f in hdr_files:
    t = clean[f]
    for m in re.finditer(
            r'^\s*(?:class|struct)\s+(?:\w+_API\s+)?([UAF][A-Z]\w+)\b[^{\n;]*(?::|\{)', t, re.M):
        type_header.setdefault(m.group(1), rel(f))

# include index
hdr_by_name = {}
for f in hdr_files:
    r = rel(f)
    hdr_by_name[r] = f
    hdr_by_name[os.path.basename(f)] = f
    hdr_by_name[r.replace('\\', '/')] = f


def resolve_inc(inc, from_path):
    inc = inc.replace('\\', '/')
    if inc in hdr_by_name:
        return hdr_by_name[inc]
    cand = os.path.normpath(os.path.join(os.path.dirname(from_path), inc))
    if os.path.isfile(cand):
        return cand
    cand = os.path.normpath(os.path.join(root, inc))
    if os.path.isfile(cand):
        return cand
    return None


def include_closure(path):
    """Quoted includes (project + engine paths written as quotes), recursively."""
    seen_files = set()
    inc_names = set()
    q = [path]
    while q:
        p = q.pop()
        if p in seen_files:
            continue
        seen_files.add(p)
        src = texts.get(p)
        if src is None:
            if not os.path.isfile(p):
                continue
            src = read(p)
        for inc in INC_RE.findall(src):
            inc = inc.replace('\\', '/')
            inc_names.add(inc)
            r = resolve_inc(inc, p)
            if r is not None:
                q.append(r)
    return inc_names, seen_files


def header_present(incs, name):
    name = name.replace('\\', '/')
    base = os.path.basename(name)
    for i in incs:
        if i == name or i.endswith('/' + name) or i == base or i.endswith('/' + base):
            return True
    return False


# --------------- parse classes / fields ---------------
# class -> {member names that are data or methods}
members = defaultdict(set)
# (struct, field) -> type string
field_ty = {}
# struct -> {field: type} for TCHAR* fields
tchar_fields = defaultdict(dict)  # struct -> field -> type

DATA_RE = re.compile(
    r'(?:^|\n)\s*(?:mutable\s+)?(?:static\s+)?(?:constexpr\s+)?(?:const\s+)?'
    r'(?!public\b|private\b|protected\b|UFUNCTION\b|UPROPERTY\b|GENERATED_BODY\b)'
    r'([\w:]+(?:\s*<[^;{}<>]*>)?)(\s*\*+|\s*&|\s+)(\w+)\s*(?:\[[^\]]*\])?\s*(?:=|;|\{)',
    re.M)
METH_RE = re.compile(
    r'(?:^|\n)\s*(?:virtual\s+)?(?:static\s+)?(?:explicit\s+)?'
    r'(?!public\b|private\b|protected\b|UFUNCTION\b|UPROPERTY\b|UCLASS\b|'
    r'USTRUCT\b|UENUM\b|GENERATED_BODY\b|GENERATED_UCLASS_BODY\b)'
    r'[\w:]+(?:\s*<[^;{}<>]*>)?(?:\s*\*+|\s*&|\s+)\s*(\w+)\s*\(',
    re.M)


def record_class(name, body, path):
    flat = flatten_depth0_strip_parens(body)
    for m in DATA_RE.finditer(flat):
        ty, stars, fn = m.group(1).strip(), m.group(2) or '', m.group(3)
        if fn in KW or fn in ('GENERATED_BODY', 'GENERATED_UCLASS_BODY', 'UPROPERTY',
                              'UFUNCTION', 'UCLASS', 'USTRUCT', 'UENUM'):
            continue
        full = (ty + (' ' + stars if stars else '')).strip()
        members[name].add(fn)
        field_ty[(name, fn)] = full
        if 'TCHAR' in ty and ('*' in (stars or '') or '*' in ty or '[' in m.group(0)):
            tchar_fields[name][fn] = full
        elif 'TCHAR' in ty and stars:
            tchar_fields[name][fn] = full
    for m in METH_RE.finditer(flat):
        fn = m.group(1)
        if fn in KW or fn == name or fn == '~' + name or fn in ('GENERATED_BODY','GENERATED_UCLASS_BODY','UFUNCTION','UPROPERTY','UCLASS','USTRUCT','UENUM','UINTERFACE'):
            continue
        members[name].add(fn)


for f, t in clean.items():
    for m in CLASS_RE.finditer(t):
        brace = t.rfind('{', 0, m.end())
        if brace < 0:
            continue
        body, _ = extract_body(t, brace)
        record_class(m.group(2), body, f)

# Walk `: public Base` so C4458 also sees inherited members (e.g. UAerondelFeature::Coins).
bases = defaultdict(list)
BASE_RE = re.compile(
    r'\b(?:class|struct)\s+(?:\w+_API\s+)?(\w+)\b\s*(?:final\s*)?:\s*([^\{]+)\{')
for f, t in clean.items():
    for m in BASE_RE.finditer(t):
        name = m.group(1)
        for b in m.group(2).split(','):
            b = re.sub(r'\b(?:public|private|protected|virtual)\b', ' ', b)
            b = re.sub(r'\s+', ' ', b).strip().split()[-1] if b.strip() else ''
            if b:
                bases[name].append(b)

def inherited_members(cls, _seen=None):
    if _seen is None:
        _seen = set()
    if cls in _seen:
        return set()
    _seen.add(cls)
    out = set(members.get(cls, ()))
    for b in bases.get(cls, ()):
        out |= inherited_members(b, _seen)
    return out

# TCHAR* also as `const TCHAR* Name` inside structs already covered.
# Catch `const TCHAR Id[]` via a looser pass
TCHAR_ARR = re.compile(r'(?:const\s+)?TCHAR\s+(\w+)\s*\[')
TCHAR_PTR = re.compile(r'(?:const\s+)?TCHAR\s*\*+\s*(?:const\s+)?(\w+)')
for f, t in clean.items():
    for m in CLASS_RE.finditer(t):
        brace = t.rfind('{', 0, m.end())
        if brace < 0:
            continue
        body, _ = extract_body(t, brace)
        name = m.group(2)
        # only depth-0? fields of this struct including simple ones
        for tm in TCHAR_PTR.finditer(body):
            # skip if this is inside a nested function; accept
            tchar_fields[name][tm.group(1)] = 'const TCHAR*'
            field_ty.setdefault((name, tm.group(1)), 'const TCHAR*')
        for tm in TCHAR_ARR.finditer(body):
            tchar_fields[name][tm.group(1)] = 'TCHAR[]'
            field_ty.setdefault((name, tm.group(1)), 'TCHAR[]')

bad = []


def line_of(text, pos):
    return text[:pos].count('\n') + 1


def extract_arg(s, start):
    depth = 0
    i = start
    n = len(s)
    while i < n:
        c = s[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return s[start + 1:i]
        i += 1
    return s[start + 1:]


def func_span_map(text):
    """List of (start, end, class, method, params, body_start, body)."""
    out = []
    covered = []
    for m in METH_DEF.finditer(text):
        brace = text.find('{', m.end() - 1)
        if brace < 0:
            continue
        body, end = extract_body(text, brace)
        out.append((m.start(), end, m.group(1), m.group(2), m.group(3), brace + 1, body))
        covered.append((m.start(), end))
    for m in FREE_DEF.finditer(text):
        # skip Class::Method already captured (and control-flow if {)
        if m.group(1) in KW:
            continue
        pos = m.start()
        if any(a <= pos < b for a, b in covered):
            continue
        # skip if this match is actually a Class::Method (contains :: near end of prefix)
        prefix = text[max(0, m.start()-2):m.start()+m.group(1).__len__()+2] if False else text[m.start():m.start()]
        if '::' in text[m.start():m.end()]:
            continue
        brace = text.find('{', m.end() - 1)
        if brace < 0 or brace > m.end() + 8:
            # METH_DEF already requires {; FREE_DEF does too
            if brace < 0:
                continue
        body, end = extract_body(text, brace)
        out.append((m.start(), end, None, m.group(1), m.group(2), brace + 1, body))
        covered.append((m.start(), end))
    return out


def locals_in(params, body):
    """name -> type for params + locals in a function body."""
    locs = {}
    if params.strip() and params.strip() != 'void':
        for part in params.split(','):
            part = part.strip()
            if not part or part == '...':
                continue
            if '=' in part:
                part = part[:part.index('=')].strip()
            ids = re.findall(r'\b([A-Za-z_]\w*)\b', part)
            if not ids:
                continue
            n = ids[-1]
            typ = part[:part.rfind(n)].strip()
            if n not in KW:
                locs[n] = typ
    # declarations in the body
    for m in re.finditer(
            r'\b((?:const\s+)?(?:static\s+)?(?:constexpr\s+)?'
            r'(?:auto|int32|int64|uint8|uint16|uint32|uint64|int|float|double|bool|'
            r'FString|FName|FText|FVector2D|FVector|FRotator|FIntPoint|'
            r'TCHAR|'
            r'[UAF][A-Z]\w+|'
            r'TArray\s*<[^;{}<>]+>|'
            r'TMap\s*<[^;{}<>]+>|'
            r'TShared(?:Ptr|Ref)\s*<[^;{}<>]+>|'
            r'TObjectPtr\s*<[^;{}<>]+>|'
            r'TWeakObjectPtr\s*<[^;{}<>]+>'
            r'))\s*(\*+|&)?\s*(?:const\s+)?(\w+)\s*(?:=\s*\[|=|;|,|\))',
            body):
        typ = m.group(1) + (' ' + m.group(2) if m.group(2) else '')
        n = m.group(3)
        if n in KW:
            continue
        locs.setdefault(n, typ)
    # for (const T& X : ...)
    for m in re.finditer(
            r'\bfor\s*\(\s*(?:const\s+)?(?:auto(?:\s*\*|\s*&)?|[\w:]+(?:\s*<[^;{}<>]*>)?)\s*'
            r'(\*|&)?\s*(?:const\s+)?(\w+)\s*(?:=|:)',
            body):
        locs.setdefault(m.group(2), 'auto')
    return locs


# ===== 1. integer RoundToInt family =====
for f, t in clean.items():
    spans = func_span_map(t)
    def span_at(pos):
        for s in spans:
            if s[0] <= pos < s[1]:
                return s
        return None
    for m in ROUND_FN.finditer(t):
        arg = extract_arg(t, m.end() - 1)
        if FLOAT_MARK.search(arg):
            continue
        # explicit float/double casts
        if re.search(r'\(\s*(?:float|double)\s*\)', arg):
            continue
        sp = span_at(m.start())
        locs = locals_in(sp[4], sp[6]) if sp else {}
        cls = sp[2] if sp else None
        # classify leaves
        unknown = []
        ints = []
        floats = []
        used_fields = set()
        for dm in re.finditer(r'\b(\w+)\s*(?:\.|->)\s*(\w+)', arg):
            obj, fld = dm.group(1), dm.group(2)
            used_fields.add((obj, fld))
            oty = locs.get(obj)
            resolved = None
            if oty:
                sn, _ = norm_type(oty)
                if (sn, fld) in field_ty:
                    resolved = field_ty[(sn, fld)]
            if resolved is None and cls and (cls, obj) in field_ty:
                # member object
                sn, _ = norm_type(field_ty[(cls, obj)])
                if (sn, fld) in field_ty:
                    resolved = field_ty[(sn, fld)]
            if resolved is None:
                # unique field name across structs?
                cands = [(s, ty) for (s, fn), ty in field_ty.items() if fn == fld]
                if cands and all(is_int_type(ty) for _, ty in cands):
                    resolved = cands[0][1]
                elif cands and all(is_float_type(ty) for _, ty in cands):
                    resolved = cands[0][1]
            if resolved is None:
                unknown.append(f'{obj}.{fld}')
            elif is_int_type(resolved):
                ints.append(f'{obj}.{fld}')
            elif is_float_type(resolved):
                floats.append(f'{obj}.{fld}')
            else:
                unknown.append(f'{obj}.{fld}:{resolved}')
        for idm in re.finditer(r'\b([A-Za-z_]\w*)\b', arg):
            ident = idm.group(1)
            if ident in ('FMath', 'TEXT', 'PI', 'true', 'false', 'nullptr'):
                continue
            if ident in INT_TYPES or ident in FLOAT_TYPES:
                continue
            after = arg[idm.end():].lstrip()
            if after.startswith('('):
                # function call: unknown return, unless we know it
                unknown.append(ident + '()')
                continue
            # skip if this ident was the object or the field in obj.fld (already handled)
            if any(ident == o or ident == f for o, f in used_fields):
                continue
            if ident in locs:
                ty = locs[ident]
                if ty == 'auto':
                    unknown.append(ident)
                elif is_int_type(ty):
                    ints.append(ident)
                elif is_float_type(ty):
                    floats.append(ident)
                else:
                    # auto or object
                    unknown.append(ident)
                continue
            if cls and (cls, ident) in field_ty:
                ty = field_ty[(cls, ident)]
                if is_int_type(ty):
                    ints.append(ident)
                elif is_float_type(ty):
                    floats.append(ident)
                else:
                    unknown.append(ident)
                continue
            unknown.append(ident)
        if floats:
            continue
        # If every resolved leaf is int and there are no unknown leaves, this is C2668.
        if ints and not unknown:
            arg1 = re.sub(r'\s+', ' ', arg).strip()
            bad.append(f'{rel(f)}:{line_of(t, m.start())}: FMath::{m.group(1)}({arg1}) takes an integer argument; '
                       f'drop the call or cast to float (UE 5.8 C2668)')
        # also: argument is only integer literals and operators
        elif not ints and not unknown and re.fullmatch(r'[\d\s+\-*/%()]+', arg or ''):
            arg1 = re.sub(r'\s+', ' ', arg).strip()
            bad.append(f'{rel(f)}:{line_of(t, m.start())}: FMath::{m.group(1)}({arg1}) takes an integer argument; '
                       f'drop the call or cast to float (UE 5.8 C2668)')


# ===== 2. TCHAR* compared to TEXT() =====
# Build a per-file map of local TCHAR* names? We'll do it per function.
CMP_RE = re.compile(
    r'((?:[\w]+(?:\.|->))*[\w]+)\s*(==|!=)\s*TEXT\s*\(|'
    r'TEXT\s*\(\s*"[^"]*"\s*\)\s*(==|!=)\s*((?:[\w]+(?:\.|->))*[\w]+)'
)

def expr_is_tchar(expr, locs, cls, file_tchar_locals):
    """Return True if expr is a raw TCHAR pointer/array (not FString/FName)."""
    if '.' in expr or '->' in expr:
        # split last field
        if '->' in expr:
            obj, fld = expr.rsplit('->', 1)
            obj = obj.split('.')[-1].split('->')[-1]
        else:
            obj, fld = expr.rsplit('.', 1)
        # type of obj
        oty = locs.get(obj)
        if oty:
            sn, _ = norm_type(oty)
            if sn in tchar_fields and fld in tchar_fields[sn]:
                return True
            if (sn, fld) in field_ty:
                ty = field_ty[(sn, fld)]
                return 'TCHAR' in ty
        if cls and (cls, obj) in field_ty:
            sn, _ = norm_type(field_ty[(cls, obj)])
            if sn in tchar_fields and fld in tchar_fields[sn]:
                return True
            if (sn, fld) in field_ty:
                return 'TCHAR' in field_ty[(sn, fld)]
        # foreach: const FBoon& B
        # if unique TCHAR* field?
        # only if every known struct with this field has TCHAR* — too risky if FString Id also exists
        return False
    # bare name
    if expr in locs:
        ty = locs[expr]
        # A TCHAR (value) is not a pointer. Only pointer/array types are C4130.
        return 'TCHAR' in ty and ('*' in ty or '[]' in ty)
    if cls and (cls, expr) in field_ty:
        ty = field_ty[(cls, expr)]
        return 'TCHAR' in ty and ('*' in ty or '[]' in ty)
    return False


for f, t in clean.items():
    # file-scope TCHAR* names (static const TCHAR* Foo, struct fields already typed)
    file_tchar = set()
    for m in TCHAR_PTR.finditer(t):
        # only at file / namespace scope roughly: skip later via locs
        file_tchar.add(m.group(1))
    for m in TCHAR_ARR.finditer(t):
        file_tchar.add(m.group(1))
    spans = func_span_map(t)
    def span_at(pos):
        for s in spans:
            if s[0] <= pos < s[1]:
                return s
        return None
    for m in CMP_RE.finditer(t):
        snippet = t[m.start():m.end()]
        if "TEXT(''" in snippet.replace(' ', '') or "TEXT(\''" in snippet:
            continue
        # TCHAR (not TCHAR*) compared to TEXT('x') is a character compare and is valid.
        if re.search(r"TEXT\s*\(\s*'", snippet):
            continue
        if m.group(1):
            expr, op = m.group(1), m.group(2)
        else:
            expr, op = m.group(4), m.group(3)
        sp = span_at(m.start())
        locs = locals_in(sp[4], sp[6]) if sp else {}
        # also bring in enclosing-file struct names? already in field_ty
        # enrich locs with foreach types more carefully: `for (const FBoon& B : Boons)`
        body = sp[6] if sp else t
        for fm in re.finditer(
                r'\bfor\s*\(\s*(?:const\s+)?(\w+)\s*(\*|&)?\s*(?:const\s+)?(\w+)\s*:', body):
            typ = fm.group(1) + (' *' if fm.group(2) == '*' else '')
            # Do not let a later for-loop (different scope, same name) overwrite a
            # more specific earlier local (FString S vs later const TCHAR* S).
            if fm.group(3) not in locs or locs[fm.group(3)] == 'auto':
                locs[fm.group(3)] = typ
        cls = sp[2] if sp else None
        if expr_is_tchar(expr, locs, cls, file_tchar):
            src = t.splitlines()[line_of(t, m.start()) - 1].strip()
            bad.append(f'{rel(f)}:{line_of(t, m.start())}: {expr} {op} TEXT(...) compares a TCHAR* '
                       f'to a string literal (UE 5.8 C4130); use FCString::Strcmp or FString')


# ===== 3. member shadowing (C4458) =====
LAMBDA_RE = re.compile(r'\bauto\s+(\w+)\s*=\s*\[')
COND_DECL = re.compile(
    r'\b(?:if|for|while|switch)\s*\(\s*(?:const\s+)?(?:auto(?:\s*\*|\s*&)?|'
    r'[\w:]+(?:\s*<[^;{}<>]*>)?)\s*(?:\*+|&)?\s*(\w+)\s*(?:=|:)'
)
# statement-start local: after { or ; or at body start
STMT_LOCAL = re.compile(
    r'(?:^|\{|;)\s*((?:(?:const|static|constexpr|volatile|mutable)\s+)*)'
    r'(auto|int32|int64|uint8|uint16|uint32|uint64|int|float|double|bool|'
    r'FString|FName|FText|FVector2D|FVector|FRotator|FIntPoint|'
    r'TCHAR|'
    r'[UAF][A-Z]\w+|'
    r'TArray\s*<[^;{}<>]+>|TMap\s*<[^;{}<>]+>|'
    r'TShared(?:Ptr|Ref)\s*<[^;{}<>]+>|TObjectPtr\s*<[^;{}<>]+>|'
    r'TWeakObjectPtr\s*<[^;{}<>]+>|TFunction\s*<[^;{}<>]+>)'
    r'\s*(\*+|&)?\s*(\w+)\s*(?:=\s*\[|=|;|\{)',
    re.M
)

for f, t in clean.items():
    for start, end, cls, meth, params, body_start, body in func_span_map(t):
        mems = inherited_members(cls) if cls else set()
        if not mems:
            continue
        declared = []  # (kind, name, pos)
        if params.strip() and params.strip() != 'void':
            for part in params.split(','):
                raw = part.strip()
                if not raw or raw == '...':
                    continue
                if '=' in raw:
                    raw = raw[:raw.index('=')].strip()
                ids = re.findall(r'\b([A-Za-z_]\w*)\b', raw)
                if ids:
                    n = ids[-1]
                    if n not in KW and n not in INT_TYPES and n not in FLOAT_TYPES and n not in (
                            'FString', 'FName', 'FText', 'FVector', 'FVector2D', 'TCHAR',
                            'TArray', 'TMap', 'TSet', 'TOptional', 'TFunction', 'TSharedPtr',
                            'TSharedRef', 'TWeakObjectPtr', 'TObjectPtr', 'TSubclassOf'):
                        declared.append(('parameter', n, start))
        for m in LAMBDA_RE.finditer(body):
            declared.append(('lambda', m.group(1), body_start + m.start()))
        for m in COND_DECL.finditer(body):
            if m.group(1) not in KW:
                declared.append(('local', m.group(1), body_start + m.start()))
        blob = '{\n' + body
        for m in STMT_LOCAL.finditer(blob):
            n = m.group(4)
            if n in KW or n == meth:
                continue
            declared.append(('local', n, body_start + m.start()))
        seen = set()
        for kind, n, pos in declared:
            if n in seen or n == meth:
                continue
            if n in mems:
                seen.add(n)
                ln = line_of(t, pos) if pos < len(t) else line_of(t, start)
                src = t.splitlines()[ln - 1].strip() if 0 < ln <= len(t.splitlines()) else ''
                bad.append(f'{rel(f)}:{ln}: {kind} `{n}` hides {cls}::{n} (UE 5.8 C4458); rename the {kind}')


# ===== 4. incomplete types / missing includes =====
def uses_need(text, typ):
    if typ == 'FJsonObject':
        return bool(JSON_USE.search(text))
    if typ == 'UGameInstance':
        return bool(GI_USE.search(text))
    if typ == 'FHitResult':
        return bool(HIT_USE.search(text))
    if typ == 'UNpcDef':
        return bool(NPC_USE.search(text))
    return False


for f in cpp_files:
    t = clean[f]
    incs, _ = include_closure(f)
    for typ, hdr in (
            ('UNpcDef', 'Data/NpcDef.h'),
            ('FJsonObject', 'Dom/JsonObject.h'),
            ('UGameInstance', 'Engine/GameInstance.h'),
            ('FHitResult', 'Engine/HitResult.h')):
        if uses_need(t, typ) and not header_present(incs, hdr):
            # find a representative line
            rx = {'UNpcDef': NPC_USE, 'FJsonObject': JSON_USE,
                  'UGameInstance': GI_USE, 'FHitResult': HIT_USE}[typ]
            m = rx.search(t)
            ln = line_of(t, m.start()) if m else 1
            bad.append(f'{rel(f)}:{ln}: uses {typ} as a complete type without including {hdr}')


# Dedup and print
# Prefer stable order
out = []
seen = set()
for b in bad:
    if b not in seen:
        seen.add(b)
        out.append(b)

if out:
    print('\n'.join(out))
    print(f'\n{len(out)} UE 5.8 pitfall(s) found')
    sys.exit(1)
print('OK: no UE 5.8 pitfalls found (integer RoundToInt, TCHAR* TEXT compares, member shadows, missing complete-type includes)')
sys.exit(0)
