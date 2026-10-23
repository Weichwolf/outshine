"""Bounded pixel integration of the unchanged resolved urban scene, offline only."""

import argparse
import hashlib
import json
import resource
import subprocess
import sys
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_urban_horizon import region
from compare_urban_parents import camera, lighting, opaque_image
from PIL import Image
from pixel_reference import integrate_pixels
from triangle_image_reference import image_rays
from urban_parent_light import display
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load


def rss_bytes():
    scale = 1 if sys.platform == "darwin" else 1024
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale


def integrate(resolved, view, box, profile, args):
    work = {}
    scene_seconds = 0.0

    def rays(pixel, samples, stream):
        low = np.asarray(pixel)
        return image_rays(
            view, np.array([low, low + 1]), 1, 1, samples, jitter_seed=stream
        )

    def scene(origins, directions):
        nonlocal scene_seconds
        started = time.perf_counter()
        rgb, alpha, counts = opaque_image(resolved, origins, directions, profile)
        scene_seconds += time.perf_counter() - started
        for group, values in counts.items():
            target = work.setdefault(group, {})
            for key, value in values.items():
                target[key] = target.get(key, 0) + value
        return rgb, alpha

    if args.trace_memory:
        tracemalloc.start()
    started = time.perf_counter()
    rgb, alpha, pixels = integrate_pixels(
        box, rays, scene, samples=args.samples, seed=args.seed, batch=args.batch
    )
    seconds = time.perf_counter() - started
    peak = tracemalloc.get_traced_memory()[1] if args.trace_memory else None
    if args.trace_memory:
        tracemalloc.stop()
    return (
        rgb,
        alpha,
        {
            "integration_seconds": seconds,
            "scene_seconds": scene_seconds,
            "integration_traced_peak_B": peak,
            "process_peak_after_integration_B": rss_bytes(),
            "pixel_work": pixels,
            "scene_work": work,
        },
    )


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=("day", "back", "night"), default="day")
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--roi", type=int, nargs=4, metavar=("X0", "Y0", "X1", "Y1"))
    parser.add_argument("--samples", type=int, default=16)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--batch", type=int, default=2048)
    parser.add_argument("--trace-memory", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("diagnostics require an explicit system-temp output")
    if output.exists() and any(output.iterdir()):
        parser.error("fresh output directory required")
    args.output = output
    return args


def main():
    args = arguments()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    dirty = bool(subprocess.check_output(["git", "status", "--porcelain"]))
    started = time.perf_counter()
    with np.load(args.parent, allow_pickle=False) as archive:
        parent = UrbanParent(
            **{key: archive[key] for key in UrbanParent.__annotations__}
        )
    resolved = prepare_resolved(load(args.resolved))
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, args.altitude)
    profile = lighting(extent, args.lighting)
    box = (
        np.asarray(args.roi).reshape(2, 2)
        if args.roi is not None
        else region(parent, resolved, view, profile)
    )
    if np.any(box < 0) or np.any(box > [1280, 720]) or np.any(box[1] <= box[0]):
        raise ValueError("nonempty pixel ROI inside 1280x720 required")
    preparation_seconds = time.perf_counter() - started
    prepared_peak = rss_bytes()
    rgb, alpha, measured = integrate(resolved, view, box, profile, args)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.output / "linear.npz", resolved=rgb, coverage=alpha)
    Image.fromarray(display(rgb)).save(args.output / "resolved.png")
    Image.fromarray(np.uint8(np.rint(alpha * 255))).save(args.output / "coverage.png")
    report = {
        "head": head,
        "working_tree_dirty": dirty,
        "parent_sha256": hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        "resolved_sha256": hashlib.sha256(args.resolved.read_bytes()).hexdigest(),
        "profile": [1280, 720],
        "target_distance_m": args.distance,
        "altitude_m": float(view[0][2]),
        "motion_m": args.motion,
        "lighting": profile,
        "roi": box.tolist(),
        "samples_per_axis": args.samples,
        "seed": args.seed,
        "batch": args.batch,
        "trace_memory": args.trace_memory,
        "preparation_seconds": preparation_seconds,
        "process_peak_after_preparation_B": prepared_peak,
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "resolved_array_B": resolved["array_bytes"],
        **measured,
        "scope": (
            "Unchanged complete opaque RGB/coverage per ray, pixel box average; "
            "flat ENU original field, offline ROI of 1280x720. Seed/density "
            "differences are observed uncertainty, not certified error bounds. "
            "Preparation/storage/PNG outside integration; traced peak excludes "
            "prepared field. RSS includes imports/preparation and is not app budget. "
            "PNG is ROI only. No source build, pin update, World/device admission."
        ),
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), **measured}))


if __name__ == "__main__":
    main()
