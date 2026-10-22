"""Flat ENU ground/horizon transport; curvature/device admission stays separate."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_parents import (
    background_image,
    camera,
    ground,
    lighting,
    opaque_image,
)
from PIL import Image
from triangle_image_reference import average_samples, image_rays, projected_bounds
from urban_parent_conditional import grid, ground_stops, render_conditional
from urban_parent_layer import render_layer
from urban_parent_light import display, surface_radiance_upper
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load
from urban_pixel_filter import filter_correction, support_points, support_polygon
from urban_shadow_runs import select_shadow_query


def probe(parent, profile, tolerance):
    product = grid(parent)
    pitch = product["header"]["pitch_m"]
    x = product["reference_origin_m"][0] + (product["shape"][0] // 2 + 0.5) * pitch
    y = product["reference_origin_m"][1] - pitch
    o = np.array([[x, y, 2.0], [x + pitch * 0.48, y, 2.0]])
    d = np.tile([0.0, 1.0, 0.0], (len(o), 1))
    results, arrays = {}, {}
    for name, threshold in [("complete", 0.0), ("bounded", tolerance)]:
        started = time.perf_counter()
        c, t, _, work = render_layer(
            parent,
            o,
            d,
            profile,
            np.full(len(o), np.inf),
            transmission_tolerance=threshold,
        )
        results[name] = {"seconds": time.perf_counter() - started, "work": work}
        arrays[name] = c
        arrays[name + "_transmission"] = t
    maximum = float(np.abs(arrays["complete"] - arrays["bounded"]).max())
    bound = tolerance * surface_radiance_upper(profile, parent.heights[-1])
    np.testing.assert_array_less(maximum, bound + 1e-14)
    results.update(
        max_abs_colour_difference=maximum,
        derived_linear_bound=bound,
        tolerance=tolerance,
        origins=o.tolist(),
        directions=d.tolist(),
    )
    return results, arrays


def region(parent, resolved, view, profile):
    products = [grid(parent), resolved]
    points = [support_points(p, parent.heights[-1], profile) for p in products]
    box = projected_bounds(np.vstack(points), view)
    return np.array(
        [
            np.maximum(np.floor(box[0]), [0, 0]),
            np.minimum(np.ceil(box[1]), [1280, 720]),
        ],
        int,
    )


def parent_sample(parent, origins, directions, profile, tolerance, query=None):
    started = time.perf_counter()
    colour, coverage, work = render_conditional(
        parent,
        origins,
        directions,
        profile,
        transmission_tolerance=tolerance,
        shadow_query=query,
    )
    sky = ~np.isfinite(ground_stops(origins, directions))
    colour[sky] += (1 - coverage[sky, None]) * (
        0.002 if profile["kind"] == "point" else 0.1
    )
    return colour, coverage, work, time.perf_counter() - started


def compare(parent, resolved, args):
    if args.footprint_order:
        return compare_pixel(parent, resolved, args)
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, altitude=args.altitude)
    profile = lighting(extent, args.lighting)
    box = region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    if min(width, height) <= 0:
        raise ValueError("nonempty projected diagnostic required")
    o, d = image_rays(view, box, width, height, args.samples, jitter_seed=args.seed)
    d /= np.linalg.norm(d, axis=1)[:, None]
    started = time.perf_counter()
    reference, alpha, reference_work = opaque_image(resolved, o, d, profile)
    reference_seconds = time.perf_counter() - started
    started = time.perf_counter()
    query = (
        select_shadow_query(parent, profile, force=args.force_shadow_runs)
        if args.shadow_runs
        else None
    )
    build_seconds = time.perf_counter() - started
    colour, coverage, work, seconds = parent_sample(
        parent, o, d, profile, args.tolerance, query
    )
    target = average_samples(reference, width, height, args.samples)
    crop = average_samples(colour, width, height, args.samples)
    backdrop = background_image(view, profile)
    for name, value in [("resolved", target), ("parent", crop)]:
        image = backdrop.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = value
        Image.fromarray(display(image)).save(args.output / f"{name}.png")
    arrays = {
        "parent": crop,
        "resolved": target,
        "coverage": average_samples(coverage, width, height, args.samples),
        "resolved_coverage": average_samples(alpha, width, height, args.samples),
    }
    paired = {}
    if args.paired_exact:
        exact, exact_coverage, exact_work, exact_seconds = parent_sample(
            parent, o, d, profile, args.tolerance
        )
        np.testing.assert_array_equal(exact_coverage, coverage)
        difference = float(np.abs(exact - colour).max())
        if difference > 2e-9:
            raise AssertionError(f"run shadow RGB differs from DDA by {difference}")
        arrays["exact"] = average_samples(exact, width, height, args.samples)
        image = backdrop.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = arrays["exact"]
        Image.fromarray(display(image)).save(args.output / "exact.png")
        paired = {
            "seconds": exact_seconds,
            "work": exact_work,
            "max_sample_rgb_difference": difference,
            "crop_rgb_rmse": float(np.sqrt(np.mean((arrays["exact"] - crop) ** 2))),
        }
    bound = args.tolerance * max(
        surface_radiance_upper(profile, parent.heights[-1]),
        0.002 if args.lighting == "night" else 0.1,
    )
    report = {
        "profile": [1280, 720],
        "target_distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "lighting": profile,
        "roi": box.tolist(),
        "samples_per_axis": args.samples,
        "jitter_seed": args.seed,
        "rays": len(o),
        "upward_rays": int((d[:, 2] > 0).sum()),
        "downward_rays": int((d[:, 2] < 0).sum()),
        "parent_seconds": seconds,
        "resolved_seconds": reference_seconds,
        "parent_work": work,
        "shadow_method": "affine-runs" if query is not None else "dda",
        "shadow_build_seconds": build_seconds,
        "paired_exact": paired,
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "resolved_work": reference_work,
        "tolerance": args.tolerance,
        "derived_linear_termination_bound": bound,
        "linear_rgb_rmse": float(np.sqrt(np.mean((crop - target) ** 2))),
        "coverage_l1_pixels": float(
            np.abs(arrays["coverage"] - arrays["resolved_coverage"]).sum()
        ),
    }
    return report, arrays


def compare_pixel(parent, resolved, args):
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, altitude=args.altitude)
    profile = lighting(extent, args.lighting)
    box = region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    polygon = support_polygon(
        support_points(grid(parent), parent.heights[-1], profile), view
    )
    query = select_shadow_query(parent, profile) if args.shadow_runs else None
    baseline_image = background_image(view, profile)
    baseline = baseline_image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]]

    def background(o, d):
        return ground(o, d, profile)[0]

    def scene(o, d):
        rgb, alpha, work, _ = parent_sample(
            parent, o, d, profile, args.tolerance, query
        )
        return rgb, alpha, work

    started = time.perf_counter()
    o, d = image_rays(view, box, width, height, args.samples, jitter_seed=args.seed)
    d /= np.linalg.norm(d, axis=1)[:, None]
    owners = np.repeat(
        np.repeat(
            np.arange(width * height).reshape(height, width), args.samples, axis=0
        ),
        args.samples,
        axis=1,
    ).ravel()
    delta, coverage, filtered_work = filter_correction(
        polygon,
        box,
        view,
        scene,
        background,
        order=args.footprint_order,
        interior=(o, d, owners),
    )
    seconds = time.perf_counter() - started
    candidate = baseline + delta
    old_rgb, old_coverage, old_work, old_seconds = parent_sample(
        parent, o, d, profile, args.tolerance, query
    )
    uniform = baseline + average_samples(
        old_rgb - background(o, d), width, height, args.samples
    )
    old_coverage = average_samples(old_coverage, width, height, args.samples)[..., 0]
    o, d = image_rays(
        view, box, width, height, args.reference_samples, jitter_seed=args.seed + 1
    )
    d /= np.linalg.norm(d, axis=1)[:, None]
    started = time.perf_counter()
    ref_rgb, ref_coverage, ref_work = opaque_image(resolved, o, d, profile)
    target = baseline + average_samples(
        ref_rgb - background(o, d), width, height, args.reference_samples
    )
    ref_coverage = average_samples(ref_coverage, width, height, args.reference_samples)[
        ..., 0
    ]
    ref_seconds = time.perf_counter() - started
    arrays = {
        "parent": candidate,
        "uniform": uniform,
        "resolved": target,
        "coverage": coverage,
        "uniform_coverage": old_coverage,
        "resolved_coverage": ref_coverage,
        "support_polygon": polygon,
    }
    for name in ["parent", "uniform", "resolved"]:
        image = baseline_image.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = arrays[name]
        Image.fromarray(display(image)).save(args.output / f"{name}.png")
    report = {
        "profile": [1280, 720],
        "roi": box.tolist(),
        "target_distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "lighting": profile,
        "footprint_order": args.footprint_order,
        "uniform_samples_per_axis": args.samples,
        "reference_samples_per_axis": args.reference_samples,
        "jitter_seed": args.seed,
        "rays": filtered_work["scene_rays"],
        "parent_seconds": seconds,
        "parent_work": filtered_work,
        "uniform_seconds": old_seconds,
        "uniform_work": old_work,
        "resolved_seconds": ref_seconds,
        "resolved_work": ref_work,
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "linear_rgb_rmse": float(np.sqrt(np.mean((candidate - target) ** 2))),
        "uniform_linear_rgb_rmse": float(np.sqrt(np.mean((uniform - target) ** 2))),
        "coverage_l1_pixels": float(np.abs(coverage - ref_coverage).sum()),
        "uniform_coverage_l1_pixels": float(np.abs(old_coverage - ref_coverage).sum()),
        "filtered_coverage_pixels": float(coverage.sum()),
        "uniform_coverage_pixels": float(old_coverage.sum()),
        "resolved_coverage_pixels": float(ref_coverage.sum()),
        "filter_scope": "Area-weighted signed correction; common pixel-centre opaque baseline. Within-support quadrature and dense resolved sampling are not certified error bounds.",
    }
    return report, arrays


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    if args.paired_exact and (not args.shadow_runs or args.ray_probe):
        raise ValueError("paired exact comparison requires the image run-shadow path")
    if args.force_shadow_runs and not args.shadow_runs:
        raise ValueError("forced run comparison requires --shadow-runs")
    if args.footprint_order and (
        args.ray_probe or args.paired_exact or args.force_shadow_runs
    ):
        raise ValueError(
            "footprint study requires its independent uniform/resolved controls"
        )
    started = time.perf_counter()
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    load_seconds = time.perf_counter() - started
    preparation_seconds = 0.0
    if args.ray_probe:
        profile = lighting(parent.frame[2] * parent.frame[3], args.lighting)
        report, arrays = probe(parent, profile, args.tolerance)
    else:
        started = time.perf_counter()
        resolved = prepare_resolved(load(args.resolved))
        preparation_seconds = time.perf_counter() - started
        report, arrays = compare(parent, resolved, args)
    np.savez_compressed(args.output / "linear.npz", **arrays)
    report.update(
        head=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        parent_sha256=hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        resolved_sha256=hashlib.sha256(args.resolved.read_bytes()).hexdigest()
        if args.resolved
        else None,
        parent_load_seconds=load_seconds,
        resolved_preparation_seconds=preparation_seconds,
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        scope="Flat ENU source/control transport only; no curved Earth/terrain/world/native/device or temporal admission. Positive camera height relative to the flat base; possible interior needs resolved geometry.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ray-probe", action="store_true")
    parser.add_argument(
        "--lighting",
        choices=["day", "back", "night", "dawn-north", "dawn-slant", "dawn-diagonal"],
        default="day",
    )
    parser.add_argument("--shadow-runs", action="store_true")
    parser.add_argument("--force-shadow-runs", action="store_true")
    parser.add_argument("--paired-exact", action="store_true")
    parser.add_argument("--footprint-order", type=int, choices=range(1, 17))
    parser.add_argument(
        "--reference-samples", type=int, choices=range(1, 65), default=16
    )
    parser.add_argument("--distance", type=float, default=50000.0)
    parser.add_argument("--altitude", type=float, default=2.0)
    parser.add_argument("--motion", type=float, default=0.0)
    parser.add_argument("--samples", type=int, choices=range(1, 17), default=4)
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--tolerance", type=float, default=1e-8)
    run(parser.parse_args())
