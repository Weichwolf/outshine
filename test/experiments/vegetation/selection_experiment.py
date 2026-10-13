"""Fair directional coverage trials for whole-organ retention and coarser clouds."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from architecture_experiment import broadleaf
from cloud_experiment import view, view_bounds
from cloud_product import build_tree
from foliage_selection import directions, organ_order, retained_cloud, visible_area
from foliage_views import bake_views, select_view
from group_cloud import greedy_cloud, group_geometry, organ_statistics
from leaf_sheet import rasterize as reference_rasterize
from shoot_plan import normal_frame


def candidates(source):
    began = time.perf_counter()
    scores = visible_area(source)
    visibility_ms = (time.perf_counter() - began) * 1000
    for method in ("random", "visible", "spread"):
        for count in (15, 30):
            began = time.perf_counter()
            charts, stats = retained_cloud(source, count, method, scores)
            stats["visibility_ms"] = visibility_ms if method != "random" else 0.0
            yield (
                f"{method}{count}",
                charts,
                stats,
                (time.perf_counter() - began) * 1000,
            )
    for tolerance, angle in ((0.015, 0.26), (0.06, 0.65), (0.12, 1.0)):
        began = time.perf_counter()
        charts = greedy_cloud(source, tolerance=tolerance, angle_limit=angle, size=16)
        yield (
            f"cloud{round(tolerance * 1000)}",
            charts,
            {"tolerance_m": tolerance, "angle_limit_rad": angle},
            (time.perf_counter() - began) * 1000,
        )
    for count, size in ((32, 8), (32, 16), (64, 16), (64, 32)):
        began = time.perf_counter()
        charts = bake_views(source, count=count, size=size)
        yield (
            f"views{count}x{size}",
            charts,
            {"view_count": count, "view_size": size, "draw_charts": 1},
            (time.perf_counter() - began) * 1000,
        )


def visibility_reference(source):
    scores = np.zeros(len(source["ids"]))
    attributes = np.broadcast_to(
        source["organ_faces"][:, None, None] + 1, (len(source["faces"]), 3, 1)
    )
    for direction in directions(4):
        frame = normal_frame(direction, np.array((0.0, 0.0, 1.0)))
        camera = np.column_stack((frame[:, 0], frame[:, 2], frame[:, 1]))
        positions = source["vertices"] @ camera
        lower, upper = positions[:, :2].min(axis=0), positions[:, :2].max(axis=0)
        margin = np.max(upper - lower) / 16
        lower, upper = lower - margin, upper + margin
        mask, values = reference_rasterize(
            positions, source["faces"], attributes, lower, upper, 16
        )
        indices, counts = np.unique(
            np.rint(values[..., 0][mask]).astype(int) - 1, return_counts=True
        )
        scores[indices] += counts * np.prod(upper - lower) / 16**2 / 4
    return scores


def check_selection(source):
    centres, _ = organ_statistics(source)
    original_vertices = source["vertices"].copy()
    np.testing.assert_allclose(
        visible_area(source, pixels=16, views=4),
        visibility_reference(source),
        rtol=2e-15,
    )
    scores = visible_area(source)
    permutation = np.random.default_rng(8).permutation(len(scores))
    for method in ("random", "visible", "spread"):
        order = organ_order(source["ids"], centres, scores, method)
        shuffled = organ_order(
            source["ids"][permutation],
            centres[permutation],
            scores[permutation],
            method,
        )
        np.testing.assert_array_equal(
            source["ids"][order], source["ids"][permutation[shuffled]]
        )
        for count in (15, 30):
            charts, result = retained_cloud(source, count, method, scores)
            assert len(charts) == count and len(set(result["selected_ids"])) == count
            assert result["selected_ids"] == result["order_ids"][:count]
            np.testing.assert_allclose(
                result["retained_area_m2"], result["source_area_m2"], rtol=2e-14
            )
            assert np.isfinite(result["scale"]) and result["scale"] >= 1
            for chart in charts:
                assert np.all(np.isfinite(chart.mesh["vertices"]))
                np.testing.assert_array_equal(
                    chart.colour[..., 3], chart.normal[..., 3]
                )
    np.testing.assert_array_equal(source["vertices"], original_vertices)
    for kwargs in (
        {"count": 0},
        {"count": 257},
        {"count": True},
        {"count": 15, "scores": np.full(len(centres), np.nan)},
    ):
        try:
            retained_cloud(source, method="visible", **kwargs)
        except ValueError:
            continue
        raise AssertionError("invalid retention request accepted")
    return {
        "selection_procedures": 3,
        "nested_counts": [15, 30],
        "permuted_ids_identical": True,
        "surface_area_compensation": True,
        "visibility_matches_triangle_reference": True,
        "source_geometry_unchanged": True,
        "invalid_requests_rejected": 4,
    }


def check_views(source):
    charts = bake_views(source, count=32, size=16)
    for index, chart in enumerate(charts):
        np.testing.assert_array_equal(source["ids"][chart.members], source["ids"])
        np.testing.assert_allclose(chart.frame.T @ chart.frame, np.eye(3), atol=1e-14)
        assert select_view(charts, chart.frame[:, 2])[0] == index
        assert np.all(np.isfinite(chart.normal)) and np.all(np.isfinite(chart.colour))
        np.testing.assert_array_equal(chart.normal[..., 3], chart.colour[..., 3])
    for direction in ((0, 0, 0), (np.nan, 0, 1), (1, 2)):
        try:
            select_view(charts, direction)
        except ValueError:
            continue
        raise AssertionError("invalid view request accepted")
    return {
        "complete_views": 32,
        "all_organs_in_every_view": True,
        "exact_direction_selects_exact_chart": True,
        "invalid_directions_rejected": 3,
    }


def study():
    plant = broadleaf()
    result = []
    tests = check_selection(group_geometry(plant, (4, 3, 3)))
    tests["views"] = check_views(group_geometry(plant, (4, 3, 3)))
    directions = [
        (np.cos(azimuth), np.sin(azimuth), elevation)
        for elevation in (-1.0, -0.4, -0.05, 0.05, 0.4, 1.0)
        for azimuth in (0, 1.2, 2.4, 3.6)
    ]
    for path in ((4, 3, 3), (8, 6, 1), (0, 0, 0), (9, 8, 6)):
        source = group_geometry(plant, path)
        compared = list(candidates(source))
        points = np.vstack(
            [
                source["vertices"],
                *[
                    chart.mesh["vertices"]
                    for _, charts, _, _ in compared
                    for chart in charts
                ],
            ]
        )
        viewports = [
            (direction, view_bounds(points, direction)) for direction in directions
        ]
        references = {}
        for method, charts, stats, generation_ms in compared:
            views = []
            for index, (direction, bounds) in enumerate(viewports):
                drawn = (
                    (select_view(charts, direction)[1],)
                    if method.startswith("views")
                    else charts
                )
                measurement, reference = view(source, drawn, direction, bounds)
                if index in references:
                    np.testing.assert_array_equal(reference, references[index])
                else:
                    references[index] = reference
                views.append(
                    measurement
                    | {"direction": direction, "bounds_m": np.asarray(bounds).tolist()}
                )
            result.append(
                {
                    "path": path,
                    "leaves": len(source["ids"]),
                    "method": method,
                    "charts": len(charts),
                    "draw_charts": stats.get("draw_charts", len(charts)),
                    "rgba8_base_bytes": sum(
                        chart.colour.size + chart.normal.size for chart in charts
                    ),
                    "stats": stats,
                    "generation_ms": generation_ms,
                    "views": views,
                }
            )
            errors = [item["coverage_l1_over_union"] for item in views]
            print(
                json.dumps(
                    {
                        "path": path,
                        "method": method,
                        "charts": len(charts),
                        "mean": float(np.mean(errors)),
                        "max": max(errors),
                    }
                ),
                flush=True,
            )
    return result, tests


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/selection")
    )
    parser.add_argument("--build-tree", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    result, tests = study()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    modified = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
            text=True,
        ).strip()
    )
    report = {
        "commit": commit,
        "source_modified": modified,
        "study": result,
        "tree": None,
        "checks": tests,
        "visibility_training_directions": 32,
        "independent_view_directions": 24,
        "process_peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": "CPU prototype construction and 8×8 orthographic coverage/light proxies; no runtime GPU",
    }
    (args.output / "selection.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.build_tree:
        report["tree"] = build_tree(
            args.output, commit, modified, chart_size=16, view_count=64
        )
        report["process_peak_rss_mib"] = (
            resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024
        )
        (args.output / "selection.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
