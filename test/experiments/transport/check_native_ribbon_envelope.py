"""Exact-rational operand check of the compiled native stored-span kernel."""

import argparse
import hashlib
import json
import subprocess
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

import numpy as np
from compare_upper_strip_envelope import run_savings, surface_rings
from native_road_parts import checked_payload, load


def difference(a, b):
    return tuple(x - y for x, y in zip(a, b))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross_xz(a, b):
    return a[0] * b[2] - a[2] * b[0]


def exact_source_bound(rings):
    points = [tuple(F(float(v)) for v in p) for p in rings]
    outer = [points[i] for i in [0, 3, 7, 4]]
    ac, bd, ab = (
        difference(outer[2], outer[0]),
        difference(outer[3], outer[1]),
        difference(outer[1], outer[0]),
    )
    denominator = cross_xz(ac, bd)
    t, u = cross_xz(ab, bd) / denominator, cross_xz(ab, ac) / denominator
    assert 0 < t < 1 and 0 < u < 1
    heights = (outer[0][1] + t * ac[1], outer[1][1] + u * bd[1])
    squared = F(0)
    for ring in [points[:4], points[4:]]:
        span = difference(ring[3], ring[0])
        length2 = dot(span, span)
        previous = F(-1)
        for p in ring:
            fraction = max(F(0), min(F(1), dot(difference(p, ring[0]), span) / length2))
            assert fraction > previous
            previous = fraction
            residual = tuple(
                x - (a + fraction * d) for x, a, d in zip(p, ring[0], span)
            )
            squared = max(squared, dot(residual, residual))
    return heights, squared


def run(binary, output):
    manifest, _, parts, payload, _ = load()
    cases, groups, rows = [], [], []
    all_triangles = sum(p["counts"][3] // 3 for p in parts)
    for part in parts:
        if part["kind"] != "edge":
            continue
        streams = checked_payload(part, payload)
        positions = np.frombuffer(streams["positions"], dtype="<f4").reshape(-1, 3)
        indices = np.frombuffer(streams["indices"], dtype="<u4") - part["base_vertex"]
        rings = surface_rings(positions, indices)
        if rings is None:
            continue
        begin = len(cases)
        for station in range(len(rings) - 1):
            for side in range(rings.shape[1]):
                source = np.r_[rings[station, side], rings[station + 1, side]]
                cases.append((part["edge"], station, side, source))
                rows.append(
                    str(side)
                    + " "
                    + " ".join(format(float(v), ".9g") for v in source.ravel())
                )
        groups.append((begin, len(rings) - 1, rings.shape[1]))
    encoded = "\n".join(rows) + "\n"
    process = subprocess.run(
        [str(binary), "--stdin"],
        input=encoded,
        text=True,
        capture_output=True,
        check=True,
    )
    actual = process.stdout.splitlines()
    assert len(actual) == len(cases)
    accepted, bounds = 0, []
    for case, row in zip(cases, actual):
        edge, station, side, source = case
        values = row.split()
        assert [int(v) for v in values[1:25]] == source.astype("<f4").view(
            "<u4"
        ).ravel().tolist(), (edge, station, "native stored words differ")
        if values[0] == "0":
            assert len(values) == 25
            bounds.append(np.inf)
            continue
        assert values[0] == "1" and len(values) == 33
        declared = F(float(values[25]))
        heights, residual2 = exact_source_bound(source)
        remaining = declared - abs(heights[0] - heights[1])
        assert remaining >= 0 and remaining * remaining >= 4 * residual2, (
            edge,
            station,
            side,
            "native geometric bound is too small",
        )
        diagonal = {0, 7} if (heights[0] > heights[1]) == (side == 0) else {3, 4}
        counts = Counter(map(int, values[27:]))
        if heights[0] != heights[1]:
            assert {v for v, n in counts.items() if n == 2} == diagonal, (
                edge,
                station,
                side,
                "wrong upper/lower roof",
            )
        bounds.append(float(declared))
        accepted += 1
    costs = []
    for begin, spans, sides in groups:
        costs.append(
            (
                np.asarray(bounds[begin : begin + spans * sides])
                .reshape(spans, sides)
                .max(axis=1),
                4 * sides,
            )
        )
    report = {
        "archive_sha256": manifest["archive_sha256"],
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "input_sha256": hashlib.sha256(encoded.encode()).hexdigest(),
        "output_sha256": hashlib.sha256(process.stdout.encode()).hexdigest(),
        "native_queries": len(cases),
        "accepted": accepted,
        "original_fallbacks": len(cases) - accepted,
        "all_original_triangles": all_triangles,
        "at_diagnostic_0_05m_triangles": all_triangles
        - sum(run_savings(b, rate, 0.05)[0] for b, rate in costs),
        "scope": "Exact-rational check of accepted finite stored-source bounds and diagonals. Not a native mesh, pose/raster/material or World performance admission.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.binary.resolve(), args.output)
