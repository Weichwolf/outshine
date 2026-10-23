"""Bounded pixel integration of complete urban RGB/Ready states, offline only."""

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

STATES = (
    "resolved",
    "pending",
    "half_view",
    "half_caster",
    "half_ready",
    "all_ready",
    "foreground",
)


def accumulate_work(total, counts):
    for key, value in counts.items():
        if isinstance(value, dict):
            accumulate_work(total.setdefault(key, {}), value)
        elif key.endswith("peak_bytes") or key in {
            "parent_array_bytes",
            "background_ray_arrays_bytes",
        }:
            total[key] = max(total.get(key, 0), value)
        else:
            total[key] = total.get(key, 0) + value


def opaque_background(parent, origins, directions, profile, stop, **kwargs):
    """Zero unknown-parent contribution; keep mixed()'s actual opaque receiver."""
    return (
        np.zeros_like(origins),
        np.ones(len(origins)),
        np.ones(len(origins)),
        {"opaque_background_rays": len(origins)},
    )


def composed_scene(
    parent, resolved, profile, state, foreground_fraction, *, component="complete"
):
    if component not in {"complete", "correction", "background"}:
        raise ValueError("complete, correction or background component required")
    if state == "resolved":
        if component != "complete":
            raise ValueError("Ready state required for background/correction")
        return None
    from compare_urban_layers import layer_states, mixed

    matches = [value for value in layer_states(parent, resolved) if value[0] == state]
    if len(matches) != 1 or not 0 <= foreground_fraction <= 1:
        raise ValueError(
            "existing Ready state and bounded foreground fraction required"
        )
    _, child, caster, view_region, caster_region, foreground = matches[0]

    def query(origins, directions):
        options = {"correction_only": component == "correction"}
        if component == "background":
            options["layer_query"] = opaque_background
        rgb, work = mixed(
            parent,
            origins,
            directions,
            profile,
            child,
            caster,
            view_region,
            caster_region,
            foreground,
            foreground_fraction=foreground_fraction,
            **options,
        )
        # Complete RGB includes the opaque ground/environment. This channel is
        # unused; it is not the resolved-only child-hit coverage diagnostic.
        return rgb, np.ones(len(origins)), work

    return query


def rss_bytes():
    scale = 1 if sys.platform == "darwin" else 1024
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * scale


def integrate(resolved, view, box, profile, args, *, scene_query=None):
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
        rgb, alpha, counts = (
            opaque_image(resolved, origins, directions, profile)
            if scene_query is None
            else scene_query(origins, directions)
        )
        scene_seconds += time.perf_counter() - started
        accumulate_work(work, counts)
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


def integrate_control(
    resolved, view, box, profile, args, correction_query, background_query
):
    if not (
        type(args.samples) is int
        and type(args.background_samples) is int
        and 1 <= args.samples <= args.background_samples <= 128
        and args.state in STATES[1:]
    ):
        raise ValueError(
            "bounded background density at least correction density required"
        )
    phase_args = argparse.Namespace(**(vars(args) | {"trace_memory": False}))
    if args.trace_memory:
        tracemalloc.start()
    started = time.perf_counter()
    peak = None
    try:
        if args.state == "all_ready":
            width, height = np.asarray(box)[1] - np.asarray(box)[0]
            delta = np.zeros((height, width, 3))
            correction_work = {
                "integration_seconds": 0.0,
                "scene_seconds": 0.0,
                "pixel_work": {"rays": 0},
                "explicit_full_ready_zero_correction_pixels": int(width * height),
            }
        else:
            delta, _, correction_work = integrate(
                resolved, view, box, profile, phase_args, scene_query=correction_query
            )
        phase_args.samples = args.background_samples
        background, _, background_work = integrate(
            resolved, view, box, profile, phase_args, scene_query=background_query
        )
        rgb = background + delta
    finally:
        if args.trace_memory:
            peak = tracemalloc.get_traced_memory()[1]
            tracemalloc.stop()
    seconds = time.perf_counter() - started
    if not np.isfinite(rgb).all():
        raise ValueError("finite composed control image required")
    return (
        rgb,
        {"background": background, "correction": delta},
        {
            "integration_seconds": seconds,
            "scene_seconds": correction_work["scene_seconds"]
            + background_work["scene_seconds"],
            "integration_traced_peak_B": peak,
            "process_peak_after_integration_B": rss_bytes(),
            "correction_phase": correction_work,
            "background_phase": background_work,
            "method": "dense background plus signed coarse correction",
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
    parser.add_argument("--state", choices=STATES, default="resolved")
    parser.add_argument("--foreground-fraction", type=float, default=0.75)
    parser.add_argument("--background-samples", type=int)
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
        component="complete" if args.background_samples is None else "correction",
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
            "Preparation/storage/PNG outside integration; traced peak excludes "
            "prepared field. RSS includes imports/preparation and is not app budget. "
            "PNG is ROI only. No source build, pin update, World/device admission."
        ),
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"output": str(args.output), **measured}))


if __name__ == "__main__":
    main()
