"""Whole-prefix comparison for existing #252; no native/device admission."""

import argparse
import hashlib
import json
import resource
import subprocess
import tempfile
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from air_path_cases import place_cases
from air_path_depth_table import depth_limit
from air_path_integral import engine_clear_air
from air_path_prefix import AirCells, AirPrefix, build_cells, optical_depth
from air_prefix_rules import CellRule

FLOOR = 1e-5
BOUND = 0.5e-6 * np.exp(0.001)
MODES = ("four", "moment", "exact32", "exact64")


def prepared_cases(root):
    medium_path = root / "src/world/sky/Atmosphere.h"
    medium = engine_clear_air(medium_path)
    cases, _paths = place_cases(root)

    for case in cases:
        limit = depth_limit(medium, case.height_km, case.arguments()[1], 200.0)
        cells = build_cells(medium, case.arguments(), limit, 19, "quadratic")
        yield case.name, cells, {"ray": case.record(), "kind": "clear-gas model"}
    edges = np.array([0.0, 0.01, 0.1, 1.0, 10.0])
    for name, sigma, source in (
        ("homogeneous", [0, 1e-5, 0.03], [0.02, 0.03, 0.04]),
        ("vacuum", [0, 0, 0], [0, 0, 0]),
        (
            "threshold",
            [np.nextafter(np.float32(0.001), np.float32(0)), 0.001, 0.002],
            [1e-4, 2e-4, 3e-4],
        ),
    ):
        extinction = np.broadcast_to(sigma, (4, 3, 3)).copy()
        emission = np.broadcast_to(source, (4, 3, 3)).copy()
        yield name, AirCells(edges.copy(), extinction, emission, 0), {"kind": name}
    controls = np.array([1e-5, 3e-5, 5e-5])[:, None] * [1.0, 2.0, 4.0]
    extinction = np.broadcast_to(controls, (4, 3, 3)).copy()
    emission = extinction * [0.25, 0.5, 1.0]
    yield (
        "source-proportional",
        AirCells(edges.copy(), extinction, emission, 0),
        {"kind": "source-proportional"},
    )


def query_distances(edges):
    end = edges[-1]
    step = min(1e-6, float(np.diff(edges).min()) / 4)
    distances = np.unique(
        np.r_[
            np.linspace(0, end, 129),
            edges,
            np.maximum(0, edges - step),
            np.minimum(end, edges + step),
        ]
    )
    return np.repeat(distances, 2)


def run_mode(cells, distances, mode):
    rule = CellRule(mode)
    began = time.perf_counter()
    with patch("air_path_prefix.cell_integral", rule):
        prefix = AirPrefix(cells)
        values = [prefix.advance(float(distance)) for distance in distances]
    light, through = (np.asarray([v[i] for v in values]) for i in (0, 1))
    return light, through, rule.work, time.perf_counter() - began


def check_case(name, cells, distances, values, kind):
    reference, reference_t = values["exact64"][:2]
    scale = np.maximum(reference, FLOOR)
    fine_delta = float((abs(reference - values["exact32"][0]) / scale).max())
    if fine_delta > 1e-10:
        raise ValueError((name, "thin-part reference not converged", fine_delta))
    moment_error = float((abs(reference - values["moment"][0]) / scale).max())
    if moment_error > BOUND + 2e-14:
        raise ValueError((name, "composed moment bound", moment_error, BOUND))
    difference = float(
        (
            abs(values["four"][0] - values["moment"][0])
            / np.maximum(values["four"][0], FLOOR)
        ).max()
    )
    if difference > 1e-5:
        raise ValueError((name, "unchanged native radiance tolerance", difference))
    for mode, (light, through, _, _) in values.items():
        if (
            not np.isfinite(light).all()
            or not np.isfinite(through).all()
            or (light < 0).any()
            or (through < 0).any()
            or (through > 1).any()
        ):
            raise ValueError((name, mode, "invalid transport"))
        np.testing.assert_array_equal(through, reference_t)
        np.testing.assert_array_equal(light[::2], light[1::2])
        np.testing.assert_array_equal(through[::2], through[1::2])
    if kind in ("homogeneous", "vacuum", "threshold"):
        beta, source = cells.extinction[0, 0], cells.emission[0, 0]
        t = np.exp(-distances[:, None] * beta)
        expected = (
            np.divide(
                -np.expm1(-distances[:, None] * beta),
                beta,
                out=np.broadcast_to(distances[:, None], t.shape).copy(),
                where=beta > 0,
            )
            * source
        )
        for mode in MODES:
            np.testing.assert_allclose(
                values[mode][0], expected, rtol=BOUND + 2e-12, atol=1e-14
            )
            np.testing.assert_allclose(values[mode][1], t, rtol=2e-14, atol=1e-14)
    if kind == "source-proportional":
        cumulative = np.vstack(
            (
                np.zeros(3),
                np.cumsum(
                    np.diff(cells.edges)[:, None] * cells.extinction.sum(axis=1) / 3,
                    axis=0,
                ),
            )
        )
        expected = []
        for distance in distances:
            index = min(
                np.searchsorted(cells.edges, distance, side="right") - 1,
                len(cells.edges) - 2,
            )
            width = cells.edges[index + 1] - cells.edges[index]
            depth = cumulative[index] + optical_depth(
                cells.extinction[index], (distance - cells.edges[index]) / width, width
            )
            expected.append(-np.expm1(-depth) * [0.25, 0.5, 1.0])
        for mode in MODES:
            np.testing.assert_allclose(
                values[mode][0], expected, rtol=2e-12, atol=1e-14
            )
    return {
        "thin_reference_refinement_relative": fine_delta,
        "moment_vs_hybrid_relative": moment_error,
        "moment_vs_four_relative": difference,
        "transmittance_bits_equal": True,
        "repeated_queries_bits_equal": True,
    }


def compare(output, root):
    reports = []
    for index, (name, original, metadata) in enumerate(prepared_cases(root)):
        cells = AirCells(
            original.edges,
            original.extinction.astype("<f4").astype(float),
            original.emission.astype("<f4").astype(float),
            original.source_evaluations,
        )
        distances = query_distances(cells.edges)
        values = {mode: run_mode(cells, distances, mode) for mode in MODES}
        checks = check_case(name, cells, distances, values, metadata["kind"])
        # The unchanged public Python prefix is a separate before control.
        before = AirPrefix(cells)
        legacy = [before.advance(float(d)) for d in distances]
        for channel in (0, 1):
            np.testing.assert_array_equal(
                np.asarray([v[channel] for v in legacy]), values["four"][channel]
            )
        saved = {
            "edges": cells.edges,
            "extinction": cells.extinction,
            "emission": cells.emission,
            "distances": distances,
        }
        for mode, (light, through, _, _) in values.items():
            saved[mode + "_L"], saved[mode + "_T"] = light, through
        np.savez_compressed(output / f"case-{index}.npz", **saved)
        report = {
            "case": name,
            **metadata,
            "cells": len(cells.edges) - 1,
            "queries": len(distances),
            "source_evaluations": cells.source_evaluations,
            "coefficient_array_bytes": cells.extinction.nbytes
            + cells.emission.nbytes
            + cells.edges.nbytes,
            "checks": checks,
            "rules": {
                mode: {"work": row[2], "seconds": row[3]}
                for mode, row in values.items()
            },
        }
        reports.append(report)
        print(name, checks, flush=True)
    return reports


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, root = args.output.resolve(), args.input_root.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if output == temporary or not output.is_relative_to(temporary) or output.exists():
        parser.error("fresh explicit system-temp subdirectory required")
    output.mkdir(parents=True)
    paths = [root / "src/world/sky/Atmosphere.h", *place_cases(root)[1]]
    sources = [
        Path(__file__).with_name(p)
        for p in (
            "air_path_cases.py",
            "air_path_depth_table.py",
            "air_path_integral.py",
            "air_path_segments.py",
            "air_path_prefix.py",
            "compare_air_cell_rules.py",
            "air_prefix_rules.py",
            Path(__file__).name,
        )
    ]
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*paths, *sources]
        },
        "threshold": 0.001,
        "relative_moment_bound": float(BOUND),
        "scope": "FP64 whole-prefix arithmetic with FP32-quantized positive coefficients and FP64 edges/queries. exact32/64 use converged RTE only for thin channels; thick channels retain four-node model. No source-fit, complete thick RTE, native/GPU/FP32 arithmetic, Mie/Psi/LUT, image or device admission.",
        "cases": compare(output, root),
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
