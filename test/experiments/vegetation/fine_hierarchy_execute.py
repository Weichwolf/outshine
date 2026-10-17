"""Execute exactly pre-counted branches through the existing bounded path requests."""

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
from spruce_fine_hierarchy import branching, preflight
from spruce_growth_execute import provenance
from spruce_growth_probe import MAX_AXES, MAX_WINDOWS, cached_areas
from spruce_growth_product import pack_axes, publish, windows
from spruce_study_source import INPUT, read_input


def execute(recipe, physical, metadata, expected):
    if (
        not 0 < expected["axes"] == sum(expected["orders"]) <= MAX_AXES
        or not 0 < expected["windows"] <= MAX_WINDOWS
    ):
        raise ValueError("complete admitted counts required before physical poses")
    graph = ParentProfileGraph.from_recipe(recipe)
    if graph.identity != expected["graph_identity"]:
        raise ValueError("pre-counted graph identity required")
    patterns = [
        NeedlePattern.from_recipe(p) if p else None for p in physical["patterns"]
    ]
    poses, counts, reports = [], [], []
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("whole organs")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("needle poses")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("needle meshes")),
    ):
        for order in range(4):
            current = []
            required = expected["orders"][order]
            for path in graph.paths(order + 1):
                if len(current) >= required:
                    raise ValueError(
                        "actual branch population exceeds pre-count before its next pose"
                    )
                current.append(graph.instance(path))
            local = [
                patterns[order].count(p.length) if patterns[order] else 0
                for p in current
            ]
            if (
                len(current) != required
                or sum(local) != expected["needle_counts"][order]
            ):
                raise ValueError(
                    "actual physical hierarchy differs from exact pre-count"
                )
            poses.extend(current)
            counts.extend(local)
            reports.append(
                {
                    "order": order,
                    "axes": len(current),
                    "needles": sum(local),
                    "stored_graph_poses": len(graph._poses),
                    "mean_initial_world_z": float(
                        np.mean([p.frame[2, 1] for p in current])
                    ),
                }
            )
        if len({p.path for p in poses}) != len(poses):
            raise ValueError("distinct full graph paths required")
        axes = pack_axes(poses, counts)
        rows = windows(axes, patterns, metadata, graph.seed, expected["windows"])
    return graph.recipe(), axes, rows, reports


def build(graph_path, source, output):
    started = time.perf_counter()
    proof = provenance()
    root = Path(__file__).resolve().parents[3]
    own = [Path(__file__), Path(__file__).with_name("spruce_fine_hierarchy.py"), INPUT]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, own)], cwd=root
    ):
        raise ValueError("freeze own implementation and fixed source before execution")
    proof["implementation_sha256"].update(
        {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in own
        }
    )
    data, original = read_input(INPUT)
    recipe = json.loads(graph_path.read_text())
    if recipe != branching(original, 0.045):
        raise ValueError("declared selected branching hierarchy required")
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, area_inputs = cached_areas(source / "models")
    expected = preflight(recipe, data["physical_source"], metadata, areas)
    if (
        abs(expected["expected_area_m2"] / data["authoring"]["target_area_m2"] - 1)
        > 0.05
    ):
        raise ValueError("unchanged nominal physical-area admission required")
    recipe, axes, rows, reports = execute(
        recipe, data["physical_source"], metadata, expected
    )
    output.mkdir(parents=True, exist_ok=False)
    execution = output / "execution"
    execution.mkdir()
    np.savez_compressed(execution / "axes.npz", **axes)
    np.savez_compressed(output / "windows.npz", **rows)
    (execution / "graph.json").write_text(json.dumps(recipe, indent=2) + "\n")
    plant = {
        "version": "experimental-branching-needle-hierarchy-1",
        "wood": recipe,
        "physical_source": data["physical_source"],
        "authoring": data["authoring"]
        | {
            "primary_whorls": 48,
            "support_spacing_m": 0.15,
            "terminal_spacing_m": 0.045,
            "support_length_ratio": 0.5,
            "terminal_length_ratio": 0.7,
            "scope": "Branchlets, not claimed single annual shoots or botanically calibrated Picea.",
        },
    }
    report = (
        proof
        | expected
        | {
            "axis_reports": reports,
            "area_input_sha256": area_inputs,
            "graph_input_sha256": hashlib.sha256(graph_path.read_bytes()).hexdigest(),
            "decoded_axis_bytes": sum(a.nbytes for a in axes.values()),
            "decoded_window_bytes": sum(a.nbytes for a in rows.values()),
            "build_s": time.perf_counter() - started,
            "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            / 1024,
            "scope": "Complete exactly pre-counted hierarchy through existing bounded path requests. No full organs or individual needle work. Unapproved morphology/coverage/LOD/native quality.",
        }
    )
    (execution / "execution.json").write_text(json.dumps(report, indent=2) + "\n")
    manifest = publish(output, plant, execution, report, source / "catalog")
    print(
        json.dumps(
            {
                "identity": manifest["plan_identity"],
                **expected,
                "build_s": report["build_s"],
                "process_peak_rss_MiB": report["process_peak_rss_MiB"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("graph", "source", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    build(args.graph, args.source, args.output)
