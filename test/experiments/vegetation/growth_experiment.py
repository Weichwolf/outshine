#!/usr/bin/env python3
"""Compare identical plant growth with brute-force and indexed nearest-node queries."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from colonization import GrowthBudget, crown_points, grow, pipe_radii
from nearest import brute_nearest, tree_nearest


def counterexamples():
    nodes = np.array([[-1.0, 0, 0], [1.0, 0, 0], [-1.0, 0, 0]])
    points = np.array([[0.0, 0, 0], [-1.0, 0, 0], [3.0, 0, 0], [0.0, 2, 0]])
    for radius in (1.0, 2.0, 4.0):
        a, da = brute_nearest(points, nodes, radius)
        b, db = tree_nearest(points, nodes, radius)
        np.testing.assert_array_equal(a, b)
        np.testing.assert_array_equal(da, db)
    for method in (brute_nearest, tree_nearest):
        indices, _ = method(np.empty((0, 3)), nodes[:1], 1)
        assert not len(indices)
        assert method(points[:1], nodes[:1], 1)[0][0] == 0
    try:
        pipe_radii(np.array([-1, 2, 1]), 0.1)
    except ValueError:
        pass
    else:
        raise AssertionError("cyclic branch parents accepted")
    distant = np.array([[0.0, 0.0, 5.0]])
    for budget, reason in (
        (GrowthBudget(2, 8), "node_budget"),
        (GrowthBudget(16, 1), "iteration_budget"),
    ):
        plan = grow(distant, 2.0, 0.5, 0.1, 10.0, 0.1, budget=budget)
        assert plan["terminal_reason"] == reason
        assert len(plan["positions"]) <= budget.nodes
        assert plan["iterations"] <= budget.iterations
        assert len(plan["remaining"]) == 1
        validate_plan(plan, 0.1)
    for invalid in (0.0, np.nan, np.inf):
        try:
            grow(distant, invalid, 0.5, 0.1, 10.0, 0.1)
        except ValueError:
            pass
        else:
            raise AssertionError("invalid plant height accepted")
    return {
        "exact_ties": True,
        "duplicate_nodes": True,
        "radius_boundary": True,
        "empty_targets": True,
        "single_node": True,
        "cyclic_plan_rejected": True,
        "bounded_partial_plans": True,
        "invalid_height_rejected": True,
    }


def validate_plan(plan, radius):
    positions, parents = plan["positions"], plan["parents"]
    assert np.all(np.isfinite(positions))
    assert parents[0] == -1 and np.all(parents[1:] < np.arange(1, len(parents)))
    lengths = np.linalg.norm(positions[1:] - positions[parents[1:]], axis=1)
    assert np.all(lengths > 0)
    radii = pipe_radii(parents, radius)
    areas = radii * radii
    total = np.bincount(parents[1:], weights=areas[1:], minlength=len(parents))
    forks = total > 0
    np.testing.assert_allclose(total[forks], areas[forks], rtol=1e-12, atol=1e-15)
    assert np.all(radii[1:] <= radii[parents[1:]])
    return radii


def run_case(profile, count, seed, budget, output, name):
    height, spread, bole = (
        profile["height_m"],
        profile["spread_m"],
        profile["bole_frac"],
    )
    radius = profile["dbh_cm"] / 200
    step = min(height * 0.025, spread * 0.065)
    influence, kill = spread * 0.38, step * 1.3
    points = crown_points(height, spread, bole, count, seed)
    results, timing = {}, {}
    for method in ("brute", "tree"):
        began = time.perf_counter()
        result = grow(points, height, bole, step, influence, kill, method, budget)
        timing[method] = (time.perf_counter() - began) * 1000
        results[method] = result
        validate_plan(result, radius)
    np.testing.assert_array_equal(
        results["brute"]["parents"], results["tree"]["parents"]
    )
    np.testing.assert_allclose(
        results["brute"]["positions"], results["tree"]["positions"], rtol=0, atol=1e-12
    )
    result = results["tree"]
    radii = validate_plan(result, radius)
    stem = f"{name}-{count}-{seed}"
    np.savez_compressed(
        output / f"{stem}.npz",
        positions=result["positions"],
        parents=result["parents"],
        radii=radii,
        attractors=points,
        remaining=result["remaining"],
    )
    return {
        "species": name,
        "attractors": count,
        "seed": seed,
        "nodes": len(result["positions"]),
        "dimensions_m": [height, spread],
        "consumed": result["consumed"],
        "remaining": len(result["remaining"]),
        "iterations": result["iterations"],
        "terminal_reason": result["terminal_reason"],
        "elapsed_ms": timing,
        "arrays_bytes": sum(
            a.nbytes for a in (result["positions"], result["parents"], radii)
        ),
        "identical_plan": True,
        "pipe_area_conserved": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--species", nargs="+", default=["oak", "beech", "birch"])
    parser.add_argument("--counts", type=int, nargs="+", default=[256, 1024])
    parser.add_argument("--seeds", type=int, nargs="+", default=[2, 7, 19])
    parser.add_argument("--nodes", type=int, default=4096)
    parser.add_argument("--iterations", type=int, default=128)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"counterexamples": counterexamples(), "cases": []}
    for name in args.species:
        source = Path("src/assets/world/species") / f"{name}.json"
        profile = json.loads(source.read_text())
        for count in args.counts:
            for seed in args.seeds:
                result = run_case(
                    profile,
                    count,
                    seed,
                    GrowthBudget(args.nodes, args.iterations),
                    args.output,
                    name,
                )
                result["profile_sha256"] = hashlib.sha256(
                    source.read_bytes()
                ).hexdigest()
                report["cases"].append(result)
                print(json.dumps(result, separators=(",", ":")), flush=True)
    report["implementation_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in Path(__file__).parent.glob("*.py")
    }
    report["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    report["source_modified"] = bool(
        subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
    )
    report["process_peak_rss_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    (args.output / "growth.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
