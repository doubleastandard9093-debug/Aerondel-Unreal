# Unity-build hazard check: names brought to file scope with `using namespace X;` (or file-scope statics) in one .cpp
# that are re-declared as locals/params/globals in another .cpp of the module. MSVC C4459 ("hides global
# declaration") is an error in this project, and duplicate file-scope statics are redefinitions once files are batched.
import glob,re,sys,collections
import os
root=sys.argv[1] if len(sys.argv)>1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Source', 'Aerondel', '')
root=os.path.normpath(root)+os.sep
files={f:open(f,encoding='utf-8',errors='ignore').read() for f in glob.glob(root+'**/*.cpp',recursive=True)}
TYPE=r'(?:const\s+)?(?:static\s+)?(?:constexpr\s+)?(?:int32|int64|uint8|float|double|bool|FString|FName|FVector2D|FVector|TCHAR|auto|T\w+<[^;()]*?>|F\w+)\s*[*&]?\s*'
exported=collections.defaultdict(set)  # name -> files that put it at file scope
def ns_bodies(s,name):
    for m in re.finditer(r'namespace\s+'+name+r'\s*\{',s):
        i,d=m.end(),1
        while d and i<len(s): d+={'{':1,'}':-1}.get(s[i],0); i+=1
        yield s[m.end():i-1]
for f,s in files.items():
    for ns in re.findall(r'^using namespace (\w+);',s,re.M):
        for body in ns_bodies(s,ns):
            # top-level declarations only (depth 0 inside the namespace)
            depth=0;buf=''
            for ch in body:
                if ch=='{': depth+=1
                if depth==0: buf+=ch
                if ch=='}': depth-=1
            for m in re.finditer(r'(?:static\s+)?(?:constexpr\s+|const\s+)+[\w:<>, *]+?\s\**(\w+)\s*(?:\[\])?\s*=',buf): exported[m.group(1)].add(f)
            for m in re.finditer(r'static\s+(?:inline\s+)?[\w:<>*& ]+?\s\**(\w+)\s*\(',buf): exported[m.group(1)].add(f)
    for m in re.finditer(r'^static\s+(?:const\s+|constexpr\s+)*[\w:<>*& ]+?\s\**(\w+)\s*(?:\[\])?\s*[=(;]',s,re.M): exported[m.group(1)].add(f)
bad=[]
for name,src in exported.items():
    if len(name)<2: continue
    if len(src)>1: bad.append(f'{name}: at file scope in {len(src)} files: '+', '.join(sorted(x.replace(root,"") for x in src)))
    pat=re.compile(r'(?<![\w:.>])'+TYPE+r'\b'+re.escape(name)+r'\b\s*(?:=|;|,|\)|\[)')
    for f,s in files.items():
        if f in src: continue
        for m in pat.finditer(s):
            line=s[s.rfind(chr(10),0,m.start())+1:s.find(chr(10),m.start())]
            if 'struct ' in line or 'class ' in line: continue
            bad.append(f'{name}: from {", ".join(x.replace(root,"") for x in src)} is re-declared in {f.replace(root,"")}:{s[:m.start()].count(chr(10))+1}'); break
print('\n'.join(sorted(set(bad))) if bad else 'OK: no unity-build name hazards found'); sys.exit(1 if bad else 0)
