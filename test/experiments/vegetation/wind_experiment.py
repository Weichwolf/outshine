"""Validate and measure cached wind deformation of current complete plant products."""

import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from spatial_examples import spatial_graph
from wind_checks import check_rejections, check_wind
from wind_field import WindField
from wind_plan import wind_plan
from wind_products import deform, export_sample, prepare


def benchmark(field, prepared):
    samples = {"cached": [], "direct": []}
    result = {}
    for _ in range(7):
        for method, timings in samples.items():
            result[method] = []
            began = time.perf_counter()
            for _, mesh, binding in prepared:
                current = (
                    binding if method == "cached" else field.bind(mesh["vertices"])
                )
                result[method].append(deform(field, mesh, current, 0.5, 1))
            timings.append(time.perf_counter() - began)
    for cached, direct in zip(result["cached"], result["direct"]):
        for key in ("vertices", "normals"):
            assert np.array_equal(cached[key], direct[key])
    return {
        "seconds": samples,
        "scope": "all requested vertices and corner normals; no IO/render",
    }


def check_triangles(field, prepared):
    minimum_ratio = float("inf")
    maximum_edge_ratio = 0.0
    for _, mesh, binding in prepared:
        rest = mesh["vertices"][mesh["faces"]]
        rest_area = np.linalg.norm(
            np.cross(rest[:, 1] - rest[:, 0], rest[:, 2] - rest[:, 0]), axis=1
        )
        for seconds in np.linspace(0, 10, 41):
            moved = deform(field, mesh, binding, seconds, 1)
            triangles = moved["vertices"][mesh["faces"]]
            cross = np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            )
            ratio = np.linalg.norm(cross, axis=1) / rest_area
            minimum_ratio = min(minimum_ratio, float(ratio.min()))
            edge_ratio = np.linalg.norm(
                triangles[:, 1] - triangles[:, 0], axis=1
            ) / np.linalg.norm(rest[:, 1] - rest[:, 0], axis=1)
            maximum_edge_ratio = max(maximum_edge_ratio, float(edge_ratio.max()))
            mean_normal = moved["normals"].reshape(-1, 3, 3).mean(axis=1)
            assert np.all(np.sum(cross * mean_normal, axis=1) > 0)
            assert ratio.min() > 0
    return {
        "sampled_times": 41,
        "minimum_area_ratio": minimum_ratio,
        "maximum_edge_stretch": maximum_edge_ratio,
    }


def main():
    output = Path("build/experiments/vegetation/wind")
    output.mkdir(parents=True, exist_ok=True)
    graph = spatial_graph()
    field = wind_plan(graph)
    np.savez_compressed(output / "wind-plan.npz", **field.arrays())
    with np.load(output / "wind-plan.npz") as stored:
        loaded = WindField(**{key: stored[key] for key in stored.files})
    reports = {"rejected_invalid_requests": check_rejections(loaded)}
    shared = np.array([p.anchor for p in graph.instances(3, 100_000)])
    anchor_results = []
    for detail, folder, scene in (
        ("near", "bark-layers", "joined-spatial-graph"),
        ("coarse", "graph-wood", "joined-spatial-coarse"),
    ):
        directory = output.parent / folder
        recipe = json.loads((directory / f"{scene}.json").read_text())
        began = time.perf_counter()
        prepared = prepare(directory, recipe, loaded, output, detail)
        preparation_s = time.perf_counter() - began
        anchor_results.append(loaded.move(shared, loaded.bind(shared), 0.5)[0])
        reports[detail] = {
            "preparation_with_io_s": preparation_s,
            "vertex_count": sum(len(m["vertices"]) for _, m, _ in prepared),
            "binding_bytes": sum(sum(a.nbytes for a in b) for _, _, b in prepared),
            "checks": check_wind(loaded, prepared[0][1], shared),
            "triangles": check_triangles(loaded, prepared),
            "performance": benchmark(loaded, prepared),
        }
        for seconds in (0.5, 2.0) if detail == "near" else (0.5,):
            name = f"wind-{detail}-{seconds:g}"
            export_sample(output, name, recipe, prepared, loaded, seconds, 1)
        if detail == "near":
            export_sample(output, "wind-near-rest", recipe, prepared, loaded, 0, 0)
    assert np.array_equal(*anchor_results)
    reports["shared_lod_anchor_error_m"] = float(
        np.max(np.abs(anchor_results[0] - anchor_results[1]))
    )
    reports["peak_process_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    reports["commit"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], text=True
    ).strip()
    reports["implementation_sha256"] = {
        p.name: hashlib.sha256(p.read_bytes()).hexdigest()
        for p in Path(__file__).parent.glob("wind_*.py")
    }
    (output / "wind.json").write_text(json.dumps(reports, indent=2) + "\n")
    print(json.dumps(reports))


if __name__ == "__main__":
    main()
