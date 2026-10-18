"""Prepare controlled private scenes; render separately inside an agreed heavy slot."""

import argparse
import hashlib
import json
import subprocess
from dataclasses import replace
from pathlib import Path

import numpy as np
from generator_graph import GeneratorGraph
from graph_products import graph_products
from outshine_ref.vegetation import DistancePattern, Profile
from parent_growth_graph import ParentGrowthGraph
from parent_growth_models import control_nodes

MODES = ("absolute-fixed", "relative-fixed", "relative-distance")
HERE = Path(__file__).parent


def comparison_graph(mode):
    if mode not in MODES:
        raise ValueError("implemented controlled parent-growth comparison required")
    nodes = list(control_nodes())
    nodes[2] = replace(
        nodes[2],
        shape=replace(nodes[2].shape, bend=0.25),
        placement=replace(
            nodes[2].placement,
            count=5,
            pattern=None,
            pitch=1.1,
            pitch_variance=0.08,
            azimuth_variance=0.18,
        ),
    )
    if mode == "relative-distance":
        nodes[2] = replace(
            nodes[2],
            placement=replace(
                nodes[2].placement,
                count=1,
                pattern=DistancePattern(spacing=Profile(np.full(4, 0.2))),
            ),
        )
    nodes[3] = replace(
        nodes[3],
        placement=replace(nodes[3].placement, pitch=1.1, pitch_variance=0.08),
    )
    if mode == "absolute-fixed":
        return GeneratorGraph(17, tuple(nodes))
    return ParentGrowthGraph(17, tuple(nodes), parent_length_ratios=((2, 0.5),))


def prepare(output):
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    names = (
        "parent_growth_graph.py",
        "parent_growth_models.py",
        "parent_growth_experiment.py",
        "generator_graph.py",
        "generator_specs.py",
        "graph_products.py",
    )
    changed = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", *[str(HERE / n) for n in names]]
        )
    )
    reports = []
    for mode in MODES:
        graph = comparison_graph(mode)
        directory = output / mode
        result = graph_products(graph, directory, max_new_poses=1000)
        assert result is not None
        for filename in {p["placements"] for p in result["products"]}:
            path = directory / filename
            with np.load(path, allow_pickle=False) as stored:
                values = {k: stored[k] for k in stored.files}
            slots = values["path_pairs"][values["path_offsets"][:-1] + 1, 1].astype(
                np.int64
            )
            values["origins"][:, 0] += (slots - 1) * 0.9
            np.savez_compressed(path, **values)
        scene = result | {
            "name": mode,
            "camera": [2.9, -5.2, 2.4],
            "target": [0, 0, 0.85],
            "fov_y": 36,
            "ground_size_m": 8,
            "resolution": [2560, 1440],
            "samples": 32,
            "source_commit": commit,
            "source_modified": changed,
            "presentation_root_offsets_m": [-0.9, 0, 0.9],
            "scope": "three 0.5/1/1.5 m parents; physical 0.02 m needles; functional control, unequal total leaf area, not a matched-quality cost comparison or mature species approval",
            "implementation_sha256": {
                name: hashlib.sha256((HERE / name).read_bytes()).hexdigest()
                for name in names
            },
        }
        (directory / "scene.json").write_text(json.dumps(scene, indent=2) + "\n")
        report = {
            "mode": mode,
            "plan_identity": graph.identity,
            "needle_instances": len(graph.instances(3, 0)),
            "axis_instances": len(graph.instances(2, 0)),
            "cached_poses": len(graph._poses),
            "prototype_triangles": result["prototype_triangles"],
        }
        reports.append(report)
        print(json.dumps(report), flush=True)
    (output / "controls.json").write_text(json.dumps(reports, indent=2) + "\n")
    return reports


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.output)
