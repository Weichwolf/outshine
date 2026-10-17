"""Build the admitted uniform hierarchy with existing bounded path/window kernels."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from fine_hierarchy_execute import execute
from spruce_fine_hierarchy import preflight
from spruce_growth_execute import provenance
from spruce_growth_probe import cached_areas
from spruce_growth_product import publish
from spruce_study_source import INPUT, read_input
from spruce_uniform import inventory, uniform


def build(graph_path, source, output):
    started = time.perf_counter()
    proof = provenance()
    root = Path(__file__).resolve().parents[3]
    own = [
        Path(__file__),
        Path(__file__).with_name("spruce_uniform.py"),
        Path(__file__).with_name("spruce_fine_hierarchy.py"),
        root / "lib/src/outshine_ref/vegetation/placement_patterns.py",
        INPUT,
    ]
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
    if recipe != uniform(original):
        raise ValueError("declared19-group uniform hierarchy required")
    metadata = json.loads((source / "catalog/catalog.json").read_text())
    areas, hashes = cached_areas(source / "models")
    expected = preflight(recipe, data["physical_source"], metadata, areas)
    if (
        abs(expected["expected_area_m2"] / data["authoring"]["target_area_m2"] - 1)
        > 0.05
    ):
        raise ValueError("unchanged nominal physical-area admission required")
    spatial = inventory(recipe, data["physical_source"], areas)
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
        "version": "experimental-uniform-branchlets-1",
        "wood": recipe,
        "physical_source": data["physical_source"],
        "authoring": data["authoring"]
        | {
            "primary_whorls": 48,
            "support_spacing_m": 0.15,
            "terminal_minimum_spacing_m": 0.015,
            "terminal_maximum_groups": 19,
            "support_length_ratio": 0.5,
            "terminal_length_ratio": 0.7,
            "end_branch_posture": "radial",
            "scope": "Own complete-interval allocation; unapproved Picea morphology, not a SpeedTree formula.",
        },
    }
    report = (
        proof
        | expected
        | {
            "spatial_inventory": spatial,
            "axis_reports": reports,
            "area_input_sha256": hashes,
            "graph_input_sha256": hashlib.sha256(graph_path.read_bytes()).hexdigest(),
            "decoded_axis_bytes": sum(a.nbytes for a in axes.values()),
            "decoded_window_bytes": sum(a.nbytes for a in rows.values()),
            "build_s": time.perf_counter() - started,
            "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            / 1024,
            "scope": "Exactly pre-counted existing path/window execution; no full organs or needle work. Unapproved image/LOD/native quality.",
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
