"""Measure complete needle sources, stable graph requests and exported products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from generator_graph import GeneratorGraph
from graph_products import graph_products, pose_arrays
from needle_checks import needle_checks
from needle_recipes import LABELS, needle_graph


def geometry_costs(shape):
    times = {"batch": [], "scalar": []}
    outputs = {}
    for repeat in range(3):
        for method in ("batch", "scalar") if repeat % 2 == 0 else ("scalar", "batch"):
            start = time.perf_counter()
            outputs[method] = shape.mesh(method=method)
            times[method].append(time.perf_counter() - start)
    for key in outputs["batch"]:
        assert np.array_equal(outputs["batch"][key], outputs["scalar"][key]), key
    return times


def graph_checks(output, index):
    graph = needle_graph(index)
    source = GeneratorGraph.from_recipe(graph.recipe())
    assert source.identity == graph.identity
    with patch(
        "graph_products.source_parts", side_effect=AssertionError("mesh before demand")
    ):
        assert graph_products(graph, output, max_new_poses=0) is None
        assert graph_products(graph, output, max_prototype_triangles=0) is None
    assert not graph._poses
    full = graph.instances(3, 1000)
    partial = source.instance(full[len(full) // 2].path)
    assert partial.path == full[len(full) // 2].path
    assert np.array_equal(partial.origin, full[len(full) // 2].origin)
    assert np.array_equal(partial.frame, full[len(full) // 2].frame)
    for pose in full:
        assert np.array_equal(pose.origin, pose.anchor)
    return {
        "needle_count": len(full),
        "identical_plan_roundtrip": True,
        "partial_request_same_pose": True,
        "budget_refusals_before_meshing": True,
    }


def write_scene(output, name, products, identity, camera=None, target=None):
    scene = {
        "name": name,
        "products": products,
        "plan_identity": identity,
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "resolution": [2560, 1440],
        "samples": 32,
        "camera": camera or [0.38, -0.66, 0.36],
        "target": target or [0, 0, 0.085],
        "fov_y": 35,
        "ground_size_m": 2,
        "procedural_material": True,
    }
    (output / f"{name}.json").write_text(json.dumps(scene, indent=2) + "\n")


def main():
    output = Path("build/experiments/vegetation/needles")
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    scenes = {"near": [], "coarse": []}
    identities = []
    checks = needle_checks()
    for index, label in enumerate(LABELS):
        case = {
            "index": index,
            "label": label,
            "graph_checks": graph_checks(output / "budget", index),
            "geometry_seconds": geometry_costs(needle_graph(index).by_id[3].shape),
        }
        poses = {}
        identities.append(needle_graph(index).identity)
        for detail in ("near", "coarse"):
            graph = needle_graph(index)
            folder = output / f"case-{index}-{detail}"
            start = time.perf_counter()
            result = graph_products(graph, folder, detail)
            case[detail + "_product_seconds"] = time.perf_counter() - start
            assert result is not None
            poses[detail] = pose_arrays(graph.instances(3, 0))
            (folder / "plan.json").write_text(json.dumps(result, indent=2) + "\n")
            for product in result["products"]:
                with np.load(folder / product["placements"]) as data:
                    stored = {k: data[k] for k in data.files}
                stored["origins"] = stored["origins"] + [(index - 1) * 0.18, 0, 0]
                np.savez_compressed(folder / product["placements"], **stored)
                record = product.copy()
                for key in ("mesh", "placements", "bark_material", "needle_material"):
                    if key in record:
                        record[key] = str((folder / record[key]).relative_to(output))
                scenes[detail].append(record)
            case[detail + "_prototype_triangles"] = result["prototype_triangles"]
            case[detail + "_instanced_triangles"] = sum(
                p["triangles_per_instance"] * p["instances"] for p in result["products"]
            )
        for key in poses["near"]:
            assert np.array_equal(poses["near"][key], poses["coarse"][key]), key
        case["same_needle_ids_origins_frames"] = True
        cases.append(case)
        print(case, flush=True)
    identity = hashlib.sha256("".join(identities).encode()).hexdigest()
    for detail, products in scenes.items():
        write_scene(output, f"needle-shoots-{detail}", products, identity)
    write_scene(
        output,
        "needle-bundle-detail",
        [p for p in scenes["near"] if p["mesh"].startswith("case-0-")],
        identity,
        camera=[-0.085, -0.16, 0.18],
        target=[-0.18, 0, 0.078],
    )
    report = {
        "cases": cases,
        "checks": checks,
        "peak_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                *Path(__file__).parent.glob("needle*.py"),
                *[
                    Path(__file__).with_name(n)
                    for n in (
                        "generator_specs.py",
                        "generator_graph.py",
                        "graph_frames.py",
                        "graph_sources.py",
                        "graph_products.py",
                        "curve_frame.py",
                    )
                ],
            ]
        },
    }
    (output / "needles.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
