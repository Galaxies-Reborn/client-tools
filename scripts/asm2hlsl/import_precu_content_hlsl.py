#!/usr/bin/env python3
"""Import hash-pinned retail HLSL needed by the Pre-CU NGE content layer."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from dataclasses import dataclass
from pathlib import Path

import toc


HERE = Path(__file__).resolve().parent
DEFAULT_MANIFEST = HERE / "precu-content-hlsl.tsv"
DEFAULT_OUTPUT = HERE / "converted"
SOURCE_TOCS = (
    "sku3_client.toc",
    "sku2_client.toc",
    "sku1_client.toc",
    "sku0_client.toc",
)
VERTEX_INCLUDE_PAIR = re.compile(
    rb'#include\s+"vertex_program/include/vertex_shader_constants\.inc"[\r\n]+'
    rb'#include\s+"vertex_program/include/functions\.inc"'
)
VERTEX_DX11_PROLOGUE = b'''#include "vertex_program/include/asm_constants.inc"

#define objectWorldCameraProjectionMatrix float4x4(c[0], c[1], c[2], c[3])

float4 precuTransform3d(float4 position_o)
{
\treturn mul(position_o, float4x4(c[0], c[1], c[2], c[3]));
}

float precuCalculateFog(float4 position_o)
{
\tfloat4 position_w = mul(position_o, float4x4(c[4], c[5], c[6], c[7]));
\tfloat3 viewer_w = c[8].xyz - position_w.xyz;
\treturn 1.0f / exp(dot(viewer_w, viewer_w) * c[10].w);
}

#define transform3d precuTransform3d
#define calculateFog precuCalculateFog
'''


class ImportError(RuntimeError):
    """Raised when clean-source provenance or HLSL validation fails."""


@dataclass(frozen=True)
class ManifestRow:
    path: str
    source_size: int
    source_sha256: str
    output_size: int
    output_sha256: str
    source_toc: str
    source_tre: str


def normalize(value: str) -> str:
    return value.lower().replace("\\", "/").lstrip("/")


def read_manifest(path: Path) -> list[ManifestRow]:
    with path.open(encoding="utf-8", newline="") as handle:
        header = handle.readline().removeprefix("# ").rstrip("\n").split("\t")
        rows = [
            ManifestRow(
                path=normalize(row["path"]),
                source_size=int(row["source_size"]),
                source_sha256=row["source_sha256"].upper(),
                output_size=int(row["output_size"]),
                output_sha256=row["output_sha256"].upper(),
                source_toc=row["source_toc"],
                source_tre=row["source_tre"],
            )
            for row in csv.DictReader(handle, fieldnames=header, delimiter="\t")
        ]
    if not rows or len({row.path for row in rows}) != len(rows):
        raise ImportError("content HLSL manifest is empty or contains duplicate paths")
    return rows


def source_index(source_client: Path):
    winners = {}
    tree_lists = {}
    for toc_name in SOURCE_TOCS:
        toc_path = source_client / toc_name
        if not toc_path.is_file():
            raise ImportError(f"clean source TOC was not found: {toc_path}")
        _header, trees, entries = toc.read_toc(str(toc_path))
        tree_lists[toc_name] = trees
        for entry in entries:
            name = normalize(entry.name)
            if entry.usable and name not in winners:
                winners[name] = (toc_name, trees[entry.treeFileIndex], entry)
    return winners, tree_lists


def require_hlsl(name: str, payload: bytes) -> None:
    if name.startswith("vertex_program/"):
        is_hlsl = payload.lstrip(b"\xef\xbb\xbf\x00\r\n\t ").startswith(b"//hlsl")
    elif name.startswith("pixel_program/"):
        is_hlsl = payload.startswith(b"FORM") and b"//hlsl" in payload
    else:
        is_hlsl = False
    if not is_hlsl:
        raise ImportError(f"refusing non-HLSL program from clean source: {name}")


def adapt_dx11(name: str, payload: bytes) -> bytes:
    if not name.startswith("vertex_program/"):
        return payload
    # D3D9 HLSL allowed both a semantic and register(vN) on an input-struct
    # member. Shader model 4 keeps the semantic and rejects the location
    # annotation with X3202, so remove only that obsolete annotation.
    output = re.sub(rb"\s*:\s*register\(v\d+\)", b"", payload)
    output, replacements = VERTEX_INCLUDE_PAIR.subn(VERTEX_DX11_PROLOGUE, output)
    if replacements != 1:
        raise ImportError(
            f"expected one retail vertex include pair in {name}, found {replacements}"
        )
    return output


def write_payload(root: Path, name: str, payload: bytes) -> Path:
    destination = root.joinpath(*name.split("/"))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(payload)
    return destination


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-client", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stage-client", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rows = read_manifest(args.manifest)
    winners, tree_lists = source_index(args.source_client)
    staged = 0
    for row in rows:
        source = winners.get(row.path)
        if source is None:
            raise ImportError(f"clean source does not contain {row.path}")
        toc_name, tree_name, entry = source
        if toc_name != row.source_toc or tree_name != row.source_tre:
            raise ImportError(
                f"source precedence drifted for {row.path}: "
                f"{toc_name}/{tree_name}, expected {row.source_toc}/{row.source_tre}"
            )
        payload = toc.open_entry(str(args.source_client), tree_lists[toc_name], entry)
        digest = hashlib.sha256(payload).hexdigest().upper()
        if len(payload) != row.source_size or digest != row.source_sha256:
            raise ImportError(
                f"clean source bytes drifted for {row.path}: "
                f"size={len(payload)} sha256={digest}"
            )
        require_hlsl(row.path, payload)
        output = adapt_dx11(row.path, payload)
        output_digest = hashlib.sha256(output).hexdigest().upper()
        if len(output) != row.output_size or output_digest != row.output_sha256:
            raise ImportError(
                f"DX11 adaptation drifted for {row.path}: "
                f"size={len(output)} sha256={output_digest}"
            )
        write_payload(args.output_root, row.path, output)
        if args.stage_client is not None:
            write_payload(args.stage_client, row.path, output)
            staged += 1

    print(
        json.dumps(
            {
                "hlsl_program_count": len(rows),
                "output_root": str(args.output_root),
                "staged_program_count": staged,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
