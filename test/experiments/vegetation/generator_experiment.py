"""Verify graph products and measure equivalent subset requests before rendering."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import time
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from generator_checks import check_invalid, check_pitch_continuity, check_requests
from generator_examples import mixed_graph
from generator_graph import GeneratorGraph
from generator_scenes import make_scenes
from graph_products import graph_products
from graph_sources import source_parts, source_triangles


def check_products(output):
    graph = mixed_graph()
    with patch(
        "graph_products.source_parts",
        side_effect=AssertionError("unexpected mesh work"),
    ):
        assert graph_products(graph, output, max_new_poses=62) is None
        assert graph_products(graph, output, max_prototype_triangles=0) is None
    assert not graph._poses
    checked = 0
    for detail in ("near", "coarse"):
        for node in graph.nodes:
            if node.kind == "root":
                continue
            parts = source_parts(node.shape, detail)
            assert sum(len(m["faces"]) for m in parts.values()) == source_triangles(
                node.shape, detail
            )
            for part, mesh in parts.items():
                assert (
                    np.isfinite(mesh["vertices"]).all()
                    and np.isfinite(mesh["normals"]).all()
                )
                np.testing.assert_allclose(
                    np.linalg.norm(mesh["normals"], axis=1), 1, atol=1e-12
                )
                for pose in graph.instances(node.id, 1000):
                    points = (
                        mesh["vertices"] @ (pose.frame * pose.scale).T + pose.origin
                    )
                    assert (
                        np.linalg.norm(points, axis=1).max()
                        <= graph.bounds[node.id] + 1e-12
                    )
                if part == "wood":
                    assert (mesh["bark_radius"] > 0).all()
                checked += 1
    return {"source_parts_checked": checked, "budget_refusals_no_poses_or_mesh": True}


def check_shared_sources(output):
    base = mixed_graph()
    blade = base.by_id[3]
    extra = replace(
        blade, id=5, parents=(1,), placement=replace(blade.placement, count=2)
    )
    graph = GeneratorGraph(base.seed, base.nodes + (extra,))
    result = graph_products(graph, output / "shared-check", nodes=(3, 5))
    assert result["prototypes_built"] == 1 and len(result["products"]) == 1
    product = result["products"][0]
    assert product["generator_ids"] == [3, 5] and product["instances"] == 50
    expected = graph.instances(3, 0) + graph.instances(5, 0)
    with np.load(output / "shared-check" / product["placements"]) as stored:
        paths = tuple(
            tuple(map(tuple, stored["path_pairs"][start:end]))
            for start, end in zip(
                stored["path_offsets"][:-1], stored["path_offsets"][1:]
            )
        )
        assert paths == tuple(p.path for p in expected)
        assert len(set(paths)) == 50
        np.testing.assert_array_equal(stored["origins"], [p.origin for p in expected])
        np.testing.assert_array_equal(
            stored["frames"], [p.frame * p.scale for p in expected]
        )
    return {"shared_prototype_instances": 50, "lossless_hierarchy_paths": True}


def check_detail_placements(output):
    graph = mixed_graph()
    near = graph_products(graph, output / "detail-check", "near")
    coarse = graph_products(graph, output / "detail-check", "coarse")
    assert near["plan_identity"] == coarse["plan_identity"]
    assert coarse["prototype_triangles"] < near["prototype_triangles"]
    for fine in near["products"]:
        low = next(
            p
            for p in coarse["products"]
            if p["generator_ids"] == fine["generator_ids"]
            and p["surface"] == fine["surface"]
        )
        with (
            np.load(output / "detail-check" / fine["placements"]) as first,
            np.load(output / "detail-check" / low["placements"]) as second,
        ):
            assert first.files == second.files
            for key in first.files:
                np.testing.assert_array_equal(first[key], second[key])
    return {"near_coarse_placements_identical": True}


def request_costs():
    samples = {method: [] for method in ("all_then_select", "requested", "cached")}
    for _ in range(7):
        results = {}
        for method in ("all_then_select", "requested"):
            graph = mixed_graph()
            began = time.perf_counter()
            if method == "all_then_select":
                for key in graph.order:
                    graph.instances(key, 1000)
            results[method] = graph.instances(4, 15 if method == "requested" else 0)
            samples[method].append(
                {
                    "elapsed_s": time.perf_counter() - began,
                    "poses_built": len(graph._poses),
                }
            )
            if method == "requested":
                began = time.perf_counter()
                cached = graph.instances(4, 0)
                samples["cached"].append(
                    {"elapsed_s": time.perf_counter() - began, "poses_built": 0}
                )
                assert all(a is b for a, b in zip(cached, results[method]))
        for left, right in zip(results["all_then_select"], results["requested"]):
            assert left.path == right.path and left.scale == right.scale
            np.testing.assert_array_equal(left.frame, right.frame)
            np.testing.assert_array_equal(left.origin, right.origin)
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/vegetation/generator-graph"),
    )
    args = parser.parse_args()
    checks = (
        check_requests()
        | check_invalid()
        | {"pitch_frame_delta": check_pitch_continuity()}
        | check_products(args.output)
        | check_shared_sources(args.output)
        | check_detail_placements(args.output)
    )
    samples = request_costs()
    began = time.perf_counter()
    summary = make_scenes(args.output)
    products_s = time.perf_counter() - began
    product_files = {
        product[key]
        for name in ("generator-plant", "generator-coarse")
        for product in json.loads((args.output / f"{name}.json").read_text())[
            "products"
        ]
        for key in ("mesh", "placements")
    }
    files = [
        Path(__file__).with_name(name)
        for name in (
            "generator_experiment.py",
            "generator_graph.py",
            "generator_specs.py",
            "placement_patterns.py",
            "generator_examples.py",
            "generator_checks.py",
            "generator_scenes.py",
            "graph_products.py",
            "graph_sources.py",
        )
    ]
    result = {
        "checks": checks,
        "summary": summary,
        "samples": samples,
        "products_s": products_s,
        "process_peak_rss_with_checks_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "stored_mesh_and_pose_bytes": sum(
            (args.output / name).stat().st_size for name in product_files
        ),
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "native_or_modeler_equivalence_proven": False,
    }
    (args.output / "generator-graph.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps({"checks": checks, "products_s": products_s}))


if __name__ == "__main__":
    main()
