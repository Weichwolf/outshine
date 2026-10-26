"""Independently check one exact whole-pixel centre per constant-depth rectangle."""

import argparse
import hashlib
import json
import math
import struct
from fractions import Fraction
from pathlib import Path

SIDE = 512
TILES = 64
SCALES = (-126, -125, -120, -80, -40, 0, 40, 80, 120, 125, 126)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def floor32(value):
    nearest = struct.unpack("<I", struct.pack("<f", float(value)))[0]
    rounded = struct.unpack("<f", struct.pack("<I", nearest))[0]
    bits = nearest - (Fraction(rounded) > value)
    result = struct.unpack("<f", struct.pack("<I", bits))[0]
    successor = struct.unpack("<f", struct.pack("<I", bits + 1))[0]
    if not Fraction(result) <= value < Fraction(successor):
        raise ValueError("incorrect downward depth rounding")
    return result, bits


def inspect(points, actual, index):
    if not all(math.isfinite(v) for p in points for v in p):
        raise ValueError("nonfinite source")
    rational = [[Fraction(v) for v in p] for p in points]
    if not all(p[3] > 0 and 0 < p[2] < p[3] for p in rational):
        raise ValueError("source crosses depth clip planes")
    xy = [
        ((x / w + 1) * (SIDE // 2), (1 - y / w) * (SIDE // 2))
        for x, y, _, w in rational
    ]
    if not (
        xy[0][0] == xy[3][0] < xy[1][0] == xy[2][0]
        and xy[0][1] == xy[1][1] < xy[2][1] == xy[3][1]
    ):
        raise ValueError("source is not the expected axis-aligned rectangle")
    depths = [p[2] / p[3] for p in rational]
    if len(set(depths)) != 1:
        raise ValueError("source is not constant depth")
    x, y = index % TILES * 8 + 4, index // TILES * 8 + 4
    if not (xy[0][0] < x < x + 1 < xy[1][0] and xy[0][1] < y < y + 1 < xy[2][1]):
        raise ValueError(
            "centre pixel is not strictly within the exact source rectangle"
        )
    bound, bits = floor32(depths[0])
    value = actual[y * SIDE + x]
    if not math.isfinite(value) or not 0 <= value <= 1:
        raise ValueError("invalid source depth output")
    return {
        "case": index,
        "scale": "calibration" if index < 16 else str(SCALES[index % len(SCALES)]),
        "pixel": [x, y],
        "bound": bound,
        "actual": value,
        "bound_bits": bits,
        "actual_bits": struct.unpack("<I", struct.pack("<f", value))[0],
        "missing": value == 0,
        "nonzero_below_bound": 0 < value < bound,
    }


def verify(folder):
    source, depth = folder / "source-quads.bin", folder / "source-depth.d32"
    if source.stat().st_size != 4096 * 64 or depth.stat().st_size != SIDE * SIDE * 4:
        raise ValueError("incorrect dump sizes")
    points = list(struct.iter_unpack("<4f", source.read_bytes()))
    actual = struct.unpack(f"<{SIDE * SIDE}f", depth.read_bytes())
    native = json.loads((folder / "result.json").read_text())
    selected = native["selected_case"]
    indices = range(4096) if selected is None else [selected]
    samples = [inspect(points[i * 4 : i * 4 + 4], actual, i) for i in indices]
    groups = {}
    for sample in samples:
        group = groups.setdefault(
            sample["scale"], {"tested": 0, "missing": 0, "nonzero_below_bound": 0}
        )
        group["tested"] += 1
        group["missing"] += sample["missing"]
        group["nonzero_below_bound"] += sample["nonzero_below_bound"]
    return {
        "folder": str(folder),
        "source_sha256": digest(source),
        "depth_sha256": digest(depth),
        "selected_case": selected,
        "exact_centre_pixels": len(samples),
        "groups": groups,
        "missing": sum(s["missing"] for s in samples),
        "nonzero_below_bound": sum(s["nonzero_below_bound"] for s in samples),
        "nonzero_examples": [s for s in samples if s["nonzero_below_bound"]],
        "missing_examples": [s for s in samples if s["missing"]][:16],
        "scope": "Independent exact source rectangle and downward depth, one whole centre pixel per case; no portable raster bound",
    }


def equivalent(raw, normalized):
    original = list(struct.iter_unpack("<4f", (raw / "source-quads.bin").read_bytes()))
    other = list(
        struct.iter_unpack("<4f", (normalized / "source-quads.bin").read_bytes())
    )
    if len(original) != len(other):
        raise ValueError("normalized source count differs")
    for a, b in zip(original, other, strict=True):
        for axis in range(3):
            if Fraction(a[axis]) / Fraction(a[3]) != Fraction(b[axis]) / Fraction(b[3]):
                raise ValueError(
                    "normalization changed exact projected coordinates or depth"
                )
    return {"vertices": len(original), "exact_ratios_compared": len(original) * 3}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    folder = args.folder.resolve()
    results = [
        verify(folder),
        *(verify(p) for p in sorted(folder.glob("isolated-*")) if p.is_dir()),
    ]
    normalized = folder / "normalized"
    comparison = None
    if normalized.is_dir():
        results.append(verify(normalized))
        comparison = equivalent(folder, normalized)
    args.output.write_text(
        json.dumps(
            {"results": results, "normalized_equivalence": comparison, "success": True},
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
