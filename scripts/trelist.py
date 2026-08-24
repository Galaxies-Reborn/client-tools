"""List / extract entries from SWG .tre archives (TREE version 0005)."""
import os, sys, zlib, struct, glob

def read_tre(path):
    with open(path, "rb") as f:
        head = f.read(36)
        if len(head) < 36 or head[0:4] not in (b"TREE", b"EERT"):
            return None
        version = head[4:8]
        # Only the 0005 layout is implemented. Version 0006 packs (e.g. SWG
        # Restoration) use a different TOC record layout: parsing them with the
        # 0005 reader yields silently CORRUPT names ("pearance", "ject", ...)
        # rather than an error, so refuse them outright.
        if version not in (b"5000", b"0005"):
            raise ValueError("unsupported TRE version %r (only 0005 implemented)" % version)
        (count, toc_off, toc_comp, toc_csize,
         name_comp, name_csize, name_usize) = struct.unpack("<7I", head[8:36])

        f.seek(toc_off)
        toc_raw = f.read(toc_csize if toc_comp else count * 24)
        if toc_comp:
            toc_raw = zlib.decompress(toc_raw)

        name_raw = f.read(name_csize if name_comp else name_usize)
        if name_comp:
            name_raw = zlib.decompress(name_raw)

    entries = []
    for i in range(count):
        rec = toc_raw[i * 24:(i + 1) * 24]
        checksum, usize, off, comp, csize, noff = struct.unpack("<6I", rec)
        end = name_raw.index(b"\0", noff)
        name = name_raw[noff:end].decode("latin-1")
        entries.append((name, off, comp, csize, usize))
    return entries

def extract(path, entry):
    name, off, comp, csize, usize = entry
    with open(path, "rb") as f:
        f.seek(off)
        blob = f.read(csize if comp else usize)
    if comp:
        blob = zlib.decompress(blob)
    return blob

if __name__ == "__main__":
    root = sys.argv[1]
    pattern = sys.argv[2].lower() if len(sys.argv) > 2 else ""
    dump = len(sys.argv) > 3 and sys.argv[3] == "--dump"

    tres = sorted(glob.glob(os.path.join(root, "*.tre")))
    total = 0
    bad = 0
    hits = []
    for t in tres:
        try:
            ents = read_tre(t)
        except Exception as e:
            print("  !! %s: %s" % (os.path.basename(t), e))
            bad += 1
            continue
        if ents is None:
            bad += 1
            continue
        total += len(ents)
        for e in ents:
            if pattern in e[0].lower():
                hits.append((os.path.basename(t), t, e))

    print("scanned %d tre(s), %d entries, %d unreadable" % (len(tres), total, bad))
    print("matches for %r: %d" % (pattern, len(hits)))
    for base, full, e in hits[:200]:
        print("  %-28s %s" % (base, e[0]))
    if dump and hits:
        base, full, e = hits[0]
        data = extract(full, e)
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           os.path.basename(e[0]))
        open(out, "wb").write(data)
        print("dumped %s (%d bytes) -> %s" % (e[0], len(data), out))
