#!/usr/bin/env python3
"""Build a local JUCE sample bank from the owner's SWG TRE files.

The generated audio is intentionally excluded from source control.  This tool
only records provenance and analysis metadata alongside the derived WAV files.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import struct
import sys
import wave
import zlib
from dataclasses import dataclass
from pathlib import Path

import numpy as np


TREE_TAG = 0x54524545
TREE_0004 = 0x30303034
TREE_0005 = 0x30303035
TREE_0006 = 0x30303036
HEADER = struct.Struct("<9I")
ENTRY_0005 = struct.Struct("<6I")
ENTRY_0006 = struct.Struct("<8I")
STEM_PATTERN = re.compile(
    r"^player_music/sample/(song\d+)_([a-z0-9]+)_"
    r"(intro|main|outro|flourish\d+)(?:_lp)?\.wav$",
    re.IGNORECASE,
)


@dataclass(frozen=True)
class TreEntry:
    tree: Path
    name: str
    length: int
    offset: int
    compressor: int
    compressed_length: int


INSTRUMENTS = {
    # performance.tab intentionally aliases several visible instruments to the
    # same recorded stem family.  Keep that legacy mapping for tonal fidelity.
    1: ("traz", ("shorn",)),
    2: ("slitherhorn", ("shorn", "slitherhorn")),
    3: ("fanfar", ("shorn",)),
    4: ("chidinkalu horn", ("khorn",)),
    5: ("kloo horn", ("khorn", "kloo", "kloohorn")),
    6: ("fizz", ("khorn",)),
    7: ("bandfill", ("drum", "bandfill")),
    8: ("omnibox", ("drum",)),
    9: ("nalargon", ("nlrg", "nalargon")),
    10: ("mandoviol", ("mand", "mandoviol")),
    11: ("xantha", ("xantha",)),
    12: ("flanged jessoon", ("shorn",)),
    13: ("valahorn", ("khorn",)),
    14: ("downey box", ("drum",)),
}


def read_search_trees(client_root: Path) -> list[Path]:
    config = client_root / "client.cfg"
    if not config.is_file():
        raise RuntimeError(f"client.cfg was not found under {client_root}")
    result: list[Path] = []
    pattern = re.compile(r"^\s*searchTree_[^=]+\s*=\s*([^#;]+)", re.IGNORECASE)
    for line in config.read_text(encoding="latin-1").splitlines():
        match = pattern.match(line)
        if not match:
            continue
        path = client_root / match.group(1).strip().strip('"')
        if path.is_file():
            result.append(path)
    if not result:
        raise RuntimeError(f"client.cfg does not reference any readable TRE files: {config}")
    return result


def read_tre_index(tree: Path) -> list[TreEntry]:
    with tree.open("rb") as stream:
        header_data = stream.read(HEADER.size)
        if len(header_data) != HEADER.size:
            raise RuntimeError(f"TRE header is truncated: {tree}")
        token, version, count, toc_offset, toc_compressor, toc_size, name_compressor, name_size, name_uncompressed = HEADER.unpack(header_data)
        if token != TREE_TAG or version not in (TREE_0004, TREE_0005, TREE_0006):
            raise RuntimeError(f"Unsupported TRE header {token:08x}/{version:08x}: {tree}")

        stride = ENTRY_0006.size if version == TREE_0006 else ENTRY_0005.size
        stream.seek(toc_offset)
        toc_data = stream.read(toc_size if toc_compressor else count * stride)
        if toc_compressor:
            toc_data = zlib.decompress(toc_data)
        expected_toc_size = count * stride
        if len(toc_data) != expected_toc_size:
            raise RuntimeError(f"Unexpected TRE table size in {tree}: {len(toc_data)} != {expected_toc_size}")

        name_data = stream.read(name_size if name_compressor else name_uncompressed)
        if name_compressor:
            name_data = zlib.decompress(name_data)
        if len(name_data) != name_uncompressed:
            raise RuntimeError(f"Unexpected TRE name block size in {tree}")

    entries: list[TreEntry] = []
    for index in range(count):
        fields = struct.unpack_from("<" + ("8I" if version == TREE_0006 else "6I"), toc_data, index * stride)
        if version == TREE_0006:
            length, offset, compressor, compressed_length, name_offset = fields[1], fields[2], fields[6], fields[7], fields[5]
        else:
            length, offset, compressor, compressed_length, name_offset = fields[1], fields[2], fields[3], fields[4], fields[5]
        name_end = name_data.find(b"\0", name_offset)
        if name_end < 0:
            raise RuntimeError(f"Unterminated TRE filename in {tree}")
        name = name_data[name_offset:name_end].decode("latin-1").replace("\\", "/").lower()
        entries.append(TreEntry(tree, name, length, offset, compressor, compressed_length))
    return entries


def extract(entry: TreEntry) -> bytes:
    with entry.tree.open("rb") as stream:
        stream.seek(entry.offset)
        size = entry.compressed_length if entry.compressor else entry.length
        payload = stream.read(size)
    if len(payload) != size:
        raise RuntimeError(f"TRE payload is truncated: {entry.tree}:{entry.name}")
    if entry.compressor:
        payload = zlib.decompress(payload)
    if len(payload) != entry.length:
        raise RuntimeError(f"Unexpected extracted size: {entry.tree}:{entry.name}")
    return payload


def build_catalog(trees: list[Path]) -> dict[str, TreEntry]:
    catalog: dict[str, TreEntry] = {}
    for number, tree in enumerate(trees, 1):
        print(f"[{number:02d}/{len(trees):02d}] Indexing {tree.name}", file=sys.stderr)
        for entry in read_tre_index(tree):
            if STEM_PATTERN.match(entry.name):
                catalog[entry.name] = entry
    return catalog


def decode_wave(payload: bytes) -> tuple[np.ndarray, int]:
    import io

    with wave.open(io.BytesIO(payload), "rb") as source:
        channels = source.getnchannels()
        width = source.getsampwidth()
        sample_rate = source.getframerate()
        frames = source.readframes(source.getnframes())
    if width == 1:
        samples = (np.frombuffer(frames, dtype=np.uint8).astype(np.float64) - 128.0) / 128.0
    elif width == 2:
        samples = np.frombuffer(frames, dtype="<i2").astype(np.float64) / 32768.0
    elif width == 3:
        raw = np.frombuffer(frames, dtype=np.uint8).reshape(-1, 3)
        values = raw[:, 0].astype(np.int32) | (raw[:, 1].astype(np.int32) << 8) | (raw[:, 2].astype(np.int32) << 16)
        values = (values ^ 0x800000) - 0x800000
        samples = values.astype(np.float64) / 8388608.0
    elif width == 4:
        samples = np.frombuffer(frames, dtype="<i4").astype(np.float64) / 2147483648.0
    else:
        raise RuntimeError(f"Unsupported PCM sample width: {width}")
    samples = samples.reshape(-1, channels).mean(axis=1)
    return samples, sample_rate


def select_periodic_waveform(samples: np.ndarray, sample_rate: int) -> tuple[np.ndarray, float, float]:
    window_size = min(16384, 1 << int(math.log2(max(2048, min(len(samples), 16384)))))
    if len(samples) < window_size:
        samples = np.pad(samples, (0, window_size - len(samples)))
    hop = max(window_size // 2, 1)
    best_score = -1.0
    best_window = samples[:window_size]
    best_frequency = 261.625565
    taper = np.hanning(window_size)
    minimum_lag = max(2, int(sample_rate / 1200.0))
    maximum_lag = min(window_size // 3, int(sample_rate / 55.0))

    for start in range(0, max(1, len(samples) - window_size + 1), hop):
        segment = samples[start:start + window_size].astype(np.float64, copy=True)
        segment -= segment.mean()
        rms = float(np.sqrt(np.mean(segment * segment)))
        if rms < 1.0e-4:
            continue
        spectrum = np.fft.rfft(segment * taper, n=window_size * 2)
        autocorrelation = np.fft.irfft(spectrum * np.conj(spectrum))[:window_size]
        if autocorrelation[0] <= 0.0:
            continue
        normalized = autocorrelation / autocorrelation[0]
        lag_region = normalized[minimum_lag:maximum_lag + 1]
        lag = minimum_lag + int(np.argmax(lag_region))
        periodicity = float(normalized[lag])
        score = periodicity * min(rms / 0.12, 1.0)
        if score > best_score:
            best_score = score
            best_window = segment
            best_frequency = sample_rate / float(lag)

    source_period = max(4, int(round(sample_rate / best_frequency)))
    cycle_count = max(2, min(24, len(best_window) // source_period - 1))
    center = len(best_window) // 2
    start = max(0, center - (cycle_count * source_period) // 2)
    cycles = best_window[start:start + cycle_count * source_period].reshape(cycle_count, source_period)
    average_cycle = cycles.mean(axis=0)
    average_cycle -= average_cycle.mean()

    target_rate = 48000
    target_period = max(32, int(round(target_rate / 261.625565)))
    old_x = np.linspace(0.0, 1.0, len(average_cycle), endpoint=False)
    new_x = np.linspace(0.0, 1.0, target_period, endpoint=False)
    cycle = np.interp(new_x, old_x, average_cycle)
    cycle -= cycle.mean()
    peak = float(np.max(np.abs(cycle)))
    if peak < 1.0e-6:
        raise RuntimeError("Could not derive a non-silent waveform")
    cycle *= 0.82 / peak
    rendered = np.tile(cycle, 128)
    return rendered, best_frequency, best_score


def write_wave(path: Path, samples: np.ndarray, sample_rate: int = 48000) -> None:
    pcm = np.clip(samples, -1.0, 1.0)
    encoded = np.round(pcm * 32767.0).astype("<i2").tobytes()
    with wave.open(str(path), "wb") as destination:
        destination.setnchannels(1)
        destination.setsampwidth(2)
        destination.setframerate(sample_rate)
        destination.writeframes(encoded)


def candidates_for(catalog: dict[str, TreEntry], aliases: tuple[str, ...]) -> list[TreEntry]:
    preferred: list[tuple[int, TreEntry]] = []
    for entry in catalog.values():
        match = STEM_PATTERN.match(entry.name)
        if not match or match.group(2).lower() not in aliases:
            continue
        part = match.group(3).lower()
        rank = 0 if part == "main" else (1 if part.startswith("flourish") else 2)
        preferred.append((rank, entry))
    preferred.sort(key=lambda item: (item[0], item[1].name))
    return [entry for _, entry in preferred]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--catalog-only", action="store_true")
    args = parser.parse_args()

    client_root = args.client_root.resolve()
    output = args.output.resolve()
    trees = read_search_trees(client_root)
    catalog = build_catalog(trees)
    tokens = sorted({STEM_PATTERN.match(name).group(2).lower() for name in catalog if STEM_PATTERN.match(name)})
    print("Available player-music stem tokens: " + ", ".join(tokens))
    if args.catalog_only:
        for token in tokens:
            count = sum(1 for name in catalog if STEM_PATTERN.match(name).group(2).lower() == token)
            print(f"  {token:16s} {count:4d} files")
        return 0

    output.mkdir(parents=True, exist_ok=True)
    for instrument_id in INSTRUMENTS:
        stale_sample = output / f"instrument_{instrument_id:02d}.wav"
        if stale_sample.exists():
            stale_sample.unlink()
    stale_manifest = output / "bank.json"
    if stale_manifest.exists():
        stale_manifest.unlink()
    metadata = {"formatVersion": 1, "rootMidiNote": 60, "sampleRate": 48000, "instruments": []}
    for instrument_id, (instrument_name, aliases) in INSTRUMENTS.items():
        choices = candidates_for(catalog, aliases)
        best = None
        failures: list[str] = []
        for entry in choices[:24]:
            try:
                source, rate = decode_wave(extract(entry))
                rendered, frequency, score = select_periodic_waveform(source, rate)
                if best is None or score > best[0]:
                    best = (score, rendered, frequency, entry)
            except Exception as error:  # Continue through alternate stems and formats.
                failures.append(f"{entry.name}: {error}")
        record = {"id": instrument_id, "name": instrument_name, "aliases": list(aliases), "available": best is not None}
        if best is None:
            print(f"[{instrument_id:02d}] {instrument_name}: no matching usable stem (procedural fallback)")
            if failures:
                record["errors"] = failures
        else:
            score, rendered, frequency, entry = best
            destination = output / f"instrument_{instrument_id:02d}.wav"
            write_wave(destination, rendered)
            record.update({
                "sourceTre": entry.tree.name,
                "sourcePath": entry.name,
                "detectedFrequencyHz": round(frequency, 4),
                "periodicityScore": round(score, 5),
                "file": destination.name,
            })
            print(f"[{instrument_id:02d}] {instrument_name}: {entry.name} -> {destination.name}")
        metadata["instruments"].append(record)

    (output / "bank.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="ascii")
    print(f"Sample bank written to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
