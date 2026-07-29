# Compile every converted program with fxc, the same way the backend will.
#
# The converted tree is overlaid on the original corpus so that includes the conversion did
# not touch (pixel_shader_constants.inc and friends) still resolve, exactly as the loose
# asset overlay will do at runtime.

import collections
import io
import os
import re
import shutil
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))

CORPUS = os.path.join(HERE, "corpus")
CONVERTED = os.path.join(HERE, "converted")
WORK = os.path.join(HERE, "convwork")
FXC = r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe"


def psrc(data):
    if data[:4] != b"FORM":
        return data
    i = data.find(b"PSRC")
    if i < 0:
        return None
    length = int.from_bytes(data[i + 4:i + 8], "big")
    return data[i + 8:i + 8 + length]


def main():
    if os.path.exists(WORK):
        shutil.rmtree(WORK)
    root = os.path.join(WORK, "root")
    shutil.copytree(CORPUS, root)

    # Overlay the conversion.
    for dirpath, _dirs, files in os.walk(CONVERTED):
        for name in files:
            source = os.path.join(dirpath, name)
            rel = os.path.relpath(source, CONVERTED)
            target = os.path.join(root, rel)
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copyfile(source, target)

    deep = os.path.join(root, "_c", "_c")
    os.makedirs(deep, exist_ok=True)

    results = collections.Counter()
    failures = []

    converted = []
    for dirpath, _dirs, files in os.walk(CONVERTED):
        for name in sorted(files):
            if name.endswith((".vsh", ".psh")):
                converted.append(os.path.relpath(os.path.join(dirpath, name), CONVERTED).replace("\\", "/"))

    for rel in sorted(converted):
        path = os.path.join(root, *rel.split("/"))
        raw = open(path, "rb").read()
        text = psrc(raw)
        if text is None:
            results["no PSRC"] += 1
            continue
        source = text.decode("latin1", "replace").replace("\x00", "")
        source = source.replace("\r\n", "\n").replace("\r", "\n")

        first, separator, body = source.partition("\n")
        if not separator or not first.strip().lower().startswith("//hlsl"):
            results["bad header"] += 1
            failures.append((rel, "missing //hlsl profile header"))
            continue
        target = "vs_4_0" if rel.endswith(".vsh") else "ps_4_0"

        work = os.path.join(deep, "shader.hlsl")
        open(work, "w", encoding="latin-1", errors="replace").write(body)

        cmd = [FXC, "/nologo", "/T", target, "/E", "main", "/Gec", "/Zpc", "/O1",
               "/I", root, "/Fo", os.path.join(deep, "out.cso"), "shader.hlsl",
               "/D", "point=_pt_lights"]
        proc = subprocess.run(cmd, cwd=deep, capture_output=True, text=True, errors="replace")
        if proc.returncode == 0:
            results["compiled"] += 1
        else:
            results["FAILED"] += 1
            out = (proc.stdout or "") + (proc.stderr or "")
            first = ""
            for line in out.splitlines():
                m = re.search(r"error (X\d+): (.*)", line)
                if m:
                    first = "%s %s" % (m.group(1), m.group(2)[:90])
                    break
            failures.append((rel, first or out.strip()[:90]))

    print("converted programs compiled: %d" % results["compiled"])
    print("converted programs FAILED:   %d" % results["FAILED"])

    if failures:
        grouped = collections.Counter(f[1] for f in failures)
        print()
        print("failure reasons:")
        for message, n in grouped.most_common(20):
            print("   %4d  %s" % (n, message))
        print()
        print("first 15 failing files:")
        for rel, message in failures[:15]:
            print("   %-52s %s" % (rel, message))
        with open(os.path.join(HERE, "conv-failures.txt"), "w", encoding="utf-8") as f:
            for rel, message in failures:
                f.write("%s\t%s\n" % (rel, message))
        raise SystemExit(1)


if __name__ == "__main__":
    main()
