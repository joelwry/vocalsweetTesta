#!/usr/bin/env python3
"""Create case-variant symlinks in an xwin SDK so that #include names used by a source tree
(e.g. JUCE uses <Dbghelp.h>, the SDK ships DbgHelp.h) resolve on a case-sensitive filesystem."""
import os, re, sys
sdk, *src_dirs = sys.argv[1:]
inc_dirs = [os.path.join(sdk, p) for p in ("crt/include", "sdk/include/ucrt", "sdk/include/um", "sdk/include/shared", "sdk/include/winrt")]
index = {}
for d in inc_dirs:
    for f in os.listdir(d):
        index.setdefault(f.lower(), []).append((d, f))
pat = re.compile(r'#\s*include\s*[<"]([^>"]+)[>"]')
wanted = set()
for sd in src_dirs:
    for root, _, files in os.walk(sd):
        for fn in files:
            if fn.endswith((".h", ".hpp", ".cpp", ".c", ".mm", ".rc", ".cc")):
                try:
                    for m in pat.finditer(open(os.path.join(root, fn), errors="ignore").read()):
                        wanted.add(m.group(1).split("/")[-1])
                except OSError:
                    pass
lib_dirs = [os.path.join(sdk, p) for p in ("crt/lib/x86_64", "sdk/lib/um/x86_64", "sdk/lib/ucrt/x86_64")]
libpat = re.compile(r'comment\s*\(\s*lib\s*,\s*"([^"]+)"')
libs = set()
for sd in src_dirs:
    for root, _, files in os.walk(sd):
        for fn in files:
            if fn.endswith((".h", ".hpp", ".cpp", ".c", ".cc")):
                try:
                    for m in libpat.finditer(open(os.path.join(root, fn), errors="ignore").read()):
                        n = m.group(1)
                        libs.add(n if n.lower().endswith(".lib") else n + ".lib")
                except OSError:
                    pass
libindex = {}
for d in lib_dirs:
    for f in os.listdir(d):
        libindex.setdefault(f.lower(), []).append((d, f))
made = 0
for name in libs:
    for d, real in libindex.get(name.lower(), []):
        target = os.path.join(d, name)
        if not os.path.exists(target):
            os.symlink(real, target); made += 1
for name in wanted:
    hits = index.get(name.lower())
    if not hits: continue
    for d, real in hits:
        target = os.path.join(d, name)
        if not os.path.exists(target):
            os.symlink(real, target); made += 1
print("created", made, "case symlinks")
