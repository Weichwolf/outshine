"""Bounded pixel integration of complete urban RGB/Ready states, offline only."""

import argparse
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_horizon import region
from compare_urban_parents import camera, lighting
from PIL import Image
from urban_parent_light import display
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load
from urban_ready_pixels import (
    STATES,
    composed_scene,
    integrate,
    integrate_control,
    rss_bytes,
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
    parser.add_argument("--state", choices=STATES, default="resolved")
    parser.add_argument("--foreground-fraction", type=float, default=0.75)
    parser.add_argument("--background-samples", type=int)
    parser.add_argument("--weighted-transport", action="store_true")
    parser.add_argument("--receiver-ownership", action="store_true")
    parser.add_argument("--camera-miss-witness", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("diagnostics require an explicit system-temp output")
    if output.exists() and any(output.iterdir()):
        parser.error("fresh output directory required")
    if not 0 <= args.foreground_fraction <= 1:
        parser.error("foreground fraction must be in [0,1]")
    if args.background_samples is not None and (
        args.state == "resolved"
        or not 1 <= args.samples <= args.background_samples <= 128
    ):
        parser.error("Ready state and 1 <= correction <= background <= 128 required")
    if (
        args.receiver_ownership or args.camera_miss_witness
    ) and args.state == "resolved":
        parser.error("visibility witnesses require an explicit Ready state")
    if args.weighted_transport and args.background_samples is None:
        parser.error("weighted transport requires --background-samples")
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
    query = composed_scene(
        parent,
        resolved,
        profile,
        args.state,
        args.foreground_fraction,
        receiver_ownership=args.receiver_ownership,
        camera_miss_witness=args.camera_miss_witness,
        component=(
            "complete"
            if args.background_samples is None
            else "moments"
            if args.weighted_transport
            else "correction"
        ),
    )
    background_query = (
        composed_scene(
            parent,
            resolved,
            profile,
            args.state,
            args.foreground_fraction,
            component="background",
        )
        if args.background_samples is not None
        else None
    )
    preparation_seconds = time.perf_counter() - started
    prepared_peak = rss_bytes()
    if background_query is None:
        rgb, alpha, measured = integrate(
            resolved, view, box, profile, args, scene_query=query
        )
        arrays = {}
    else:
        rgb, arrays, measured = integrate_control(
            resolved, view, box, profile, args, query, background_query
        )
    args.output.mkdir(parents=True, exist_ok=True)
    arrays[args.state] = rgb
    if args.state == "resolved":
        arrays["coverage"] = alpha
        Image.fromarray(np.uint8(np.rint(alpha * 255))).save(
            args.output / "coverage.png"
        )
    np.savez_compressed(args.output / "linear.npz", **arrays)
    Image.fromarray(display(rgb)).save(args.output / (args.state + ".png"))
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
        "state": args.state,
        "foreground_fraction": args.foreground_fraction,
        "background_samples_per_axis": args.background_samples,
        "weighted_transport": args.weighted_transport,
        "receiver_ownership": args.receiver_ownership,
        "camera_miss_witness": args.camera_miss_witness,
        "linear_rgb_min": float(rgb.min()),
        "negative_linear_rgb_channels": int(np.count_nonzero(rgb < 0)),
        "coverage_semantics": "resolved child hits" if query is None else None,
        "preparation_seconds": preparation_seconds,
        "process_peak_after_preparation_B": prepared_peak,
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "resolved_array_B": resolved["array_bytes"],
        **measured,
        "scope": (
            "Complete composed RGB per ray, pixel box average; resolved-only "
            "state also records child-hit coverage. Mixed states requery all "
            "stops/normals/casters per ray and compose before averaging. "
            "Optional control integration adds a dense physical background to "
            "signed coarse D without clamping; both phases and total cost count. "
            "flat ENU original field, offline ROI of 1280x720. Seed/density "
            "differences are observed uncertainty, not certified error bounds. "
            "Optional receiver ownership excludes only a proven matching convex "
            "receiver body from its own shadow prior. No domain readiness is inferred. "
            "Preparation/storage/PNG outside integration; traced peak excludes "
            "prepared field. RSS includes imports/preparation and is not app budget. "
            "PNG is ROI only. No source build, pin update, World/device admission."
        ),
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), **measured}))


if __name__ == "__main__":
    main()
