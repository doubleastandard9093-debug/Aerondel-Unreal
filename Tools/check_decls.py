# Every Class::Method defined in a .cpp must be declared in some header (catches C2039/C2270 before a Windows build).
import glob,re,sys
import os
root=sys.argv[1] if len(sys.argv)>1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Source', 'Aerondel', '')
root=os.path.normpath(root)+os.sep
hdr={}
for f in glob.glob(root+'**/*.h',recursive=True): hdr[f]=open(f,encoding='utf-8',errors='ignore').read()
allh='\n'.join(hdr.values())
bad=[]
for f in glob.glob(root+'**/*.cpp',recursive=True):
    s=open(f,encoding='utf-8',errors='ignore').read()
    for m in re.finditer(r'^[^\s/#][^\n;{]*?\b([UAF][A-Z]\w+)::(~?\w+)\s*\(',s,re.M):
        c,meth=m.group(1),m.group(2)
        if meth==c or meth=='~'+c: continue
        # find class body
        cm=re.search(r'(class|struct)\s+(?:\w+_API\s+)?'+c+r'\b[^;{]*\{',allh)
        if not cm: continue
        i,d=cm.end(),1
        while d and i<len(allh): d+= {'{':1,'}':-1}.get(allh[i],0); i+=1
        body=allh[cm.end():i]
        if not re.search(r'\b'+re.escape(meth)+r'\s*\(',body): bad.append(f'{f.replace(root,"")}: {c}::{meth}')
print('\n'.join(bad) if bad else 'OK: every Class::Method definition has a declaration'); sys.exit(1 if bad else 0)
