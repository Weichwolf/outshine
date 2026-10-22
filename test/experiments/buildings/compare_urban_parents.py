"""Actual direct-light GGX parent/control images; native/world claims stay separate."""

import argparse
import hashlib
import json
import math
import os
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image
from triangle_image_reference import average_samples, image_rays, projected_bounds
from urban_parent_light import display, light_at, shade
from urban_parent_reference import prepare_resolved, trace_resolved
from urban_parent_stats import UrbanParent, compile_parent
from urban_parent_volume import render_volume, shadow_transmission
from urban_patches import load


def camera(extent, distance, motion, altitude=None):
    target = np.array([extent / 2, extent / 2, 12.0])
    eye = target + [
        motion,
        -distance,
        altitude if altitude is not None else distance / 3,
    ]
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    return (
        eye,
        np.array([right, up, forward]),
        720 / (2 * math.tan(math.pi / 8)),
        1280,
        720,
    )


def lighting(extent, name):
    if name == "night":
        return {
            "kind": "point",
            "position": [extent / 2, extent / 2, 1000],
            "intensity": 2e8,
            "ambient": 0.002,
        }
    return {
        "kind": "sun",
        "direction": [0.45, -0.5 if name == "day" else 0.5, 0.7],
        "intensity": 3.0,
        "ambient": 0.1,
    }


def ground(
    origins, directions, profile, *, resolved=None, parent=None, mode="midpoint"
):
    times = -origins[:, 2] / directions[:, 2]
    points = origins + times[:, None] * directions
    light, _, limit = light_at(points, profile)
    work = {}
    if resolved is not None:
        depth, _, _, work = trace_resolved(
            resolved, points + [0, 0, 0.001], light, minimum=0
        )
        visible = (~np.isfinite(depth) | (depth >= limit)).astype(float)
    elif parent is not None:
        visible = shadow_transmission(parent, points, light, mode)
    else:
        visible = np.ones(len(points))
    return shade(points, np.array([0, 0, 1]), -directions, 0.2, visible, profile), work


def opaque_image(resolved, origins, directions, profile):
    depth, normal, owner, work = trace_resolved(resolved, origins, directions)
    result = np.zeros_like(origins)
    hits = np.flatnonzero(np.isfinite(depth))
    points = origins[hits] + depth[hits, None] * directions[hits]
    light, _, limit = light_at(points, profile)
    lit = np.einsum("ij,ij->i", normal[hits], light) > 0
    shadow, _, _, shadow_work = trace_resolved(
        resolved, points[lit] + normal[hits][lit] * 0.001, light[lit], minimum=0
    )
    visible = np.zeros(len(points))
    visible[lit] = (~np.isfinite(shadow) | (shadow >= limit[lit])).astype(float)
    result[hits] = shade(points, normal[hits], -directions[hits], 0.6, visible, profile)
    missed = np.flatnonzero(owner < 0)
    result[missed], ground_work = ground(
        origins[missed], directions[missed], profile, resolved=resolved
    )
    return (
        result,
        np.isfinite(depth).astype(float),
        {
            "primary": work,
            "shadow": shadow_work,
            "ground_shadow": ground_work,
        },
    )


def prepare_parent(args):
    if args.parent:
        with np.load(args.parent, allow_pickle=False) as archive:
            parent = UrbanParent(
                **{key: archive[key] for key in UrbanParent.__annotations__}
            )
        return parent, json.loads(args.parent.with_suffix(".json").read_text())
    from coarse_city_domains import load_snapshot

    if args.input is None:
        raise ValueError("source snapshot or a stored parent required")
    parent, metadata = compile_parent(
        load_snapshot(args.input), divisions=args.divisions
    )
    metadata["input_snapshot_sha256"] = hashlib.sha256(
        args.input.read_bytes()
    ).hexdigest()
    return parent, metadata


def background_image(view, profile):
    origins, directions = image_rays(
        view, np.array([[0, 0], [1280, 720]]), 1280, 720, 1
    )
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    image = np.zeros_like(directions)
    down = directions[:, 2] < -1e-8
    image[~down] = 0.002 if profile["kind"] == "point" else 0.1
    image[down] = ground(origins[down], directions[down], profile)[0]
    return image.reshape(720, 1280, 3)


def prepare_reference(args):
    if args.resolved:
        return prepare_resolved(load(args.resolved)), hashlib.sha256(
            args.resolved.read_bytes()
        ).hexdigest()
    from coarse_city_domains import load_snapshot
    from urban_patch_bake import bake

    if args.input is None:
        raise ValueError(
            "offline control needs a stored resolved field or its source snapshot"
        )
    product, _ = bake(load_snapshot(args.input))
    path = args.output / "reference.npz"
    np.savez_compressed(path, **product.arrays())
    return prepare_resolved(product), hashlib.sha256(path.read_bytes()).hexdigest()


def study_rays(parent, args):
    extent = parent.frame[2] * parent.frame[3]
    view = camera(extent, args.distance, args.motion, args.altitude)
    profile = lighting(extent, args.lighting)
    pad = extent * (0.08 if args.lighting == "night" else 0.005)
    corners = np.array(
        [
            [x, y, z]
            for x in [-pad, extent + pad]
            for y in [-pad, extent + pad]
            for z in [0, 48]
        ]
    )
    box = projected_bounds(corners, view)
    low = np.maximum(np.floor(box[0]).astype(int), [0, 0])
    high = np.minimum(np.ceil(box[1]).astype(int), [1280, 720])
    width, height = high - low
    box = np.array([low, high])
    origins, directions = image_rays(view, box, width, height, args.samples)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    origins = np.array(origins)
    if np.any(directions[:, 2] >= 0):
        raise ValueError("down-looking control region required")
    return view, profile, box, origins, directions


def run(args):
    if not any(
        args.output.resolve().is_relative_to(root.resolve())
        for root in [Path(tempfile.gettempdir()), Path("/tmp")]
    ):
        raise ValueError("fresh system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    parent, metadata = prepare_parent(args)
    compile_seconds = time.perf_counter() - started
    # The parent is already complete before the independent resolved control is opened.
    np.savez_compressed(args.output / "parent.npz", **parent.arrays())
    (args.output / "parent.json").write_text(json.dumps(metadata, indent=2) + "\n")
    started = time.perf_counter()
    resolved, reference_sha256 = prepare_reference(args)
    prepare_seconds = time.perf_counter() - started
    view, profile, box, origins, directions = study_rays(parent, args)
    low, high = box
    width, height = high - low
    background = background_image(view, profile)
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": metadata["input_snapshot_sha256"],
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "renderer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "resolved_sha256": reference_sha256,
        "profile": [1280, 720],
        "lighting": profile,
        "distance_m": args.distance,
        "motion_m": args.motion,
        "altitude_m": args.altitude if args.altitude is not None else args.distance / 3,
        "parent_cache_hit": args.parent is not None,
        "samples_per_axis": args.samples,
        "roi": box.tolist(),
        "rays": len(origins),
        "parent_compile_s": compile_seconds,
        "reference_prepare_s": prepare_seconds,
        "parent_array_bytes": sum(a.nbytes for a in parent.arrays().values()),
        "reference_array_bytes": resolved["array_bytes"],
        "paths": {},
        "scope": "Standalone common direct GGX plus unoccluded ambient control, flat inferred geometry; not Blender/native material/GI, true detailed city, terrain/curvature, complete-world or device admission.",
    }
    crops = {}
    coverage = {}
    for mode in ["resolved", "raw", "midpoint", "phase"]:
        started = time.perf_counter()
        if mode == "resolved":
            values, alpha, work = opaque_image(resolved, origins, directions, profile)
        else:
            emission, transmission, work = render_volume(
                parent,
                origins,
                directions,
                profile,
                mode="midpoint" if mode == "phase" else mode,
                phase_mixture=mode == "phase",
            )
            surface, _ = ground(
                origins,
                directions,
                profile,
                parent=parent,
                mode="midpoint" if mode == "phase" else mode,
            )
            values = emission + transmission[:, None] * surface
            alpha = 1 - transmission
        seconds = time.perf_counter() - started
        crop = average_samples(values, width, height, args.samples)
        crops[mode] = crop
        coverage[mode] = average_samples(alpha[:, None], width, height, args.samples)[
            ..., 0
        ]
        image = background.copy()
        image[low[1] : high[1], low[0] : high[0]] = crop
        Image.fromarray(display(image)).save(args.output / (mode + ".png"))
        report["paths"][mode] = {
            "seconds": seconds,
            "work": work,
            "coverage_sum_px": float(coverage[mode].sum()),
        }
        if mode != "resolved":
            difference = crop - crops["resolved"]
            report["paths"][mode]["linear_rgb_roi_rmse"] = float(
                np.sqrt(np.mean(difference**2))
            )
            report["paths"][mode]["coverage_l1_px"] = float(
                np.abs(coverage[mode] - coverage["resolved"]).sum()
            )
    np.savez_compressed(args.output / "linear-crops.npz", **crops)
    np.savez_compressed(args.output / "coverage.npz", **coverage)
    report["load_state"] = {
        "pid": os.getpid(),
        "note": "Shared development load; times are Python work controls, not device/frame performance.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"roi": box.tolist(), "rays": len(origins), "paths": report["paths"]}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--parent", type=Path)
    parser.add_argument("--resolved", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--distance", type=float, default=240000)
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--divisions", type=int, default=16)
    parser.add_argument("--samples", type=int, default=4)
    run(parser.parse_args())
