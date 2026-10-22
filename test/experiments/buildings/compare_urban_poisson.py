"""Isolate correlated coarse Poisson shadows; primary slabs remain approximate.

GEOS validates projected void geometry offline. No native/frame implementation,
terrain/curvature, ready-child composition or lattice-equivalence claim.
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
from compare_urban_parents import (
    background_image,
    camera,
    ground,
    lighting,
    opaque_image,
)
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_parent_light import display, light_at, shade
from urban_parent_poisson import PoissonParent
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_parent_volume import render_volume
from urban_patches import load


def render_candidate(model, origins, directions, profile, *, joint):
    def visibility(eye, points, light):
        return model.conditional_visibility(eye, points, light, joint=joint)

    colour, transmission, work = render_volume(
        model.parent,
        origins,
        directions,
        profile,
        visibility_query=visibility,
        extinction_query=model.extinction,
    )
    points = origins - (origins[:, 2] / directions[:, 2])[:, None] * directions
    light = light_at(points, profile)[0]
    visible = visibility(origins, points, light)
    background = shade(
        points, np.array([0.0, 0, 1]), -directions, 0.2, visible, profile
    )
    return colour + transmission[:, None] * background, 1 - transmission, work


def scaling_probe(parent):
    results = []
    for factor in [1, 4, 16]:
        larger = UrbanParent(**{k: v.copy() for k, v in parent.arrays().items()})
        larger.frame[3] *= factor
        extent = larger.frame[2] * larger.frame[3]
        started = time.perf_counter()
        model = PoissonParent(larger)
        build = time.perf_counter() - started
        points = np.tile([extent / 2, extent / 2, 0.0], (256, 1))
        origins = points + [-60, -30, 100]
        lights = np.tile([0.4, -0.2, 1.0], (256, 1))
        started = time.perf_counter()
        value = model.conditional_visibility(origins, points, lights)
        results.append(
            {
                "extent_m": extent,
                "hypothetical_recipe_cells": int(
                    np.ceil(larger.frame[3] / larger.frame[4])
                )
                ** 2,
                "build_seconds": build,
                "query_seconds": time.perf_counter() - started,
                "work": model.work.copy(),
                "mean_conditional_visibility": float(value.mean()),
            }
        )
    return results


def compare(parent, resolved, args):
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, altitude=args.altitude)
    profile = lighting(extent, args.lighting)
    box = region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    o, d = image_rays(view, box, width, height, args.samples, jitter_seed=args.seed)
    d /= np.linalg.norm(d, axis=1)[:, None]
    if np.any(d[:, 2] >= 0):
        raise ValueError(
            "primary slab comparison requires downward rays; horizon is an open counterexample"
        )
    started = time.perf_counter()
    model = PoissonParent(parent)
    build_seconds = time.perf_counter() - started
    arrays, reports = {}, {}
    for name, joint in [("independent", False), ("joint", True)]:
        before = model.work.copy()
        started = time.perf_counter()
        rgb, alpha, work = render_candidate(model, o, d, profile, joint=joint)
        reports[name] = {
            "seconds": time.perf_counter() - started,
            "primary_work": work,
            "geometry_work": {k: v - before[k] for k, v in model.work.items()},
        }
        arrays[name] = average_samples(rgb, width, height, args.samples)
        arrays[name + "_coverage"] = average_samples(
            alpha, width, height, args.samples
        )[..., 0]
    np.testing.assert_array_equal(
        arrays["joint_coverage"], arrays["independent_coverage"]
    )
    started = time.perf_counter()
    colour, transmission, work = render_volume(parent, o, d, profile)
    background = ground(o, d, profile, parent=parent)[0]
    arrays["legacy"] = average_samples(
        colour + transmission[:, None] * background, width, height, args.samples
    )
    reports["legacy"] = {"seconds": time.perf_counter() - started, "work": work}
    o, d = image_rays(
        view, box, width, height, args.reference_samples, jitter_seed=args.seed
    )
    d /= np.linalg.norm(d, axis=1)[:, None]
    started = time.perf_counter()
    reference, alpha, work = opaque_image(resolved, o, d, profile)
    reports["resolved"] = {"seconds": time.perf_counter() - started, "work": work}
    arrays["resolved"] = average_samples(
        reference, width, height, args.reference_samples
    )
    arrays["resolved_coverage"] = average_samples(
        alpha, width, height, args.reference_samples
    )[..., 0]
    for name in ["independent", "joint", "legacy"]:
        reports[name]["linear_rgb_rmse"] = float(
            np.sqrt(np.mean((arrays[name] - arrays["resolved"]) ** 2))
        )
    backdrop = background_image(view, profile)
    for name in ["independent", "joint", "legacy", "resolved"]:
        image = backdrop.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = arrays[name]
        Image.fromarray(display(image)).save(args.output / f"{name}.png")
    return {
        "profile": [1280, 720],
        "roi": box.tolist(),
        "lighting": profile,
        "distance_m": args.distance,
        "altitude_m": args.altitude,
        "motion_m": args.motion,
        "samples_xy": args.samples,
        "resolved_samples_xy": args.reference_samples,
        "seed": args.seed,
        "coarse_build_seconds": build_seconds,
        "coarse_build_work": {
            k: model.work[k]
            for k in ["coarse_density_bins", "density_rate_array_bytes"]
        },
        "comparison": reports,
        "coverage_l1_pixels": float(
            np.abs(arrays["joint_coverage"] - arrays["resolved_coverage"]).sum()
        ),
        "limitations": [
            "Joint void exact only for the derived piecewise-Poisson box model",
            "Primary slab/bilinear-density approximation is not exact Poisson transport",
            "Regular lattice corridors disappear; horizon/opaque composition unresolved",
            "GEOS offline reference; no native/frame/device or complete memory claim",
            "Flat ENU, no terrain/curvature/real-Earth visibility proof",
        ],
    }, arrays


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    if not 1 <= args.samples <= 16 or not 1 <= args.reference_samples <= 32:
        raise ValueError("bounded image quadrature required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as archive:
        parent = UrbanParent(**{k: archive[k] for k in UrbanParent.__annotations__})
    started = time.perf_counter()
    resolved = prepare_resolved(load(args.resolved))
    preparation = time.perf_counter() - started
    report, arrays = compare(parent, resolved, args)
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        dirty=subprocess.check_output(["git", "status", "--porcelain"], text=True),
        resolved_preparation_seconds=preparation,
        input_sha256={
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.parent, args.resolved]
        },
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        scaling=scaling_probe(parent) if args.scaling_probe else [],
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
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--samples", type=int, default=4)
    parser.add_argument("--reference-samples", type=int, default=16)
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--scaling-probe", action="store_true")
    run(parser.parse_args())


if __name__ == "__main__":
    main()
