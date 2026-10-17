"""Compare filtered raw transfer factors with per-footprint refits on fixed fields."""

import argparse
import json
from pathlib import Path

import numpy as np
from joint_basis_reduction import errors, interpolate
from normal_lobe_study import coarsen
from positive_light_basis import angular_weights


def matrix_for(fields, knot_count):
    return fields[:, :knot_count].transpose(0, 2, 1).reshape(-1, knot_count)


def summary(raw, truth, mass):
    bounded = np.clip(raw, 0, mass)
    independent = errors(bounded[:, 2:], truth[:, 2:])
    independent["coverage_normalized_rmse"] = float(
        independent["rmse"] / np.sqrt(np.mean(mass**2))
    )
    return {
        "six_independent_lights": independent,
        "all_eight_queries": errors(bounded, truth),
    }


def analyse(joint, coverage, knots, held):
    cones = [angular_weights(knots, queries) for queries in held]
    matrix = matrix_for(joint, len(knots))
    u, s, vh = np.linalg.svd(matrix, full_matrices=False)
    base = {rank: (u[:, :rank] * s[:rank]).reshape(3, 256, rank) for rank in (4, 8, 12)}
    levels = []
    size = 16
    while True:
        matrix = matrix_for(joint, len(knots))
        truth = joint[:, len(knots) :].transpose(0, 2, 1).reshape(-1, held.shape[1])
        mass = coverage.reshape(-1, 1)
        full = np.concatenate(
            [matrix[v * size**2 : (v + 1) * size**2] @ w for v, w in enumerate(cones)]
        )
        row = {"size": size, "angular": summary(full, truth, mass), "signed": {}}
        cu, cs, cvh = np.linalg.svd(matrix, full_matrices=False)
        for rank, basis in base.items():
            filtered = interpolate(basis.reshape(-1, rank), vh[:rank], cones, size**2)
            actual_rank = min(rank, len(cs))
            refit = interpolate(
                cu[:, :actual_rank] * cs[:actual_rank],
                cvh[:actual_rank],
                cones,
                size**2,
            )
            row["signed"][str(rank)] = {
                "filtered_base_factors": summary(filtered, truth, mass),
                "per_level_refit": summary(refit, truth, mass),
                "refit_actual_rank": actual_rank,
                "refit_float32_bytes": (3 * size**2 + len(knots)) * actual_rank * 4,
            }
        levels.append(row)
        if size == 1:
            break
        joint = np.stack([coarsen(field.T, size).T for field in joint])
        coverage = np.stack(
            [coarsen(field[:, None], size).ravel() for field in coverage]
        )
        base = {
            rank: np.stack([coarsen(field, size) for field in basis])
            for rank, basis in base.items()
        }
        size //= 2
    return levels


def run(source, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private results must be under/tmp")
    original = json.loads((source / "report.json").read_text())
    if original["angular_coordinates"] != "view" or original["knot_count"] != 64:
        raise ValueError("aligned64 fixed source fields required")
    with np.load(source / "fields.npz") as fields:
        levels = analyse(
            np.stack([fields[f"joint{i}"] for i in range(3)]),
            np.stack([fields[f"coverage{i}"] for i in range(3)]),
            fields["knots"],
            fields["heldout"],
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            {
                "source": str(source),
                "source_sha256": original["source_sha256"],
                "levels": levels,
                "scope": "same finite16x16 source fields, nonnegative box filter, six independent lights; no new-camera/continuous-source/temporal guarantee",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.source, args.output)
