"""Compare native bounded spatial growth with the independent Python procedure."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "vegetation"))
import colonization
from colonization import GrowthBudget, crown_points, grow
from oak_scaffold import OakRecipe

REASONS = (
    "iteration_budget",
    "node_budget",
    "all_attractors_consumed",
    "no_attractor_in_reach",
    "cancelled_growth_directions",
    "occupied_growth_endpoints",
)


def cases():
    seed = (np.array([[0.0, 0, 0], [0, 0, 1.0]]), np.array([-1, 0], np.int32))
    for name, points, nodes, iterations, kill in (
        ("node-cap", [[0, 0, 5]], 2, 8, 0.1),
        ("iteration-cap", [[0, 0, 5]], 16, 1, 0.1),
        ("empty", [], 16, 8, 0.1),
        ("consumed", [[0, 0, 1.2]], 64, 1, 0.15),
        ("cancelled", [[-1, 0, 1], [1, 0, 1]], 64, 8, 0.1),
        ("occupied", [[-1, 0, 1.01], [1, 0, 1.01]], 64, 128, 0.1),
        ("unreachable", [[0, 0, 100]], 16, 8, 0.1),
        ("duplicate-attractors", [[0, 0, 1], [0, 0, 1], [0, 0, 8]], 16, 8, 0.1),
    ):
        yield (
            name,
            np.asarray(points, float).reshape((-1, 3)),
            seed,
            nodes,
            iterations,
            0.1,
            10.0,
            kill,
        )
    for number in range(12):
        points = crown_points(5, 3, 0.23, 64, number)
        yield f"small-{number}", points, seed, 512, 48, 0.2, 2.5, 0.3
    for number in (682, 718):
        recipe = OakRecipe(seed=number)
        for limbs in (False, True):
            yield (
                f"oak-{number}-limbs-{int(limbs)}",
                recipe.growth_space(),
                recipe.skeleton(limbs),
                16384,
                128,
                recipe.step_m,
                2.5,
                0.38,
            )


def encode(points, seed, nodes, iterations, step, influence, kill):
    positions, parents = seed
    lines = [
        f"{len(parents)} {len(points)} {nodes} {iterations} {step} {influence} {kill}"
    ]
    lines.extend(
        f"{parent} {position[0]:.17g} {position[1]:.17g} {position[2]:.17g}"
        for parent, position in zip(parents, positions, strict=True)
    )
    lines.extend(" ".join(format(value, ".17g") for value in point) for point in points)
    return "\n".join(lines) + "\n"


def remaining_ids(points, remaining):
    ids = []
    next_row = 0
    for row, point in enumerate(points):
        if next_row < len(remaining) and np.array_equal(point, remaining[next_row]):
            ids.append(row)
            next_row += 1
    assert next_row == len(remaining)
    return ids


def compare(binary, case, output):
    name, points, seed, nodes, iterations, step, influence, kill = case
    text = encode(points, seed, nodes, iterations, step, influence, kill)
    (output / f"{name}.input").write_text(text)
    result = subprocess.run(
        [str(binary)],
        input=text,
        text=True,
        capture_output=True,
        check=True,
        timeout=30,
    )
    (output / f"{name}.native").write_text(result.stdout)
    lines = result.stdout.splitlines()
    header = lines[0].split()
    count, left, reason, consumed, actual_iterations, queries, rejected = map(
        int, header[1:8]
    )
    actual_parents = np.array([int(row.split()[1]) for row in lines[1 : count + 1]])
    actual_positions = np.array(
        [list(map(float, row.split()[2:])) for row in lines[1 : count + 1]]
    )
    actual_remaining = list(map(int, lines[count + 1].split()[1:]))
    started = time.perf_counter()
    brute_distance_checks = 0
    original_nearest = colonization.tree_nearest

    def counted_nearest(targets, positions, radius):
        nonlocal brute_distance_checks
        brute_distance_checks += len(targets) * len(positions)
        return original_nearest(targets, positions, radius)

    with patch.object(colonization, "tree_nearest", counted_nearest):
        reference = grow(
            points,
            18,
            0.23,
            step,
            influence,
            kill,
            budget=GrowthBudget(nodes, iterations),
            seed=seed,
        )
    python_ms = (time.perf_counter() - started) * 1000
    if len(points) <= 64:
        brute = grow(
            points,
            18,
            0.23,
            step,
            influence,
            kill,
            "brute",
            GrowthBudget(nodes, iterations),
            seed=seed,
        )
        np.testing.assert_array_equal(reference["parents"], brute["parents"])
        np.testing.assert_array_equal(reference["positions"], brute["positions"])
    np.testing.assert_array_equal(actual_parents, reference["parents"])
    np.testing.assert_allclose(
        actual_positions, reference["positions"], rtol=0, atol=1e-9
    )
    np.testing.assert_array_equal(actual_positions[: len(seed[0])], seed[0])
    assert actual_remaining == remaining_ids(points, reference["remaining"])
    assert left == len(actual_remaining) and consumed + left == len(points)
    assert REASONS[reason] == reference["terminal_reason"]
    assert (consumed, actual_iterations, queries, rejected) == tuple(
        reference[key]
        for key in ("consumed", "iterations", "queries", "rejected_endpoints")
    )
    np.savez_compressed(output / f"{name}.python.npz", **reference)
    return {
        "case": name,
        "nodes": count,
        "unreached": left,
        "consumed": consumed,
        "iterations": actual_iterations,
        "reason": REASONS[reason],
        "queries": queries,
        "radius_queries": int(header[8]),
        "distance_checks": int(header[9]),
        "brute_nearest_distance_checks": brute_distance_checks,
        "partition_comparisons": int(header[10]),
        "requested_array_bytes": int(header[11]),
        "native_ms": float(header[12]),
        "python_ms": python_ms,
        "position_max_error_m": float(
            np.max(np.abs(actual_positions - reference["positions"]))
        ),
        "input_sha256": hashlib.sha256(text.encode()).hexdigest(),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    receipt = {
        "source_head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "numpy": np.__version__,
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "scope": "finite same-input growth comparison; no surface, radiance, LOD or device claim",
        "position_comparison_atol_m": 1e-9,
        "cases": [],
    }
    for case in cases():
        row = compare(args.binary.resolve(), case, args.output)
        receipt["cases"].append(row)
        (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(json.dumps(row), flush=True)


if __name__ == "__main__":
    main()
