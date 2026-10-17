"""Execute an admitted two-population graph without placing individual needles."""

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
from spruce_growth_execute import provenance
from spruce_growth_probe import MAX_AXES, cached_areas
from spruce_growth_product import pack_axes, publish, windows
from spruce_interwhorl import intercalated, preflight
from spruce_study_source import INPUT, read_input


def execute(recipe, physical, metadata, expected):
    graph = ParentProfileGraph.from_recipe(recipe)
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
        for order, keys in enumerate(((1,), (2, 5), (3,), (4,))):
            current = []
            for key in keys:
                if key not in graph.by_id:
                    continue
                local = graph.instances(key, MAX_AXES)
                if local is None:
                    raise ValueError("admitted graph exceeds original pose bound")
                current.extend(local)
            local_counts = [
                patterns[order].count(p.length) if patterns[order] else 0
                for p in current
            ]
            if (
                len(current) != expected["orders"][order]
                or sum(local_counts) != expected["needle_counts"][order]
            ):
                raise ValueError("actual full graph differs from count-only preflight")
            poses.extend(current)
            counts.extend(local_counts)
            if len(poses) > MAX_AXES:
                raise ValueError("unchanged complete axis budget required")
            reports.append(
                {"order": order, "axes": len(current), "needles": sum(local_counts)}
            )
        if len({p.path for p in poses}) != len(poses):
            raise ValueError("both primary populations need distinct full graph paths")
        axes = pack_axes(poses, counts)
        rows = windows(axes, patterns, metadata, graph.seed, expected["windows"])
    return graph.recipe(), axes, rows, reports


def build(graph_path, source, output):
    started = time.perf_counter()
    proof = provenance()
    root = Path(__file__).resolve().parents[3]
    own = [Path(__file__), Path(__file__).with_name("spruce_interwhorl.py"), INPUT]
    status = subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, own)], cwd=root, text=True
    )
    if status.strip():
        raise ValueError("commit the experiment before building its actual product")
    proof["implementation_sha256"].update(
        {
            str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in own
        }
    )
    data, original = read_input(INPUT)
    graph_recipe = json.loads(graph_path.read_text())
    if graph_recipe != intercalated(original, 1.45):
        raise ValueError(
            "the declared selected two-population study recipe is required"
        )
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, area_inputs = cached_areas(source / "models")
    expected = preflight(graph_recipe, data["physical_source"], metadata, areas)
    target = data["authoring"]["target_area_m2"]
    if abs(expected["expected_area_m2"] / target - 1) > 0.05:
        raise ValueError("unchanged five-percent physical area admission required")
    graph_recipe, axes, rows, orders = execute(
        graph_recipe, data["physical_source"], metadata, expected
    )
    output.mkdir(parents=True, exist_ok=False)
    execution = output / "execution"
    execution.mkdir()
    np.savez_compressed(execution / "axes.npz", **axes)
    np.savez_compressed(output / "windows.npz", **rows)
    (execution / "graph.json").write_text(json.dumps(graph_recipe, indent=2) + "\n")
    recipe = {
        "version": "experimental-intercalated-needled-1",
        "wood": graph_recipe,
        "physical_source": data["physical_source"],
        "authoring": data["authoring"]
        | {
            "primary_whorls": 48,
            "whorl_members": 4,
            "small_primaries": 47,
            "small_length_factor": 0.7,
            "secondary_spacing_factor": 1.45,
            "scope": "Independent smaller primary population, not observed botanical calibration.",
        },
    }
    report = (
        proof
        | expected
        | {
            "axis_reports": orders,
            "area_input_sha256": area_inputs,
            "graph_input_sha256": hashlib.sha256(graph_path.read_bytes()).hexdigest(),
            "decoded_axis_bytes": sum(a.nbytes for a in axes.values()),
            "decoded_window_bytes": sum(a.nbytes for a in rows.values()),
            "build_s": time.perf_counter() - started,
            "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            / 1024,
            "scope": "Complete admitted axes and physical windows, no organ/needle expansion. Unapproved experimental crown architecture.",
        }
    )
    (execution / "execution.json").write_text(json.dumps(report, indent=2) + "\n")
    manifest = publish(output, recipe, execution, report, source / "catalog")
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
    parser.add_argument("--graph", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    build(args.graph, args.source, args.output)
