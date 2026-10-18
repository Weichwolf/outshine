"""Execute one preflighted spruce form, never expand its complete physical needles."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern
from parent_profile_graph import ParentProfileGraph
from spruce_growth_probe import MAX_AXES, cached_areas, measure
from spruce_growth_product import pack_axes, publish, windows


def provenance():
    root = Path(__file__).resolve().parents[3]
    names = [
        "spruce_growth_execute.py",
        "spruce_growth_probe.py",
        "spruce_growth_product.py",
        "parent_profile_graph.py",
        "parent_growth_graph.py",
        "generator_graph.py",
        "generator_specs.py",
        "conifer_windows.py",
    ]
    paths = [Path(__file__).with_name(name) for name in names]
    paths += [
        root / "lib/src/outshine_ref/vegetation" / name
        for name in ("plant_plan.py", "needle_windows.py")
    ]
    modified = subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], cwd=root, text=True
    )
    if modified.strip():
        raise ValueError("freeze and commit the product implementation first")
    return {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=root, text=True
        ).strip(),
        "source_modified": False,
        "implementation_sha256": {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
    }


def execute(graph_recipe, physical, metadata, expected):
    graph = ParentProfileGraph.from_recipe(graph_recipe)
    patterns = tuple(
        NeedlePattern.from_recipe(p) if p else None for p in physical["patterns"]
    )
    poses, counts, reports = [], [], []
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("complete organs")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("needle positions")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("needle mesh")),
    ):
        for order in range(4):
            current = graph.instances(order + 1, MAX_AXES)
            if current is None or len(poses) + len(current) > MAX_AXES:
                raise ValueError("preflighted full axes must fit the original budget")
            local = [
                patterns[order].count(p.length) if patterns[order] else 0
                for p in current
            ]
            if (
                len(current) != expected["orders"][order]
                or sum(local) != expected["needle_counts"][order]
            ):
                raise ValueError(
                    "executed axes must exactly match the count-only prediction"
                )
            reports.append(
                {
                    "order": order,
                    "axes": len(current),
                    "needles": sum(local),
                    "frame_residual_max": max(
                        (
                            float(abs(p.frame.T @ p.frame - np.eye(3)).max())
                            for p in current
                        ),
                        default=0,
                    ),
                }
            )
            poses.extend(current)
            counts.extend(local)
            before = len(graph._poses)
            assert (
                len(graph.instances(order + 1, 0)) == len(current)
                and len(graph._poses) == before
            )
        axes = pack_axes(poses, counts)
        window_rows = windows(axes, patterns, metadata, graph.seed, expected["windows"])
    if len(poses) != expected["axes"] or len({p.path for p in poses}) != len(poses):
        raise ValueError("one complete unique stable path per physical axis required")
    replay = ParentProfileGraph.from_recipe(graph.recipe())
    for index in (0, len(poses) // 2, len(poses) - 1):
        actual = replay.instance(poses[index].path)
        np.testing.assert_array_equal(actual.origin, poses[index].origin)
        np.testing.assert_array_equal(actual.frame, poses[index].frame)
        assert actual.length == poses[index].length
    assert len(replay._poses) <= 15
    return graph.recipe(), axes, window_rows, reports


def build(graph_path, plant, catalog, needles, authoring_path, output):
    proof = provenance()
    started = time.perf_counter()
    original = json.loads((plant / "plant-recipe.json").read_text())
    physical = original["physical_source"]
    graph_recipe = json.loads(graph_path.read_text())
    metadata = json.loads((catalog / "catalog.json").read_text())
    areas, area_inputs = cached_areas(needles)
    expected = measure(graph_recipe, physical, metadata, areas)
    settings = json.loads(authoring_path.read_text())
    if (
        settings["graph_identity"] != expected["graph_identity"]
        or abs(expected["expected_area_m2"] - settings["target_area_m2"])
        / settings["target_area_m2"]
        > 0.05
    ):
        raise ValueError(
            "same preflighted graph and unchanged five-percent area admission required"
        )
    graph_recipe, axes, window_rows, orders = execute(
        graph_recipe, physical, metadata, expected
    )
    output.mkdir(parents=True, exist_ok=False)
    execution = output / "execution"
    execution.mkdir()
    np.savez_compressed(execution / "axes.npz", **axes)
    np.savez_compressed(output / "windows.npz", **window_rows)
    (execution / "graph.json").write_text(json.dumps(graph_recipe, indent=2) + "\n")
    keys = (
        "width_factor",
        "primary_whorls",
        "length_variance",
        "attachment_spread_m",
        "nominal_lai",
        "nominal_crown_width_m",
        "target_area_m2",
        "internode_variation",
        "upper_inclination_rad",
        "crown_last_parameter",
        "terminal_length_fraction",
        "crown_length_exponent",
        "upper_density_bias",
        "secondary_spacing_factor",
        "crown_span_factor",
        "secondary_divergence_rad",
        "secondary_phase",
        "secondary_normal_reference",
    )
    recipe = {
        "version": "experimental-parent-needled-2",
        "wood": graph_recipe,
        "physical_source": physical,
        "authoring": {k: settings[k] for k in keys if k in settings},
    }
    report = (
        proof
        | expected
        | {
            "orders": orders,
            "graph_identity": expected["graph_identity"],
            "source_graph_file_sha256": hashlib.sha256(
                graph_path.read_bytes()
            ).hexdigest(),
            "physical_source_file_sha256": hashlib.sha256(
                (plant / "plant-recipe.json").read_bytes()
            ).hexdigest(),
            "area_inputs_sha256": area_inputs,
            "axis_array_bytes": sum(a.nbytes for a in axes.values()),
            "window_array_bytes": sum(a.nbytes for a in window_rows.values()),
            "build_s": time.perf_counter() - started,
            "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            / 1024,
            "scope": "Complete preflighted graph and physical windows, no individual needle placement/mesh, no whole-tree atlas; source physical-needle reference is unchanged. Own new crown proportions and nominal area target, not observed botanical calibration or native/LOD admission.",
        }
    )
    (execution / "execution.json").write_text(json.dumps(report, indent=2) + "\n")
    manifest = publish(output, recipe, execution, report, catalog)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "source_commit",
                    "axes",
                    "windows",
                    "needle_counts",
                    "expected_area_m2",
                    "build_s",
                    "process_peak_rss_MiB",
                )
            }
            | {"identity": manifest["plan_identity"]}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("graph", "plant", "catalog", "needles", "authoring", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    build(
        args.graph, args.plant, args.catalog, args.needles, args.authoring, args.output
    )
