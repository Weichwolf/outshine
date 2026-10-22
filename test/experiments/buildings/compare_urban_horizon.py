"""Flat ENU ground/horizon transport; curvature/device admission stays separate."""

import argparse
import hashlib
import itertools
import json
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_parents import background_image, camera, lighting, opaque_image
from PIL import Image
from triangle_image_reference import average_samples, image_rays, projected_bounds
from urban_parent_conditional import grid, ground_stops, render_conditional
from urban_parent_layer import render_layer
from urban_parent_light import display, surface_radiance_upper
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load


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
    points = []
    for p in products:
        xy = np.array(
            [
                p["reference_origin_m"][:2],
                p["reference_origin_m"][:2] + p["shape"] * p["header"]["pitch_m"],
            ]
        )
        corners = np.array(list(itertools.product(xy[:, 0], xy[:, 1])), float)
        h = parent.heights[-1]
        points.extend([np.c_[corners, np.zeros(4)], np.c_[corners, np.full(4, h)]])
        if profile["kind"] == "point":
            centre = np.array(profile["position"])
            shadows = corners + (corners - centre[:2]) * h / (centre[2] - h)
        else:
            light = np.array(profile["direction"], float)
            shadows = corners - h * light[:2] / light[2]
        points.append(np.c_[shadows, np.zeros(4)])
    box = projected_bounds(np.vstack(points), view)
    return np.array(
        [
            np.maximum(np.floor(box[0]), [0, 0]),
            np.minimum(np.ceil(box[1]), [1280, 720]),
        ],
        int,
    )


def compare(parent, resolved, args):
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
    colour, coverage, work = render_conditional(
        parent, o, d, profile, transmission_tolerance=args.tolerance
    )
    sky = ~np.isfinite(ground_stops(o, d))
    colour[sky] += (1 - coverage[sky, None]) * (
        0.002 if args.lighting == "night" else 0.1
    )
    seconds = time.perf_counter() - started
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
        "resolved_work": reference_work,
        "tolerance": args.tolerance,
        "derived_linear_termination_bound": bound,
        "linear_rgb_rmse": float(np.sqrt(np.mean((crop - target) ** 2))),
        "coverage_l1_pixels": float(
            np.abs(arrays["coverage"] - arrays["resolved_coverage"]).sum()
        ),
    }
    return report, arrays


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    if args.ray_probe:
        profile = lighting(parent.frame[2] * parent.frame[3], args.lighting)
        report, arrays = probe(parent, profile, args.tolerance)
    else:
        report, arrays = compare(parent, prepare_resolved(load(args.resolved)), args)
    np.savez_compressed(args.output / "linear.npz", **arrays)
    report.update(
        head=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        working_tree_dirty=bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        parent_sha256=hashlib.sha256(args.parent.read_bytes()).hexdigest(),
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
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--distance", type=float, default=50000.0)
    parser.add_argument("--altitude", type=float, default=2.0)
    parser.add_argument("--motion", type=float, default=0.0)
    parser.add_argument("--samples", type=int, choices=range(1, 17), default=4)
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--tolerance", type=float, default=1e-8)
    run(parser.parse_args())
