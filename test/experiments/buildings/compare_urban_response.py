"""Coarse projected response versus matched dense transport and resolved boxes.

All images retain the same full viewport and geometric diagnostic region. Sparse
field construction is timed, including all recipe-cell work. Only subsequent
pixel consumption has fixed field-sampling cost. Flat ENU, fixed study materials,
and no ready-child/foreground composition: no World/native replacement admission.
"""

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
from compare_urban_horizon import region
from compare_urban_parents import background_image, camera, lighting, opaque_image
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_parent_light import display
from urban_parent_reference import prepare_resolved
from urban_parent_response import ProjectedResponse, collect_response, light_geometry
from urban_parent_stats import UrbanParent
from urban_patches import load


def coordinates(width, height):
    x, y = np.meshgrid(
        (np.arange(width) + 0.5) / width, (np.arange(height) + 0.5) / height
    )
    return np.column_stack((x.ravel(), y.ravel()))


def build_field(parent, view, box, profile, shape, args):
    started = time.perf_counter()
    origins, directions = image_rays(view, box, *shape, 1)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    moments, work = collect_response(
        parent,
        origins,
        directions,
        profile,
        side=args.phase,
        phase="centre" if args.altitude is not None else "ground",
        transmission_tolerance=args.tolerance,
    )
    field = ProjectedResponse(
        moments.reshape(shape[1], shape[0], 7, 4), light_geometry(profile)
    )
    return field, {
        "build_seconds": time.perf_counter() - started,
        "grid_shape": np.asarray(shape).tolist(),
        "work": work,
    }


def consume(field, shape, profile, macro_y):
    width, height = shape
    started = time.perf_counter()
    uv = coordinates(width, height * macro_y)
    colour, coverage = field.evaluate(
        uv, profile, sky=0.002 if profile["kind"] == "point" else 0.1
    )
    colour = colour.reshape(height, macro_y, width, 3).mean(axis=1)
    coverage = coverage.reshape(height, macro_y, width).mean(axis=1)
    return (
        colour,
        coverage,
        {
            "consume_seconds": time.perf_counter() - started,
            "output_samples": len(uv),
            "recipe_cell_queries": 0,
            "bilinear_channel_taps": len(uv) * 4 * 28,
            "sampled_coefficient_bytes": len(uv) * 28 * 8,
            "nominal_four_tap_read_bytes": len(uv) * 4 * 28 * 8,
            "output_colour_coverage_bytes": colour.nbytes + coverage.nbytes,
            "uv_bytes": uv.nbytes,
        },
    )


def compare(parent, resolved, args):
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, altitude=args.altitude)
    profile = lighting(extent, args.lighting)
    box = region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    if min(width, height) < 1:
        raise ValueError("nonempty projected diagnostic required")
    dense_shape = np.array([width, height * args.macro_y])
    sparse_shape = np.ceil(dense_shape / [args.spacing_x, args.spacing_y]).astype(int)
    arrays, reports = {}, {}
    for name, shape in [("sparse", sparse_shape), ("dense", dense_shape)]:
        field, report = build_field(parent, view, box, profile, shape, args)
        colour, coverage, consumption = consume(
            field, (width, height), profile, args.macro_y
        )
        reports[name] = report | consumption
        reports[name]["total_seconds"] = (
            report["build_seconds"] + consumption["consume_seconds"]
        )
        arrays[name], arrays[name + "_coverage"] = colour, coverage
        arrays[name + "_coefficients"] = field.moments
    origins, directions = image_rays(
        view, box, width, height, args.samples, jitter_seed=args.seed
    )
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    started = time.perf_counter()
    rgb, alpha, work = opaque_image(resolved, origins, directions, profile)
    reports["resolved"] = {
        "seconds": time.perf_counter() - started,
        "rays": len(origins),
        "work": work,
    }
    arrays["resolved"] = average_samples(rgb, width, height, args.samples)
    arrays["resolved_coverage"] = average_samples(alpha, width, height, args.samples)[
        ..., 0
    ]
    for name in ["sparse", "dense"]:
        reports[name]["rgb_rmse_vs_resolved"] = float(
            np.sqrt(np.mean((arrays[name] - arrays["resolved"]) ** 2))
        )
        reports[name]["coverage_l1_pixels_vs_resolved"] = float(
            np.abs(arrays[name + "_coverage"] - arrays["resolved_coverage"]).sum()
        )
    reports["sparse"]["rgb_rmse_vs_dense"] = float(
        np.sqrt(np.mean((arrays["sparse"] - arrays["dense"]) ** 2))
    )
    reports["sparse"]["coverage_l1_pixels_vs_dense"] = float(
        np.abs(arrays["sparse_coverage"] - arrays["dense_coverage"]).sum()
    )
    backdrop = background_image(view, profile)
    for name in ["sparse", "dense", "resolved"]:
        image = backdrop.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = arrays[name]
        Image.fromarray(display(image)).save(args.output / f"{name}.png")
    report = {
        "profile": [1280, 720],
        "roi": box.tolist(),
        "lighting": profile,
        "target_distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "phase_samples_per_axis": args.phase,
        "phase_projection": "centre" if args.altitude is not None else "ground",
        "macro_samples_xy": [1, args.macro_y],
        "sparse_spacing_xy": [args.spacing_x, args.spacing_y],
        "resolved_samples_per_axis": args.samples,
        "jitter_seed": args.seed,
        "transmission_tolerance": args.tolerance,
        "comparison": reports,
        "limitations": [
            "Flat ENU; no DEM/curvature/real-Earth visibility evidence",
            "Temporary view/light-geometry-dependent fixed-material response",
            "Construction still traverses recipe cells; not scale-independent overall",
            "No certified interpolation/pixel-footprint error bound",
            "No ready-child or foreground composition; no native/World admission",
            "Python process memory/times are not native device evidence",
        ],
    }
    return report, arrays


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    if not all(
        1 <= value <= 32 for value in [args.spacing_x, args.spacing_y, args.macro_y]
    ):
        raise ValueError("bounded positive grid spacing required")
    if not 1 <= args.samples <= 32:
        raise ValueError("bounded resolved quadrature required")
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with np.load(args.parent, allow_pickle=False) as archive:
        parent = UrbanParent(
            **{key: archive[key] for key in UrbanParent.__annotations__}
        )
    parent_seconds = time.perf_counter() - started
    started = time.perf_counter()
    resolved = prepare_resolved(load(args.resolved))
    preparation_seconds = time.perf_counter() - started
    report, arrays = compare(parent, resolved, args)
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        dirty=subprocess.check_output(["git", "status", "--porcelain"], text=True),
        parent_load_seconds=parent_seconds,
        resolved_preparation_seconds=preparation_seconds,
        input_sha256={
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.parent, args.resolved]
        },
        parent_array_bytes=sum(a.nbytes for a in parent.arrays().values()),
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
    )
    np.savez_compressed(args.output / "samples.npz", **arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"output": str(args.output), "comparison": report["comparison"]}, indent=2
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", required=True, type=Path)
    parser.add_argument("--resolved", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--spacing-x", type=int, default=4)
    parser.add_argument("--spacing-y", type=int, default=1)
    parser.add_argument("--macro-y", type=int, default=2)
    parser.add_argument("--phase", type=int, default=6)
    parser.add_argument("--samples", type=int, default=16)
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--tolerance", type=float, default=1e-8)
    run(parser.parse_args())


if __name__ == "__main__":
    main()
