"""Compose a stored parent with complete ready child domains and an opaque stop."""

import argparse
import functools
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_horizon import region
from compare_urban_parents import background_image, camera, lighting, opaque_image
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_opaque_receivers import opaque_receivers
from urban_parent_conditional import grid
from urban_parent_layer import compose, correction, render_layer
from urban_parent_light import display, light_at, shade
from urban_parent_reference import prepare_resolved, trace_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load
from urban_pixel_samples import pixel_surface_packets
from urban_sampled_layer import render_sampled_layer
from urban_shadow_runs import select_shadow_query


def ready_subset(resolved, parent_grid, region):
    """Offline child control only; preserve empty cells inside the ready domain."""
    result = resolved.copy()
    y, x = np.indices(resolved["lookup"].shape)
    offset = (resolved["reference_origin_m"] - parent_grid["reference_origin_m"])[:2]
    cells = np.stack((x, y), axis=-1) + np.rint(
        offset / resolved["header"]["pitch_m"]
    ).astype(int)
    inside = np.all((cells >= region[0]) & (cells < region[1]), axis=-1)
    result["lookup"] = np.where(inside, resolved["lookup"], -1)
    return result


def layer_states(parent, resolved):
    pg = grid(parent)
    whole = np.array([[[0, 0], pg["shape"]]])
    half = np.array([[[0, 0], [pg["shape"][0] // 2, pg["shape"][1]]]])
    subset = ready_subset(resolved, pg, half[0])
    return [
        ("pending", None, None, None, None, False),
        ("half_view", subset, None, half, None, False),
        ("half_caster", None, subset, None, half, False),
        ("half_ready", subset, subset, half, half, False),
        ("all_ready", resolved, resolved, whole, whole, False),
        ("foreground", subset, subset, half, half, True),
    ]


def child_visibility(product, points, normals, profile, work=None):
    light, _, limit = light_at(points, profile)
    depth, _, _, counts = trace_resolved(
        product, points + normals * 0.001, light, minimum=0, inside_is_hit=True
    )
    if work is not None:
        for key, value in counts.items():
            work[key] = work.get(key, 0) + value
    return (~np.isfinite(depth) | (depth >= limit)).astype(float)


def mixed(
    parent,
    o,
    d,
    profile,
    camera_child,
    caster_child,
    view_region,
    caster_region,
    foreground,
    *,
    foreground_fraction=0.5,
    correction_only=False,
    return_moments=False,
    receiver_ownership=False,
    layer_query=render_layer,
):
    if correction_only and return_moments:
        raise ValueError("choose signed correction or joint moments")
    total_started = time.perf_counter()
    stop, normals, albedo, receiver_cells, primary_work = opaque_receivers(
        parent,
        o,
        d,
        camera_child,
        foreground,
        foreground_fraction,
        ownership=receiver_ownership,
    )
    shadow_work = {}
    points = o + stop[:, None] * d
    external = None
    visibility = np.ones(len(o))
    if caster_child is not None or foreground:

        def external(points, normals):
            visibility = np.ones(len(points))
            if caster_child is not None:
                visibility = child_visibility(
                    caster_child, points, normals, profile, shadow_work
                )
            if foreground:
                light, _, limit = light_at(points, profile)
                origin = points + normals * 0.001
                distance = (120 - origin[:, 2]) / light[:, 2]
                at_x = origin[:, 0] + distance * light[:, 0]
                blocked = (
                    (distance > 0)
                    & (distance < limit)
                    & (at_x < parent.frame[2] * parent.frame[3] * foreground_fraction)
                )
                visibility[blocked] = 0
            return visibility

        visibility = external(points, normals)
    direct = np.zeros_like(o)
    for value in np.unique(albedo):
        at = albedo == value
        direct[at] = shade(
            points[at],
            normals[at],
            -d[at],
            value,
            visibility[at],
            profile | {"ambient": 0},
        )
    started = time.perf_counter()
    receiver_options = (
        {} if receiver_cells is None else {"receiver_cells": receiver_cells}
    )
    c, t, s, work = layer_query(
        parent,
        o,
        d,
        profile,
        stop,
        receiver_normals=normals,
        primary_covered=view_region,
        caster_covered=caster_region,
        external_visibility=external,
        **receiver_options,
    )
    if return_moments:
        from urban_transport_moments import pack_moments

        rgb = pack_moments(c, t, s, direct, profile["ambient"] * albedo[:, None])
    else:
        composite = correction if correction_only else compose
        rgb = composite(c, t, s, direct, profile["ambient"] * albedo[:, None])
    return rgb, {
        "layer_and_compose_seconds": time.perf_counter() - started,
        "total_mixed_seconds": time.perf_counter() - total_started,
        "parent_work": work,
        "child_primary_work": primary_work,
        "child_shadow_work": shadow_work,
        "parent_array_bytes": c.nbytes + t.nbytes + s.nbytes,
        "moment_output_peak_bytes": rgb.nbytes if return_moments else 0,
        "background_ray_arrays_bytes": sum(
            a.nbytes for a in (stop, normals, albedo, points, visibility, direct)
        ),
    }


def layer_state(parent, o, d, profile, state, args, arrays):
    name, child, caster, region, caster_region, front = state
    query = render_layer
    if args.surface_samples is not None:
        query = functools.partial(
            render_sampled_layer,
            samples=args.surface_samples,
            seed=args.surface_seed,
            batch=args.layer_batch,
            shadow_query=select_shadow_query(parent, profile),
            surface_packets=(
                pixel_surface_packets(
                    *map(int, args.pixel_shape),
                    args.samples,
                    args.surface_samples,
                    args.surface_seed,
                    batch=args.layer_batch,
                    pixel_origin=args.pixel_origin,
                )
                if args.pixel_strata
                else None
            ),
        )
    inputs = (parent, o, d, profile, child, caster, region, caster_region, front)
    rgb, report = mixed(
        *inputs, layer_query=query, foreground_fraction=args.foreground_fraction
    )
    if args.paired_sampled:
        reference, control = mixed(
            *inputs,
            layer_query=functools.partial(render_layer, batch=args.layer_batch),
            foreground_fraction=args.foreground_fraction,
        )
        arrays[name + "_control"] = reference
        mean = lambda value: average_samples(value, *args.pixel_shape, args.samples)
        report["paired_control"] = {
            **control,
            "max_sample_rgb_difference": float(np.abs(rgb - reference).max()),
            "pixel_rgb_rmse": float(
                np.sqrt(np.mean((mean(rgb) - mean(reference)) ** 2))
            ),
        }
        if name == "all_ready":
            np.testing.assert_array_equal(rgb, reference)
    return rgb, report


def run(args):
    if args.paired_sampled and args.surface_samples is None:
        raise ValueError("paired sampling requires --surface-samples")
    if args.pixel_strata and args.surface_samples is None:
        raise ValueError("pixel stratification requires --surface-samples")
    if args.screen_strata and not args.pixel_strata:
        raise ValueError("screen anchoring requires --pixel-strata")
    if not 0 <= args.foreground_fraction <= 1:
        raise ValueError("foreground fraction must lie in [0,1]")
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    resolved = prepare_resolved(load(args.resolved))
    view = camera(
        parent.frame[2] * parent.frame[3], args.distance, args.motion, args.altitude
    )
    profile = lighting(parent.frame[2] * parent.frame[3], args.lighting)
    box = region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    args.pixel_shape = (width, height)
    args.pixel_origin = tuple(map(int, box[0])) if args.screen_strata else None
    o, d = image_rays(view, box, width, height, args.samples)
    d /= np.linalg.norm(d, axis=1)[:, None]
    backdrop = background_image(view, profile)
    control, _, control_work = opaque_image(resolved, o, d, profile)
    results, arrays = {}, {"resolved": control}
    states = layer_states(parent, resolved)
    for state in states:
        name = state[0]
        rgb, report = layer_state(parent, o, d, profile, state, args, arrays)
        arrays[name] = rgb
        report["linear_rgb_vs_resolved_rmse"] = float(
            np.sqrt(np.mean((rgb - control) ** 2))
        )
        if name != "pending":
            report["linear_rgb_vs_pending_rmse"] = float(
                np.sqrt(np.mean((rgb - arrays["pending"]) ** 2))
            )
        results[name] = report
    np.testing.assert_allclose(arrays["all_ready"], control, rtol=0, atol=2e-14)
    for name, rgb in arrays.items():
        crop = average_samples(rgb, width, height, args.samples)
        image = backdrop.copy()
        image[box[0, 1] : box[1, 1], box[0, 0] : box[1, 0]] = crop
        Image.fromarray(display(image)).save(args.output / f"{name}.png")
    np.savez_compressed(args.output / "linear.npz", **arrays)
    report = {
        "head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True)
        ),
        "parent_sha256": hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        "resolved_sha256": hashlib.sha256(args.resolved.read_bytes()).hexdigest(),
        "target_distance_m": args.distance,
        "altitude_m": float(view[0][2]),
        "motion_m": args.motion,
        "foreground_fraction": args.foreground_fraction,
        "layer_batch": args.layer_batch,
        "profile": [1280, 720],
        "lighting": profile,
        "rays": len(o),
        "samples_per_axis": args.samples,
        "surface_samples_per_ray": args.surface_samples,
        "surface_seed": args.surface_seed,
        "pixel_strata": args.pixel_strata,
        "screen_strata": args.screen_strata,
        "pixel_origin": args.pixel_origin,
        "surface_sampling_scope": "One pixel-row band plus ray packet; generation inside layer timings.",
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "roi": box.tolist(),
        "results": results,
        "control_work": control_work,
        "scope": "Flat ENU finite-ray composition, optional sampled C with deterministic T/S; not a filtered LOD transition, terrain/curvature or native admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--samples", type=int, choices=range(1, 17), default=4)
    parser.add_argument("--surface-samples", type=int, choices=range(1, 513))
    parser.add_argument("--surface-seed", type=int, default=1113)
    parser.add_argument("--paired-sampled", action="store_true")
    parser.add_argument("--pixel-strata", action="store_true")
    parser.add_argument("--screen-strata", action="store_true")
    parser.add_argument("--layer-batch", type=int, choices=range(1, 2049), default=2048)
    parser.add_argument("--foreground-fraction", type=float, default=0.5)
    parser.add_argument("--distance", type=float, default=240000.0)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0.0)
    run(parser.parse_args())
