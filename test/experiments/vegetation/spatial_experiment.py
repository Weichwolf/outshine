"""Compare continuous spatial preparation and equivalent graph placement methods."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from bend_checks import check_bends
from spatial_checks import check_all
from spatial_examples import spatial_axis, spatial_graph
from spatial_scenes import make_scenes
from spine_checks import ode_reference


def compare_fields():
    result = []
    u = np.linspace(0, 1, 513)
    for intervals in (4, 8, 16, 32):
        samples = []
        for _ in range(5):
            axis = spatial_axis(intervals=intervals)
            began = time.perf_counter()
            field = axis.field.sample(u)
            samples.append(time.perf_counter() - began)
        reference, _ = ode_reference(axis.spine, field["parameter"])
        error = float(np.linalg.norm(reference - field["lateral"], axis=-1).max())
        began = time.perf_counter()
        for _ in range(20):
            axis.field.sample(u)
        lookup = (time.perf_counter() - began) / 20
        result.append(
            {
                "intervals_per_segment": intervals,
                "samples_s": samples,
                "frame_error": error,
                "lookup_513_s": lookup,
                "prepared_coefficient_bytes": axis.measure.polynomial.c.nbytes
                + axis.measure.inverse.c.nbytes
                + axis.field.lateral_curve.c.nbytes
                + axis.measure.parameter_nodes.nbytes
                + axis.measure.length_nodes.nbytes,
            }
        )
    return result


def compare_graph():
    result = {str(mode): [] for mode in (False, True)}
    for _ in range(5):
        poses = {}
        for mode in (False, True):
            began = time.perf_counter()
            graph = replace(spatial_graph(), spatial_batches=mode)
            poses[mode] = graph.instances(3, 1000)
            result[str(mode)].append(time.perf_counter() - began)
        for a, b in zip(poses[False], poses[True]):
            assert a.path == b.path
            np.testing.assert_array_equal(a.origin, b.origin)
            np.testing.assert_array_equal(a.frame, b.frame)
    direct = spatial_graph()
    full = spatial_graph()
    poses = full.instances(3, 1000)
    for expected in poses:
        actual = direct.instance(expected.path)
        np.testing.assert_array_equal(actual.origin, expected.origin)
        np.testing.assert_array_equal(actual.frame, expected.frame)
    return {
        "samples_s": result,
        "equal_leaf_poses": len(poses),
        "direct_anchors_identical": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/spatial-graph"),
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    checks = check_all() | {"bends": check_bends()}
    fields = compare_fields()
    graph_cost = compare_graph()
    began = time.perf_counter()
    scene = make_scenes(args.output)
    products_s = time.perf_counter() - began
    names = (
        "spatial_experiment.py",
        "spatial_examples.py",
        "spatial_checks.py",
        "spatial_scenes.py",
        "spatial_axis.py",
        "spine_measure.py",
        "spine_field.py",
        "spine_curve.py",
        "spine_frames.py",
        "spine_surface.py",
        "tube_topology.py",
        "bend_pattern.py",
        "bend_checks.py",
        "graph_frames.py",
        "generator_specs.py",
        "generator_graph.py",
        "graph_products.py",
        "graph_sources.py",
    )
    report = {
        "checks": checks,
        "fields": fields,
        "graph_cost": graph_cost,
        "scene": scene,
        "products_s": products_s,
        "process_peak_with_checks_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "implementation_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in names
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "native_spine_or_bifurcation_equivalence_proven": False,
    }
    (args.output / "spatial-functions.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps({"scene": scene, "products_s": products_s}))


if __name__ == "__main__":
    main()
