"""Compare persistent world-cell candidates and exact packed lookup semantics (#448)."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from export_local_light_native import inputs
from local_light_inputs import ROOT, receivers
from local_light_world import build_world, cell_positions, query_words


def verify_contracts(packed):
    directional = packed[1:2].copy()
    directional[0, 1, 3] = 100
    grid = build_world(directional)
    if grid.globals.tolist() != [0] or grid.cells:
        raise AssertionError("directional range is unused, even when positive")
    for method in ("hash", "sorted"):
        words, _ = grid.pack(method)
        got, _ = query_words(words, [[1e6, 0, 0], [-1e6, 0, 0]], 32, method)
        if not all(ids.tolist() == [0] for ids in got):
            raise AssertionError("directional IDs remain global")
        fallback = build_world(packed, index_bytes=1)
        words, _ = fallback.pack(method)
        got, _ = query_words(words, [[0, 0, 0]], 32, method)
        if not fallback.overflow or not np.array_equal(got[0], np.arange(len(packed))):
            raise AssertionError("overflow retains every original ID")
    invalid = []
    for width in (-1, 0, np.inf, np.nan):
        invalid.append((packed, {"width": width}))
    for index, value in (((0, 0, 0), np.nan), ((0, 1, 3), -1), ((0, 0, 3), 3)):
        changed = packed.copy()
        changed[index] = value
        invalid.append((changed, {}))
    invalid.append((packed, {"translation": [np.inf, 0, 0]}))
    for changed, kwargs in invalid:
        try:
            build_world(changed, **kwargs)
        except ValueError:
            continue
        raise AssertionError("invalid world-light input was accepted")
    try:
        cell_positions([[2**38, 0, 0]], 128)
    except ValueError:
        return {"invalid_inputs_rejected": len(invalid) + 1, "methods_checked": 2}
    raise AssertionError("coordinate domain was silently wrapped")


def verify_queries(grid, packed, points, words, method):
    actual, probes = query_words(words, points, grid.width, method)
    sizes = []
    for point, got, expected in zip(points, actual, grid.candidates(points)):
        if not np.array_equal(got, expected):
            raise AssertionError("packed lookup must retain every original ID in order")
        delta = packed[:, 1, :3].astype(float) - point
        distance = np.linalg.norm(delta, axis=1)
        required = np.flatnonzero(
            (packed[:, 0, 3] == 0)
            | (packed[:, 1, 3] == 0)
            | (distance * packed[:, 1, 3] <= 1)
        )
        if not np.all(np.isin(required, got)):
            raise AssertionError("a true finite-light sphere lost a receiver")
        sizes.append(len(got))
    return {
        "receiver_queries": len(points),
        "list_count_p50_p95_p99_max": np.percentile(sizes, [50, 95, 99, 100]).tolist(),
        "slot_probes_p50_p95_p99_max": np.percentile(
            probes, [50, 95, 99, 100]
        ).tolist(),
    }


def source_samples(packed):
    finite = np.flatnonzero((packed[:, 0, 3] != 0) & (packed[:, 1, 3] > 0))
    selected = finite[np.linspace(0, len(finite) - 1, min(64, len(finite))).astype(int)]
    centres = packed[selected, 1, :3].astype(float)
    radii = 1 / packed[selected, 1, 3].astype(float)
    axes = np.r_[np.eye(3), -np.eye(3)]
    return np.concatenate([centres + radii[:, None] * axis for axis in axes])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private external output required")
    args.output.mkdir(parents=True, exist_ok=False)
    _, _, packed, _, _ = inputs("small", True, "street")
    (args.output / "contracts.json").write_text(
        json.dumps(verify_contracts(packed), indent=2) + "\n"
    )
    rows = []
    for scene in ("small", "street", "dense", "overlap"):
        _, frame, packed, _, receipt = inputs(scene, True, "street")
        points = np.concatenate(
            [receivers(frame, depth, 24, 14)[0] for depth in (25, 250, 500, 5000)]
            + [source_samples(packed)]
        )
        for width in (16, 32, 64, 128):
            start = time.perf_counter()
            grid = build_world(packed, width)
            elapsed = (time.perf_counter() - start) * 1000
            for method in ("hash", "sorted"):
                start = time.perf_counter()
                words, stats = grid.pack(method)
                pack_ms = (time.perf_counter() - start) * 1000
                checks = verify_queries(grid, packed, points, words, method)
                path = args.output / f"{scene}-{width}-{method}.bin"
                words.tofile(path)
                row = {
                    "scene": scene,
                    "source_sha256": receipt["fixture_sha256"],
                    "lights": len(packed),
                    "python_build_ms": elapsed,
                    "python_pack_ms": pack_ms,
                    "words_sha256": hashlib.sha256(words.tobytes()).hexdigest(),
                    **stats,
                    **checks,
                }
                rows.append(row)
                (args.output / "results.json").write_text(
                    json.dumps(rows, indent=2) + "\n"
                )
                print(
                    scene,
                    width,
                    method,
                    stats["payload_bytes"],
                    checks["list_count_p50_p95_p99_max"],
                    checks["slot_probes_p50_p95_p99_max"],
                    flush=True,
                )


if __name__ == "__main__":
    main()
