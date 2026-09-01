"""Relativize the tool cfgs in C:/swg/current/exe/win32 for the SOE-shaped root.

All paths become relative to the exe dir (cwd contract: tools launch from
exe/win32). Mappings:
  D:/Code/SWGSource Client v3.0/            -> ../../tre/
  D:/Code/Galaxies-Reborn/stage-B-override  -> ../../data/override
  D:/Code/Galaxies-Reborn/stage-cee-loose   -> ../../data/override-cee
  D:/SWG All Tools Working/swg/current/     -> ../../
Both slash directions, case-insensitive drive/dir matching not needed (the
cfgs are machine-generated and consistent); comments are transformed too so
no stale absolute paths survive to confuse anyone reading the installed cfg.
"""
import io, os, re, sys

EXE = r"C:\swg\current\exe\win32"
RULES = [
    (re.compile(r"D:[/\\]Code[/\\]SWGSource Client v3\.0[/\\]", re.I), "../../tre/"),
    (re.compile(r"D:[/\\]Code[/\\]Galaxies-Reborn[/\\]stage-B-override", re.I), "../../data/override"),
    (re.compile(r"D:[/\\]Code[/\\]Galaxies-Reborn[/\\]stage-cee-loose", re.I), "../../data/override-cee"),
    (re.compile(r"D:[/\\]SWG All Tools Working[/\\]swg[/\\]current[/\\]", re.I), "../../"),
]

changed = 0
for name in sorted(os.listdir(EXE)):
    if not name.lower().endswith((".cfg", ".ini", ".xml")):
        continue
    p = os.path.join(EXE, name)
    src = io.open(p, "r", encoding="utf-8", errors="replace").read()
    out = src
    for rx, rep in RULES:
        out = rx.sub(rep, out)
    if out != src:
        io.open(p, "w", encoding="utf-8", newline="").write(out)
        n = sum(1 for a, b in zip(src.splitlines(), out.splitlines()) if a != b)
        print("%-34s %d lines changed" % (name, n))
        changed += 1
print("--- %d files rewritten" % changed)
