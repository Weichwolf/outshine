"""Compare spatial foliage clouds to identical canonical branch groups."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from architecture_experiment import broadleaf
from cloud_product import build_tree
from group_cloud import greedy_cloud, group_geometry, make_cloud
from leaf_sheet import average_blocks, sample_image
from plant_plan import PlantPlan
from raster_batch import rasterize
from raster_experiment import checks as raster_checks
from shoot_plan import normal_frame


def view(source, charts, direction, pixels=8):
    frame = normal_frame(np.asarray(direction, dtype=float), np.array((0, 0, 1)))
    camera = np.column_stack((frame[:, 0], frame[:, 2], frame[:, 1]))
    original = source["vertices"] @ camera
    points = np.vstack(
        [original, *[chart.mesh["vertices"] @ camera for chart in charts]]
    )
    centre = (points[:, :2].min(axis=0) + points[:, :2].max(axis=0)) / 2
    extent = np.max(np.ptp(points[:, :2], axis=0)) * 1.1
    lower, upper = centre - extent / 2, centre + extent / 2
    mask, normals = rasterize(
        original,
        source["faces"],
        source["normals"].reshape(-1, 3, 3),
        lower,
        upper,
        pixels * 8,
    )
    normals /= np.maximum(np.linalg.norm(normals, axis=-1, keepdims=True), 1e-12)
    sampled = []
    for chart in charts:
        positions = chart.mesh["vertices"] @ camera
        attributes = np.concatenate((chart.mesh["uv"], positions[:, 2, None]), axis=1)
        covered, data = rasterize(
            positions,
            chart.mesh["faces"],
            attributes[chart.mesh["faces"]],
            lower,
            upper,
            pixels * 8,
        )
        encoded = sample_image(np.rint(chart.normal * 255) / 255, data[..., :2])
        alpha = encoded[..., 3] * covered
        n = (2 * encoded[..., :3] - encoded[..., 3, None]) / np.maximum(
            encoded[..., 3, None], 1e-12
        )
        n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-12)
        sampled.append((alpha, n @ chart.frame.T, data[..., 2]))
    alpha = np.stack([item[0] for item in sampled])
    depths = np.stack([item[2] for item in sampled])
    order = np.argsort(-depths, axis=0)
    alpha = np.take_along_axis(alpha, order, axis=0)
    transmittance = np.cumprod(
        np.concatenate((np.ones_like(alpha[:1]), 1 - alpha[:-1])), axis=0
    )
    weights = alpha * transmittance
    coverage = weights.sum(axis=0)
    reference, reduced = (
        average_blocks(mask.astype(float), 8),
        average_blocks(coverage, 8),
    )
    union = np.maximum(reference, reduced).sum()
    response = []
    normal_array = np.stack([item[1] for item in sampled])
    normal_array = np.take_along_axis(normal_array, order[..., None], axis=0)
    for light_direction in ((0, 0, 1), (1, 1, 1), (-1, 0.5, 0.25)):
        light = np.asarray(light_direction, dtype=float)
        light /= np.linalg.norm(light)
        original_light = ((0.2 + 0.8 * np.abs(normals @ light)) * mask).sum()
        simplified_light = ((0.2 + 0.8 * np.abs(normal_array @ light)) * weights).sum()
        response.append(
            float(abs(original_light - simplified_light) / max(original_light, 1e-12))
        )
    return {
        "coverage_l1_over_union": float(
            np.abs(reference - reduced).sum() / max(union, 1e-12)
        ),
        "area_relative_error": float(
            abs(reference.sum() - reduced.sum()) / max(reference.sum(), 1e-12)
        ),
        "light_response_relative_errors": response,
    }


def study(output):
    cases = []
    plant = broadleaf()
    assert group_geometry(plant, (4, 3, 3), max_leaves=0) is None
    assert not plant._axes and not plant._leaves
    restored = PlantPlan.from_recipe(json.loads(json.dumps(plant.recipe())))
    assert restored.identity == plant.identity
    assert not restored._axes and not restored._leaves
    left, right = group_geometry(plant, (4, 3, 3)), group_geometry(restored, (4, 3, 3))
    for key in ("vertices", "faces", "normals", "ids", "anchor_origin", "anchor_frame"):
        np.testing.assert_array_equal(left[key], right[key])
    for bad in ({}, plant.recipe() | {"version": "unsupported"}):
        try:
            PlantPlan.from_recipe(bad)
        except ValueError:
            continue
        raise AssertionError("invalid recipe accepted")
    for path in ((4, 3, 3), (8, 6, 1), (0, 0, 0), (9, 8, 6)):
        source = group_geometry(plant, path)
        for method in ("axes1", "axes4", "greedy8", "greedy16"):
            start = time.perf_counter()
            charts = (
                make_cloud(source, layers=int(method[-1]), fit=False)
                if method.startswith("axes")
                else greedy_cloud(source, size=int(method[6:]))
            )
            elapsed = (time.perf_counter() - start) * 1000
            members = np.concatenate([chart.members for chart in charts])
            np.testing.assert_array_equal(
                np.sort(members), np.arange(len(source["ids"]))
            )
            for chart in charts:
                vertices = source["vertices"][
                    source["faces"][np.isin(source["organ_faces"], chart.members)]
                ].reshape(-1, 3)
                measured = np.abs(vertices @ chart.frame[:, 2] - chart.offset).max()
                assert measured <= chart.displacement_m
                np.testing.assert_array_equal(
                    chart.colour[..., 3], chart.normal[..., 3]
                )
                for image in (chart.colour, chart.normal):
                    filtered = image
                    while len(filtered) > 1:
                        filtered = average_blocks(filtered)
                        np.testing.assert_allclose(
                            filtered.mean(axis=(0, 1)),
                            image.mean(axis=(0, 1)),
                            atol=2e-12,
                        )
                assert np.all(np.isfinite(chart.normal)) and np.all(
                    np.isfinite(chart.colour)
                )
            views = []
            for elevation in (-1.0, -0.4, -0.05, 0.05, 0.4, 1.0):
                for azimuth in (0, 1.2, 2.4, 3.6):
                    direction = (np.cos(azimuth), np.sin(azimuth), elevation)
                    views.append(
                        view(source, charts, direction) | {"direction": direction}
                    )
            cases.append(
                {
                    "path": path,
                    "leaves": len(source["ids"]),
                    "method": method,
                    "charts": len(charts),
                    "generation_ms": elapsed,
                    "maximum_displacement_m": max(
                        chart.displacement_m for chart in charts
                    ),
                    "views": views,
                }
            )
    (output / "cloud-study.json").write_text(json.dumps(cases, indent=2) + "\n")
    for case in cases:
        errors = [view["coverage_l1_over_union"] for view in case["views"]]
        print(
            json.dumps(
                {
                    key: case[key]
                    for key in (
                        "path",
                        "method",
                        "charts",
                        "generation_ms",
                        "maximum_displacement_m",
                    )
                }
                | {"coverage_mean": float(np.mean(errors)), "coverage_max": max(errors)}
            ),
            flush=True,
        )
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/clouds")
    )
    parser.add_argument("--study-only", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = study(args.output)
    probe = group_geometry(broadleaf(), (4, 3, 3))
    raster = raster_checks(probe)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            [
                "git",
                "status",
                "--porcelain",
                "--",
                str(Path(__file__).resolve().parent),
            ],
            text=True,
        ).strip()
    )
    tree = None if args.study_only else build_tree(args.output, commit, modified)
    report = {
        "commit": commit,
        "source_modified": modified,
        "study": results,
        "raster": raster,
        "tree": tree,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "peak_scope": "whole Linux correctness, group study and optional tree build; no Blender or GPU",
    }
    (args.output / "clouds.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": commit,
                "source_modified": modified,
                "raster": raster,
                "tree": tree,
                "process_peak_rss_mib": report["process_peak_rss_mib"],
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
