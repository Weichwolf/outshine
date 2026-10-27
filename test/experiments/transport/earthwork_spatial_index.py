"""Compare stable occupied-cell lookup with the native dense-extent failure."""

import argparse
import bisect
import json
import time
from pathlib import Path

import numpy as np
from canonical_port_native_inputs import load_source
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame

CELL_M = 32.0
MAX_DENSE_CELLS = 1 << 22
MAX_SUPPORT_ENTRIES = 64


def entries(bounds):
    lower = np.floor(bounds[:, :2] / CELL_M).astype(np.int64)
    upper = np.floor(bounds[:, 2:] / CELL_M).astype(np.int64)
    keys, stamps = [], []
    for stamp, (low, high) in enumerate(zip(lower, upper, strict=True)):
        for east in range(int(low[0]), int(high[0]) + 1):
            for north in range(int(low[1]), int(high[1]) + 1):
                keys.append((east, north))
                stamps.append(stamp)
    return keys, stamps, lower, upper


def sorted_lookup(keys, stamps, points):
    order = sorted(range(len(keys)), key=lambda i: (keys[i], i))
    ordered_stamps = [stamps[i] for i in order]
    ordered_keys = sorted(keys)
    output = []
    for point in points:
        key = tuple(np.floor(point / CELL_M).astype(np.int64))
        low = bisect.bisect_left(ordered_keys, key)
        high = bisect.bisect_right(ordered_keys, key)
        output.append(tuple(ordered_stamps[low:high]))
    return output


def bounded_lookup(bounds, points):
    lower = np.floor(bounds[:, :2] / CELL_M).astype(np.int64)
    upper = np.floor(bounds[:, 2:] / CELL_M).astype(np.int64)
    levels = {}
    for stamp, (low, high) in enumerate(zip(lower, upper, strict=True)):
        level = 0
        while int(np.prod((high >> level) - (low >> level) + 1)) > MAX_SUPPORT_ENTRIES:
            level += 1
            if level >= 63:
                raise ValueError("support exceeds the signed cell-coordinate range")
        keys, owners = levels.setdefault(level, ([], []))
        first, last = low >> level, high >> level
        for east in range(int(first[0]), int(last[0]) + 1):
            for north in range(int(first[1]), int(last[1]) + 1):
                keys.append((east, north))
                owners.append(stamp)
    indexed = {}
    for level, (keys, owners) in levels.items():
        order = sorted(range(len(keys)), key=lambda i: (keys[i], i))
        indexed[level] = sorted(keys), [owners[i] for i in order]
    output, visits = [], 0
    for point in points:
        cell = np.floor(point / CELL_M).astype(np.int64)
        candidates = []
        for level, (keys, owners) in indexed.items():
            key = tuple(cell >> level)
            first = bisect.bisect_left(keys, key)
            last = bisect.bisect_right(keys, key)
            if level == 0:
                candidates.extend(owners[first:last])
                visits += last - first
                continue
            for stamp in owners[first:last]:
                visits += 1
                if np.all(lower[stamp] <= cell) and np.all(cell <= upper[stamp]):
                    candidates.append(stamp)
        output.append(tuple(sorted(candidates)))
    return output, {
        "levels": sorted(levels),
        "bounded_entries": sum(len(keys) for keys, _owners in levels.values()),
        "coarse_candidate_visits": visits,
    }


def compare_bounded(name, bounds, points):
    began = time.perf_counter()
    actual, work = bounded_lookup(bounds, points)
    elapsed = time.perf_counter() - began
    lower = np.floor(bounds[:, :2] / CELL_M).astype(np.int64)
    upper = np.floor(bounds[:, 2:] / CELL_M).astype(np.int64)
    for point, candidates in zip(points, actual, strict=True):
        cell = np.floor(point / CELL_M).astype(np.int64)
        expected = tuple(
            np.flatnonzero(np.all((lower <= cell) & (cell <= upper), axis=1))
        )
        assert candidates == expected, (name, point, candidates, expected)
    assert work["bounded_entries"] <= MAX_SUPPORT_ENTRIES * len(bounds)
    return {
        "name": name,
        "stamps": len(bounds),
        "queries": len(points),
        "fine_materialized_entries": sum(map(int, np.prod(upper - lower + 1, axis=1))),
        "candidate_visits": sum(map(len, actual)),
        "sorted_mismatched_queries": 0,
        "python_bounded_build_and_query_ms": elapsed * 1000,
        **work,
    }


def compare(name, bounds, points):
    started = time.perf_counter()
    keys, stamps, lower, upper = entries(bounds)
    emitted_at = time.perf_counter()
    actual = sorted_lookup(keys, stamps, points)
    sorted_at = time.perf_counter()
    expected = []
    for point in points:
        cell = np.floor(point / CELL_M).astype(np.int64)
        expected.append(
            tuple(np.flatnonzero(np.all((lower <= cell) & (cell <= upper), axis=1)))
        )
    checked_at = time.perf_counter()
    assert actual == expected, name
    dense_cells = (
        int(np.prod(upper.max(axis=0) - lower.min(axis=0) + 1)) if len(bounds) else 0
    )
    baseline = expected if dense_cells <= MAX_DENSE_CELLS else [()] * len(points)
    return {
        "name": name,
        "stamps": len(bounds),
        "queries": len(points),
        "occupied_entries": len(keys),
        "distinct_cells": len(set(keys)),
        "enclosing_dense_cells": dense_cells,
        "baseline_mismatched_queries": sum(
            a != b for a, b in zip(baseline, expected, strict=True)
        ),
        "sorted_mismatched_queries": 0,
        "candidate_visits": sum(map(len, actual)),
        "brute_stamp_visits": len(bounds) * len(points),
        "native_final_array_payload_bytes": 4 * len(keys)
        + (8 * len(keys) if dense_cells > MAX_DENSE_CELLS else 4 * (dense_cells + 1))
        if keys
        else 0,
        "dense_offsets_payload_bytes": 4 * (dense_cells + 1),
        "python_emit_ms": 1000 * (emitted_at - started),
        "python_sort_and_query_ms": 1000 * (sorted_at - emitted_at),
        "python_brute_query_ms": 1000 * (checked_at - sorted_at),
    }


def actual_source():
    source, manifest = load_source()
    ids = {node.identifier: i for i, node in enumerate(source.nodes)}
    geographic = np.array([node.longitude_latitude for node in source.nodes])
    points = positions_in_frame(geographic[0], geographic, np.zeros(len(geographic)))[
        :, :2
    ]
    bounds = []
    for span in source.spans:
        endpoints = points[[ids[node] for node in span.nodes]]
        reach = span.width_m * 0.5 + 20.0
        bounds.append(
            np.r_[endpoints.min(axis=0) - reach, endpoints.max(axis=0) + reach]
        )
    return np.array(bounds), points, manifest["native_raw_sha256"]


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    reports = []
    for offset in (-100000.0, 100000.0):
        bounds = np.array(
            [[0, 0, 4, 4], [2, 0, 6, 4], [offset, offset, offset + 4, offset + 4]]
        )
        points = np.array([[1, 1], [3, 1], [offset + 1, offset + 1], [offset / 2] * 2])
        row = compare(f"sparse-{offset:g}", bounds, points)
        assert row["baseline_mismatched_queries"] == 3
        reports.append(row)
        reports.append(compare(f"reversed-{offset:g}", bounds[::-1], points))
    reports.append(compare("empty", np.empty((0, 4)), np.array([[0, 0]])))
    rng = np.random.default_rng(1494)
    low = rng.integers(-240000, 240000, (1000, 2)).astype(float)
    bounds = np.c_[low, low + rng.integers(1, 80, (1000, 2))]
    points = np.concatenate((low, bounds[:, 2:], (low + bounds[:, 2:]) / 2))
    reports.append(compare("world-radius-boundaries", bounds, points))
    bounds, points, source_hash = actual_source()
    reports.append(compare("actual-source-segment-aabbs", bounds, points))
    bounded = [compare_bounded("actual-source-segment-aabbs", bounds, points)]
    large = np.array(
        [[0, 0, 70000, 70000], [-100000, -100000, -30000, -30000], [0, 0, 4, 4]]
    )
    queries = np.array(
        [[1, 1], [69999, 69999], [70016, 1], [-99999, -99999], [-29984, -30000], [0, 0]]
    )
    bounded.append(compare_bounded("large-supports", large, queries))
    bounded.append(compare_bounded("large-supports-reversed", large[::-1], queries))
    bounded.append(compare_bounded("empty", np.empty((0, 4)), np.array([[0, 0]])))
    output = {
        "cell_m": CELL_M,
        "max_dense_cells": MAX_DENSE_CELLS,
        "source_sha256": source_hash,
        "actual_input_scope": "native source axis bounds, half width plus explicit 20m apron",
        "limits": "candidate lookup only; no height solver, native timing or allocator peak",
        "reports": reports,
        "max_entries_per_support": MAX_SUPPORT_ENTRIES,
        "bounded_reports": bounded,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps(output))


if __name__ == "__main__":
    run()
