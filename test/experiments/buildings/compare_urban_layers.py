"""Compose a stored parent with complete ready child domains and an opaque stop."""

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
from compare_urban_parents import background_image, camera, lighting, opaque_image
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_parent_conditional import grid
from urban_parent_layer import compose, render_layer
from urban_parent_light import display, light_at, shade
from urban_parent_reference import prepare_resolved, projected_region, trace_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load


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
):
    total_started = time.perf_counter()
    stop = -o[:, 2] / d[:, 2]
    normals = np.tile([0.0, 0.0, 1.0], (len(o), 1))
    albedo = np.full(len(o), 0.2)
    primary_work = {}
    if camera_child is not None:
        depth, n, _, primary_work = trace_resolved(camera_child, o, d)
        hit = depth < stop
        stop[hit], normals[hit], albedo[hit] = depth[hit], n[hit], 0.6
    if foreground:
        plane = (120 - o[:, 2]) / d[:, 2]
        points = o + plane[:, None] * d
        width = parent.frame[2] * parent.frame[3]
        foreground_mask = (points[:, 0] < width / 2) & (plane < stop)
        stop[foreground_mask] = plane[foreground_mask]
        normals[foreground_mask], albedo[foreground_mask] = [0, 0, 1], 0.35
    points = o + stop[:, None] * d
    shadow_work = {}
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
                    & (at_x < parent.frame[2] * parent.frame[3] / 2)
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
    c, t, s, work = render_layer(
        parent,
        o,
        d,
        profile,
        stop,
        receiver_normals=normals,
        primary_covered=view_region,
        caster_covered=caster_region,
        external_visibility=external,
    )
    rgb = compose(c, t, s, direct, profile["ambient"] * albedo[:, None])
    return rgb, {
        "layer_and_compose_seconds": time.perf_counter() - started,
        "total_mixed_seconds": time.perf_counter() - total_started,
        "parent_work": work,
        "child_primary_work": primary_work,
        "child_shadow_work": shadow_work,
        "parent_array_bytes": c.nbytes + t.nbytes + s.nbytes,
        "background_ray_arrays_bytes": sum(
            a.nbytes for a in (stop, normals, albedo, points, visibility, direct)
        ),
    }


def rgb_errors(value, reference, width, height, samples):
    """Separate sample disagreement from the image produced by pixel filtering."""
    filtered = average_samples(value, width, height, samples) - average_samples(
        reference, width, height, samples
    )
    return {
        "sample_rgb_rmse": float(np.sqrt(np.mean((value - reference) ** 2))),
        "pixel_rgb_rmse": float(np.sqrt(np.mean(filtered**2))),
    }


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    resolved = prepare_resolved(load(args.resolved))
    pg = grid(parent)
    whole = np.array([[[0, 0], pg["shape"]]])
    half = np.array([[[0, 0], [pg["shape"][0] // 2, pg["shape"][1]]]])
    subset = ready_subset(resolved, pg, half[0])
    view = camera(
        parent.frame[2] * parent.frame[3], args.distance, args.motion, args.altitude
    )
    profile = lighting(parent.frame[2] * parent.frame[3], args.lighting)
    box = projected_region(parent, resolved, view, profile)
    width, height = box[1] - box[0]
    o, d = image_rays(view, box, width, height, args.samples)
    d /= np.linalg.norm(d, axis=1)[:, None]
    backdrop = background_image(view, profile)
    control, _, control_work = opaque_image(resolved, o, d, profile)
    results, arrays = {}, {"resolved": control}
    states = [
        ("pending", None, None, None, None, False),
        ("half_view", subset, None, half, None, False),
        ("half_caster", None, subset, None, half, False),
        ("half_ready", subset, subset, half, half, False),
        ("all_ready", resolved, resolved, whole, whole, False),
        ("foreground", subset, subset, half, half, True),
    ]
    for name, camera_child, caster_child, view_region, caster_region, front in states:
        rgb, report = mixed(
            parent,
            o,
            d,
            profile,
            camera_child,
            caster_child,
            view_region,
            caster_region,
            front,
        )
        arrays[name] = rgb
        if name != "foreground":
            report["bare_scene_errors"] = rgb_errors(
                rgb, control, width, height, args.samples
            )
        if name != "pending":
            report["pending_image_delta"] = rgb_errors(
                rgb, arrays["pending"], width, height, args.samples
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
        "prepared_parent_B": sum(a.nbytes for a in parent.arrays().values()),
        "prepared_reference_B": resolved["array_bytes"],
        "ready_lookup_copy_B": subset["lookup"].nbytes,
        "process_peak_RSS_B": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "profile": [1280, 720],
        "lighting": profile,
        "rays": len(o),
        "samples_per_axis": args.samples,
        "roi": box.tolist(),
        "results": results,
        "control_work": control_work,
        "scope": "Flat ENU controlled direct lighting; exact finite-ray composition, not a filtered LOD transition, terrain/curvature or native admission.",
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
    parser.add_argument("--distance", type=float, default=240000.0)
    parser.add_argument("--altitude", type=float)
    parser.add_argument("--motion", type=float, default=0.0)
    run(parser.parse_args())
