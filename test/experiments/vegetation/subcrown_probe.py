"""Bounded skeleton pilot before leaf placement and geometry expansion."""

import argparse
import json
import resource
import time
from dataclasses import asdict
from pathlib import Path
from unittest.mock import patch

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from colonization import GrowthBudget, grow
from crown_shoots import CrownShoots
from growth_experiment import validate_plan
from oak_scaffold import OakRecipe
from subcrown_space import sample_space


def build(recipe, points, *, method="tree", budget=None):
    if budget is None:
        budget = GrowthBudget(16384, 128)
    start = time.perf_counter()
    plan = grow(
        points,
        recipe.height_m,
        0.23,
        recipe.step_m,
        2.5,
        0.38,
        method=method,
        budget=budget,
        seed=recipe.skeleton(),
    )
    validate_plan(plan, recipe.root_radius_m)
    seed = recipe.skeleton()
    for key, original in zip(("positions", "parents"), seed, strict=True):
        np.testing.assert_array_equal(plan[key][: len(original)], original)
    variants = (np.arange(recipe.leaves, dtype=np.uint32) % 8).astype(np.uint8)
    with (
        patch("outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError),
        patch("outshine_ref.vegetation.Shoot.detail", side_effect=AssertionError),
    ):
        crown = CrownShoots(
            plan["positions"],
            plan["parents"],
            variants,
            recipe.seed,
            length_m=0.35,
            bend_rad=0.4,
            foliage_depth_m=1.1,
        )
        summary = crown.summary()
    if crown._groups:
        raise AssertionError("premature foliage poses")
    metrics = {
        **{k: v for k, v in plan.items() if not isinstance(v, np.ndarray)},
        "unreached_attractors": len(plan["remaining"]),
        "skeleton_nodes": len(plan["parents"]),
        "source_seed_nodes": len(seed[1]),
        "skeleton_bounds_m": [
            plan["positions"].min(0).tolist(),
            plan["positions"].max(0).tolist(),
        ],
        "skeleton_array_bytes": plan["positions"].nbytes + plan["parents"].nbytes,
        "summary": summary,
        "identity": crown.identity,
        "build_s": time.perf_counter() - start,
    }
    return plan, crown, metrics


def plot(plans, points, output):
    figure, axes = plt.subplots(
        2, len(plans), figsize=(6.4 * len(plans), 14.4), constrained_layout=True
    )
    for column, name in enumerate(plans):
        plan = plans[name]
        p, parents = plan["positions"], plan["parents"]
        segments = np.stack((p[parents[1:]], p[1:]), axis=1)
        for row, horizontal in enumerate((0, 1)):
            from matplotlib.collections import LineCollection

            ax = axes[row, column]
            ax.scatter(
                points[name][:, horizontal],
                points[name][:, 2],
                s=1,
                alpha=0.16,
                color="green",
            )
            ax.add_collection(
                LineCollection(
                    segments[:, :, [horizontal, 2]], linewidths=0.4, colors="#563d27"
                )
            )
            ax.set(
                xlim=(-8, 8),
                ylim=(0, 19),
                aspect="equal",
                xlabel="XY"[horizontal] + " (m)",
                ylabel="Z (m)",
                title=name,
            )
    figure.savefig(output, dpi=100)
    plt.close(figure)


def run(output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    recipe = OakRecipe()
    spaces = {"original": recipe.growth_space()}
    for placement in ("raised", "limb"):
        sampled, owners, cohorts = sample_space(recipe, placement)
        spaces[placement] = sampled
        np.savez_compressed(
            output / f"{placement}-cohorts.npz", owners=owners, **cohorts
        )
    plans, products = {}, {}
    for name, points in spaces.items():
        plan, _, metrics = build(recipe, points)
        plans[name], products[name] = plan, metrics
        np.savez_compressed(output / f"{name}.npz", points=points, **plan)
    with np.load(
        Path(__file__).with_name("data") / "crown682/spatial/skeleton.npz"
    ) as fixed:
        for key in ("positions", "parents"):
            np.testing.assert_array_equal(plans["original"][key], fixed[key])
    plot(plans, spaces, output / "skeletons.png")
    result = {
        "recipe": asdict(recipe),
        "products": products,
        "baseline_fixed_source_exact": True,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
