#!/usr/bin/env python3
"""Import hash-pinned retail HLSL needed by the Pre-CU NGE content layer."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import struct
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
PIXEL_CUBE_SAMPLER = re.compile(rb"(?m)^sampler(?P<spacing>[ \t]+)envMap(?P<tail>[ \t]*:[ \t]*register\(s2\);)")
PIXEL_CUBE_PROGRAMS = {
    "pixel_program/a_alpha_envmask_ps20.psh",
    "pixel_program/a_envmask_specmap_ps20.psh",
    "pixel_program/h_color2_envmask_specmap_ps20.psh",
}
PLAIN_PIXEL_INCLUDE_PROGRAMS = {
    "pixel_program/terrain_dot3.inc",
}
TERRAIN_DOT3_PIXEL_HELPER = b'''float3 grPrecuCalculateHemisphericLightingVertexColor(
\tfloat3 direction,
\tfloat3 normal,
\tfloat3 vertexDiffuse,
\tfloat3 vertexColor)
{
\tfloat dotProduct = dot(direction, normal);
\tfloat3 light =
\t\tdot3LightTangentMinusDiffuseColor
\t\t+ dot3LightDiffuseColor
\t\t- max(0.0f, dotProduct) * dot3LightTangentMinusDiffuseColor
\t\t+ min(0.0f, dotProduct) * dot3LightTangentMinusBackColor;
\tlight = light * vertexColor + vertexDiffuse;
\treturn saturate(light);
}

'''
VERTEX_FLAT_CONSTANT_PROGRAMS = {
    "vertex_program/a_lava_alpha_vs11.vsh",
}
VERTEX_EMISSIVE_CONSTANT_PROGRAMS = {
    "vertex_program/terrain_dot3_vs20_blend0_spec.vsh",
    "vertex_program/terrain_dot3_vs20_blend1_spec.vsh",
    "vertex_program/terrain_dot3_vs20_blend2_spec.vsh",
    "vertex_program/terrain_dot3_vs20_blend3_spec.vsh",
}
TERRAIN_DOT3_VERTEX_HELPERS = b'''

float3 precuTransformTerrainDot3(float3 inputDirection, float3 vertexNormal_o)
{
\tfloat3 j = cross(vertexNormal_o, float3(1.0f, 0.0f, 0.0f));
\tfloat3 i = cross(j, vertexNormal_o);
\treturn mul(float3x3(i, j, vertexNormal_o), inputDirection);
}

float3 precuTransformTerrainDot3LightDirection(float3 vertexNormal_o)
{
\treturn normalize(precuTransformTerrainDot3(cLightData_dot3_0_direction.xyz, vertexNormal_o));
}

float4 precuCalculateDiffusePointLight(
\tfloat3 position_w,
\tfloat4 diffuseColor,
\tfloat4 attenuation,
\tfloat3 vertexPosition_w,
\tfloat3 normal_w,
\tfloat attenuationW)
{
\tfloat3 lightDirection = position_w - vertexPosition_w;
\tfloat lightDistanceSquared = dot(lightDirection, lightDirection);
\tfloat oneOverLightDistance = rsqrt(lightDistanceSquared);
\tlightDirection *= oneOverLightDistance;
\tfloat4 attenuationFactors = float4(
\t\t1.0f,
\t\tlightDistanceSquared * oneOverLightDistance,
\t\tlightDistanceSquared,
\t\tattenuationW);
\tfloat distanceAttenuation = 1.0f / dot(attenuation, attenuationFactors);
\treturn max(dot(normal_w, lightDirection), 0.0f) * distanceAttenuation * diffuseColor;
}

float4 precuCalculateDiffuseTerrainLighting(float4 vertexPosition_o, float3 vertexNormal_o)
{
\tfloat4x4 objectWorld = float4x4(c[4], c[5], c[6], c[7]);
\tfloat3 vertexPosition_w = mul(vertexPosition_o, objectWorld).xyz;
\tfloat3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorld));
\tfloat4 result =
\t\tmax(dot(normal_w, cLightData_parallel_0_direction.xyz), 0.0f) * cLightData_parallel_0_diffuseColor
\t\t+ max(dot(normal_w, cLightData_parallel_1_direction.xyz), 0.0f) * cLightData_parallel_1_diffuseColor;
\tresult += precuCalculateDiffusePointLight(
\t\tcLightData_pointSpecular_0_position.xyz,
\t\tcLightData_pointSpecular_0_diffuseColor,
\t\tcLightData_pointSpecular_0_attenuation,
\t\tvertexPosition_w,
\t\tnormal_w,
\t\t1.0f);
\tresult += precuCalculateDiffusePointLight(cLightData_point_0_position.xyz, cLightData_point_0_diffuseColor, cLightData_point_0_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_0_position.xyz - vertexPosition_w, cLightData_point_0_position.xyz - vertexPosition_w)));
\tresult += precuCalculateDiffusePointLight(cLightData_point_1_position.xyz, cLightData_point_1_diffuseColor, cLightData_point_1_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_1_position.xyz - vertexPosition_w, cLightData_point_1_position.xyz - vertexPosition_w)));
\tresult += precuCalculateDiffusePointLight(cLightData_point_2_position.xyz, cLightData_point_2_diffuseColor, cLightData_point_2_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_2_position.xyz - vertexPosition_w, cLightData_point_2_position.xyz - vertexPosition_w)));
\tresult += precuCalculateDiffusePointLight(cLightData_point_3_position.xyz, cLightData_point_3_diffuseColor, cLightData_point_3_attenuation, vertexPosition_w, normal_w, rsqrt(dot(cLightData_point_3_position.xyz - vertexPosition_w, cLightData_point_3_position.xyz - vertexPosition_w)));
\treturn result;
}

float precuIntensity(float3 rgb)
{
\treturn dot(rgb, float3(0.3f, 0.59f, 0.11f));
}
'''
PIXEL_HEMISPHERIC_CALL = re.compile(rb"(?<![A-Za-z0-9_])calculateHemisphericLighting\(")
PIXEL_HEMISPHERIC_HELPER = b'''float3 grPrecuCalculateHemisphericLighting(float3 direction, float3 normal, float3 vertexDiffuse)
{
\tfloat dotProduct = dot(direction, normal);
\tfloat3 light = vertexDiffuse + dot3LightTangentMinusDiffuseColor + dot3LightDiffuseColor
\t\t+ (-max(0.0, dotProduct) * dot3LightTangentMinusDiffuseColor);
\tlight += min(0.0, dotProduct) * dot3LightTangentMinusBackColor;
\treturn saturate(light);
}

'''
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
        header = handle.readline().removeprefix("# ").rstrip("\r\n").split("\t")
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
    elif name in PLAIN_PIXEL_INCLUDE_PROGRAMS:
        is_hlsl = (
            payload.lstrip(b"\xef\xbb\xbf\x00\r\n\t ").startswith(b"//")
            and b"computeLayerColor" in payload
        )
    elif name.startswith("pixel_program/"):
        is_hlsl = payload.startswith(b"FORM") and b"//hlsl" in payload
    else:
        is_hlsl = False
    if not is_hlsl:
        raise ImportError(f"refusing non-HLSL program from clean source: {name}")


def rewrite_iff_chunks(payload: bytes, transform) -> bytes:
    """Rebuild an IFF while allowing a leaf chunk payload to grow."""

    output = bytearray()
    offset = 0
    while offset + 8 <= len(payload):
        tag = payload[offset : offset + 4]
        length = int.from_bytes(payload[offset + 4 : offset + 8], "big")
        body_start = offset + 8
        body_end = body_start + length
        if body_end > len(payload):
            raise ImportError("shader IFF contains a truncated chunk")
        body = payload[body_start:body_end]
        if tag == b"FORM":
            if len(body) < 4:
                raise ImportError("shader IFF contains a truncated FORM")
            body = body[:4] + rewrite_iff_chunks(body[4:], transform)
        else:
            body = transform(tag, body)
        output.extend(tag)
        output.extend(struct.pack(">I", len(body)))
        output.extend(body)
        offset = body_end
    if offset != len(payload):
        raise ImportError("shader IFF contains trailing partial chunk data")
    return bytes(output)


def adapt_dx11(name: str, payload: bytes) -> bytes:
    if name == "pixel_program/terrain_dot3.inc":
        output, replacements = re.subn(
            rb"(?<![A-Za-z0-9_])calculateHemisphericLightingVertexColor\(",
            b"grPrecuCalculateHemisphericLightingVertexColor(",
            payload,
        )
        if replacements != 3:
            raise ImportError(
                f"expected three terrain hemispheric-light calls in {name}, found {replacements}"
            )
        return TERRAIN_DOT3_PIXEL_HELPER + output

    if name in PIXEL_CUBE_PROGRAMS:
        sampler_replacements = 0
        call_replacements = 0
        helper_insertions = 0

        def transform(tag: bytes, body: bytes) -> bytes:
            nonlocal sampler_replacements, call_replacements, helper_insertions
            if tag != b"PSRC":
                return body
            output, count = PIXEL_CUBE_SAMPLER.subn(
                rb"samplerCUBE\g<spacing>envMap\g<tail>", body
            )
            sampler_replacements += count
            output, count = PIXEL_HEMISPHERIC_CALL.subn(
                b"grPrecuCalculateHemisphericLighting(", output
            )
            call_replacements += count
            sampler_start = output.find(b"sampler ")
            if sampler_start < 0:
                raise ImportError(f"could not locate sampler declarations in {name}")
            output = (
                output[:sampler_start]
                + PIXEL_HEMISPHERIC_HELPER
                + output[sampler_start:]
            )
            helper_insertions += 1
            return output

        output = rewrite_iff_chunks(payload, transform)
        if sampler_replacements != 1:
            raise ImportError(
                f"expected one envMap cube sampler declaration in {name}, "
                f"found {sampler_replacements}"
            )
        if call_replacements != 1 or helper_insertions != 1:
            raise ImportError(
                f"expected one hemispheric-light call and helper insertion in {name}, "
                f"found calls={call_replacements}, helpers={helper_insertions}"
            )
        return output

    if name.startswith("pixel_program/"):
        return payload

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
    if name in VERTEX_FLAT_CONSTANT_PROGRAMS:
        output, emissive_replacements = re.subn(
            rb"\bmaterial\.emissiveColor\b", b"cMaterial_emissiveColor", output
        )
        output, specular_replacements = re.subn(
            rb"\bmaterial\.specularColor\b", b"cMaterial_specularColor", output
        )
        output, time_replacements = re.subn(
            rb"\bcurrentTime\b", b"cCurrentTime.x", output
        )
        if (emissive_replacements, specular_replacements, time_replacements) != (1, 3, 1):
            raise ImportError(
                f"unexpected flat-constant references in {name}: "
                f"emissive={emissive_replacements}, "
                f"specular={specular_replacements}, time={time_replacements}"
            )
    if name in VERTEX_EMISSIVE_CONSTANT_PROGRAMS:
        output = output.replace(VERTEX_DX11_PROLOGUE, VERTEX_DX11_PROLOGUE + TERRAIN_DOT3_VERTEX_HELPERS)
        output, emissive_replacements = re.subn(
            rb"\bmaterial\.emissiveColor\b", b"cMaterial_emissiveColor", output
        )
        replacements = {
            b"transformTerrainDot3LightDirection": b"precuTransformTerrainDot3LightDirection",
            b"transformTerrainDot3": b"precuTransformTerrainDot3",
            b"lightData.ambient.ambientColor": b"cLightData_ambient_ambientColor",
            b"lightData.dot3[0].direction_o": b"cLightData_dot3_0_direction.xyz",
            b"lightData.dot3[0].cameraPosition_o": b"cLightData_dot3_0_cameraPosition.xyz",
            b"intensity(": b"precuIntensity(",
        }
        replacement_counts = {}
        for source, target in replacements.items():
            output, replacement_counts[source] = re.subn(
                rb"(?<![A-Za-z0-9_])" + re.escape(source), target, output
            )
        output, lighting_replacements = re.subn(
            rb"DiffuseSpecular diffuseSpecular = calculateDiffuseSpecularTerrainLighting\(true, inputVertex\.position, inputVertex\.normal\);",
            b"float4 precuDiffuse = precuCalculateDiffuseTerrainLighting(inputVertex.position, inputVertex.normal);",
            output,
        )
        output, diffuse_replacements = re.subn(
            rb"diffuseSpecular\.diffuse", b"precuDiffuse", output
        )
        expected_counts = {
            b"transformTerrainDot3LightDirection": 1,
            b"transformTerrainDot3": 2,
            b"lightData.ambient.ambientColor": 1,
            b"lightData.dot3[0].direction_o": 1,
            b"lightData.dot3[0].cameraPosition_o": 2,
            b"intensity(": 1,
        }
        if (
            emissive_replacements != 1
            or replacement_counts != expected_counts
            or lighting_replacements != 1
            or diffuse_replacements != 1
        ):
            raise ImportError(
                f"unexpected terrain shader references in {name}: "
                f"emissive={emissive_replacements}, replacements={replacement_counts}, "
                f"lighting={lighting_replacements}, diffuse={diffuse_replacements}"
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
