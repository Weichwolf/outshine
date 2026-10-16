"""Exact spatial-growth alternatives and avoided whole-foliage placement work."""

import argparse
import json
import os
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import numpy as np
from colonization import GrowthBudget, grow
from crown_shoots import CrownShoots
from growth_experiment import counterexamples, validate_plan
from outshine_ref.vegetation import Shoot


def restore(inputs):
    recipe = json.loads((inputs / "spatial-plan.json").read_text())
    if (
        recipe.pop("format") != "spatial-crown-shoots-2"
        or recipe.pop("numpy") != np.__version__
    ):
        raise ValueError("same-version saved crown recipe required")
    with np.load(inputs / "spatial-plan.npz", allow_pickle=False) as data:
        return CrownShoots(
            data["positions"], data["parents"], data["variants"], **recipe
        )


def operate(crown, mode):
    if mode == "summary":
        with patch.object(
            Shoot, "detail", side_effect=AssertionError("unrequested organ poses")
        ):
            return crown.summary()
    if mode == "one":
        return crown.group(int(crown.hosts[-1]), 100000)
    return crown.tables(100000)


def foliage_costs(inputs):
    rows = []
    for repeat in range(4):
        modes = (
            ("summary", "one", "all") if repeat % 2 == 0 else ("all", "one", "summary")
        )
        for mode in modes:
            crown = restore(inputs)
            start = time.perf_counter()
            operate(crown, mode)
            rows.append(
                {
                    "mode": mode,
                    "repeat": repeat,
                    "seconds": time.perf_counter() - start,
                    "requested_groups": len(crown._groups),
                    "placed_leaves": sum(len(v["ids"]) for v in crown._groups.values()),
                    "retained_array_bytes": sum(
                        a.nbytes
                        for group in crown._groups.values()
                        for a in group.values()
                    ),
                }
            )
    peaks = {}
    for mode in ("summary", "one", "all"):
        crown = restore(inputs)
        tracemalloc.start()
        operate(crown, mode)
        live, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            r.count for r in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        peaks[mode] = {"live_bytes": live, "peak_bytes": peak, "live_blocks": blocks}
    return rows, peaks


def growth_costs(inputs):
    with np.load(inputs / "spatial-plan.npz", allow_pickle=False) as data:
        points = data["attractors"]
        expected_positions = data["positions"]
        expected_parents = data["parents"]
    rows = []
    for method in ("brute", "tree", "tree", "brute"):
        start = time.perf_counter()
        result = grow(
            points, 8.0, 0.23, 0.16, 1.75, 0.22, method, GrowthBudget(4096, 128)
        )
        elapsed = time.perf_counter() - start
        np.testing.assert_array_equal(result["parents"], expected_parents)
        np.testing.assert_allclose(
            result["positions"], expected_positions, rtol=0, atol=1e-12
        )
        validate_plan(result, 0.14)
        rows.append(
            {
                "method": method,
                "seconds": elapsed,
                "query_points": result["queries"],
                "nodes": len(result["parents"]),
                "iterations": result["iterations"],
                "remaining": len(result["remaining"]),
                "terminal_reason": result["terminal_reason"],
            }
        )
    return rows


def run(inputs, output):
    load = os.getloadavg()
    controls = counterexamples()
    growth = growth_costs(inputs)
    foliage, peaks = foliage_costs(inputs)
    result = {
        "growth": growth,
        "foliage": foliage,
        "tracemalloc": peaks,
        "existing_growth_controls": controls,
        "load_start": load,
        "load_end": os.getloadavg(),
        "scope": "Same actual attractors/skeleton for Brute/KDTree. Same prepared crown for summary/one/all; latter compare different explicit demands, not equal-work competitors. Summary creates no poses; array bytes/live allocations exclude native allocator/RSS and total copy traffic. Offline CPU only.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.inputs, args.output)
