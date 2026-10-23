"""Compare crop-relative and absolute-pixel strata through moving ready states."""

import argparse
import functools
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from compare_urban_horizon import region
from compare_urban_layers import layer_states, mixed
from compare_urban_parents import background_image, camera, lighting
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_parent_layer import render_layer
from urban_parent_light import display
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load
from urban_pixel_samples import pixel_surface_packets
from urban_sampled_layer import render_sampled_layer
from urban_shadow_runs import select_shadow_query


def frame_context(parent, resolved, args):
    extent = parent.frame[2] * parent.frame[3]
    views = [
        camera(extent, 240000, args.motion_start + i * args.motion_step)
        for i in range(6)
    ]
    profile = lighting(extent, args.lighting)
    boxes = np.asarray([region(parent, resolved, view, profile) for view in views])
    base = np.array([boxes[:, 0].min(0), boxes[:, 1].max(0)])
    full = np.clip(base + [[-2, -2], [2, 2]], [0, 0], [1280, 720])
    return views, profile, base, full


def window_rays(view, full, window):
    w, h = map(int, full[1] - full[0])
    o, d = image_rays(view, full, w, h, 4)
    d /= np.linalg.norm(d, axis=1)[:, None]
    low, high = (window - full[0]) * 4
    selected = np.s_[low[1] : high[1], low[0] : high[0], :]
    return (
        o.reshape(h * 4, w * 4, 3)[selected].reshape(-1, 3),
        d.reshape(h * 4, w * 4, 3)[selected].reshape(-1, 3),
    )


def compare_frame(parent, view, profile, full, window, base, state, args):
    o, d = window_rays(view, full, window)
    width, height = map(int, window[1] - window[0])
    low, high = (base - window[0]) * 4
    crop = np.s_[low[1] : high[1], low[0] : high[0], :]
    arrays, work = {}, {}
    for method in ("reference", "crop", "screen"):
        query = render_layer
        if method != "reference":
            packets = pixel_surface_packets(
                width,
                height,
                4,
                4,
                args.seed,
                pixel_origin=tuple(map(int, window[0])) if method == "screen" else None,
            )
            query = functools.partial(
                render_sampled_layer,
                samples=4,
                seed=args.seed,
                shadow_query=select_shadow_query(parent, profile),
                surface_packets=packets,
            )
        rgb, counts = mixed(
            parent,
            o,
            d,
            profile,
            *state[1:],
            foreground_fraction=0.75,
            layer_query=query,
        )
        arrays[method] = (
            rgb.reshape(height * 4, width * 4, 3)[crop].copy().reshape(-1, 3)
        )
        work[method] = counts
    if state[0] == "all_ready":
        np.testing.assert_array_equal(arrays["crop"], arrays["reference"])
        np.testing.assert_array_equal(arrays["screen"], arrays["reference"])
    return arrays, work


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    resolved = prepare_resolved(load(args.resolved))
    views, profile, base, full = frame_context(parent, resolved, args)
    width, height = map(int, base[1] - base[0])
    states = layer_states(parent, resolved)
    order = [0, 1, 2, 3, 5, 4] if args.ready_changes else [0] * 6
    trims = [
        ((0, 0), (0, 0)),
        ((1, 1), (0, 0)),
        ((0, 0), (-1, -1)),
        ((1, 0), (0, -1)),
        ((0, 1), (-1, 0)),
        ((0, 0), (0, 0)),
    ]
    all_arrays, frames = {}, []
    residuals = {"crop": [], "screen": []}
    for i, (view, index, trim) in enumerate(zip(views, order, trims, strict=True)):
        window = full + np.asarray(trim)
        window = np.array(
            [np.minimum(window[0], base[0]), np.maximum(window[1], base[1])]
        )
        assert np.all(window[0] <= base[0]) and np.all(window[1] >= base[1])
        state = states[index]
        arrays, work = compare_frame(
            parent, view, profile, full, window, base, state, args
        )
        frame = {"state": state[0], "window": window.tolist(), "work": work}
        reference = average_samples(arrays["reference"], width, height, 4)
        for method, rgb in arrays.items():
            all_arrays[f"f{i:02d}_{method}"] = rgb
            pixels = average_samples(rgb, width, height, 4)
            image = background_image(view, profile)
            image[base[0, 1] : base[1, 1], base[0, 0] : base[1, 0]] = pixels
            Image.fromarray(display(image)).save(args.output / f"f{i:02d}-{method}.png")
            if method != "reference":
                error = pixels - reference
                residuals[method].append(error)
                frame[method + "_pixel_RMSE"] = float(np.sqrt(np.mean(error**2)))
        frames.append(frame)
    metrics = {}
    for method, values in residuals.items():
        values = np.stack(values)
        metrics[method] = {
            "pixel_RMSE": float(np.sqrt(np.mean(values**2))),
            "error_increment_RMSE": float(
                np.sqrt(np.mean(np.diff(values, axis=0) ** 2))
            ),
        }
    if not args.ready_changes and args.motion_step == 0:
        assert metrics["screen"]["error_increment_RMSE"] < 1e-12
    report = {
        "head": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "parent_sha256": hashlib.sha256(args.parent.read_bytes()).hexdigest(),
        "resolved_sha256": hashlib.sha256(args.resolved.read_bytes()).hexdigest(),
        "resolved_control_shape": resolved["shape"].tolist(),
        "resolved_control_masses": len(resolved["columns"]),
        "base": base.tolist(),
        "full": full.tolist(),
        "profile": [1280, 720],
        "lighting": args.lighting,
        "seed": args.seed,
        "frames": frames,
        "motion_start": args.motion_start,
        "motion_step": args.motion_step,
        "ready_changes": args.ready_changes,
        "metrics": metrics,
        "scope": "Six flat ENU views, same original field and all union support rays; error increments remove the complete model's change at fixed screen pixels. No reprojection, TAA, general transition or native/device admission.",
    }
    np.savez_compressed(args.output / "linear.npz", **all_arrays)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(metrics))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=("day", "back", "night"), default="day")
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--motion-start", type=float, default=0)
    parser.add_argument("--motion-step", type=float, default=0)
    parser.add_argument("--ready-changes", action="store_true")
    run(parser.parse_args())
