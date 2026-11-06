"""Measure the upper-quad candidate against unchanged native surface ribbons."""

import argparse
import json
from itertools import pairwise
from pathlib import Path
from time import perf_counter

import numpy as np
from native_road_parts import checked_payload, load
from upper_strip_envelope import lower_quad, upper_quad


def surface_rings(positions, indices):
    if len(positions) >= 8 and len(positions) % 4 == 0:
        stations = len(positions) // 4
        expected = np.array(
            [
                (a, a + 4, a + 1, a + 1, a + 4, a + 5)
                for station in range(stations - 1)
                for across in range(3)
                for a in [station * 4 + across]
            ],
            dtype=np.uint32,
        ).ravel()
        if np.array_equal(indices, expected):
            return positions.reshape(stations, 1, 4, 3)
    if len(positions) >= 40 and (len(positions) - 16) % 12 == 0:
        stations = (len(positions) - 16) // 12
        pattern = []
        for across in range(3):
            pattern.extend(
                [across, 12 + across, across + 1, across + 1, 12 + across, 13 + across]
            )
            pattern.extend(
                [
                    4 + across,
                    5 + across,
                    16 + across,
                    5 + across,
                    17 + across,
                    16 + across,
                ]
            )
        pattern.extend([8, 9, 20, 20, 9, 21, 10, 22, 11, 22, 23, 11])
        expected = (np.asarray(pattern) + 12 * np.arange(stations - 1)[:, None]).ravel()
        if (
            len(indices) == len(expected) + 36
            and np.array_equal(indices[:-36], expected)
            and np.all(indices[-36:] >= stations * 12)
        ):
            body = positions[: stations * 12].reshape(stations, 12, 3)
            return body[:, :8].reshape(stations, 2, 4, 3)
    return None


def run_savings(bounds, per_span, error):
    selected = np.isfinite(bounds) & (np.asarray(bounds) <= error)
    changes = np.diff(np.r_[False, selected, False].astype(int))
    lengths = np.flatnonzero(changes == -1) - np.flatnonzero(changes == 1)
    return int(per_span * np.maximum(lengths - 1, 0).sum()), int(
        lengths[lengths > 1].sum()
    )


def run(output):
    manifest, _, parts, payload, _ = load()
    all_triangles = sum(part["counts"][3] // 3 for part in parts)
    bounds, residuals, costs, unsupported = [], [], [], 0
    part_counts = {"surface": 0, "closed_shell": 0}
    original_triangles = 0
    began = perf_counter()
    for part in parts:
        if part["kind"] != "edge":
            continue
        streams = checked_payload(part, payload)
        positions = np.frombuffer(streams["positions"], dtype="<f4").reshape(-1, 3)
        indices = np.frombuffer(streams["indices"], dtype="<u4") - part["base_vertex"]
        rings = surface_rings(positions, indices)
        if rings is None:
            continue
        part_counts["surface" if rings.shape[1] == 1 else "closed_shell"] += 1
        original_triangles += len(indices) // 3
        part_bounds = []
        for first, second in pairwise(rings):
            candidates = [upper_quad(first[0], second[0])]
            if rings.shape[1] == 2:
                candidates.append(lower_quad(first[1], second[1]))
            if any(candidate is None for candidate in candidates):
                unsupported += 1
                part_bounds.append(np.inf)
                continue
            error = max(
                candidate["position_bound_m"] + candidate["ring_residual_m"]
                for candidate in candidates
            )
            bounds.append(error)
            part_bounds.append(error)
            residuals.append(
                max(candidate["ring_residual_m"] for candidate in candidates)
            )
        costs.append((np.asarray(part_bounds), 4 * rings.shape[1]))
    saved = sum(run_savings(b, rate, np.inf)[0] for b, rate in costs)
    report = {
        "archive_sha256": manifest["archive_sha256"],
        "part_counts": part_counts,
        "covered_original_triangles": original_triangles,
        "all_original_triangles": all_triangles,
        "candidate_spans": len(bounds),
        "unresolved_spans": unsupported,
        "all_candidate_triangles": all_triangles - saved,
        "bounded_error_comparisons": {
            str(error): {
                "coarsened_spans": sum(
                    run_savings(b, rate, error)[1] for b, rate in costs
                ),
                "all_candidate_triangles": all_triangles
                - sum(run_savings(b, rate, error)[0] for b, rate in costs),
            }
            for error in [0.001, 0.005, 0.02, 0.05, 0.1, 0.25]
        },
        "position_bound_m_percentiles": dict(
            zip(
                ["p50", "p95", "p99", "max"],
                np.percentile(bounds, [50, 95, 99, 100]).tolist(),
            )
        ),
        "ring_residual_max_m": max(residuals),
        "loop_wall_ms": (perf_counter() - began) * 1000,
        "scope": "Offline finite source-mesh opponent, including exact run-end contacts and unchanged sides/caps/junctions/unknown templates. No runtime Fine read, directed arithmetic, native image, continuous-curve or normal/material qualification follows.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
