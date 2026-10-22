"""Compare local phase quadrature against a frozen full-image control run."""

import argparse
import hashlib
import json
import resource
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_parents import camera
from PIL import Image
from triangle_image_reference import image_rays
from urban_parent_adaptive import adaptive_axes, rays_at
from urban_parent_light import display
from urban_parent_phase import render_phase
from urban_parent_stats import UrbanParent
from urban_parent_transfer import ShadowTransfer, collect_transfer


def footprint_periods(parent, view, box):
    """Finite pixel-edge Jacobian on the flat study ground, in recipe pitches."""
    width, height = box[1] - box[0]
    origins, directions = image_rays(view, box, width, height, 1)
    floors = []
    for delta in [np.zeros(3), view[1][0] / view[2], -view[1][1] / view[2]]:
        ray = directions + delta
        if np.any(ray[:, 2] >= 0):
            raise ValueError("down-looking pixel footprint required")
        floors.append(origins - (origins[:, 2] / ray[:, 2])[:, None] * ray)
    jacobian = np.stack((floors[1] - floors[0], floors[2] - floors[0]), axis=2)
    singular = np.linalg.svd(jacobian[:, :2], compute_uv=False)[:, -1]
    periods = singular / (parent.frame[2] * parent.frame[4])
    return float(periods.min()), float(periods.max())


def build_shadow_transfer(parent, view, box, profile, macro_shape, phase_side, args):
    if args.adaptive_shadow and not args.shadow_stride:
        raise ValueError("adaptive sampling requires positive base spacing")
    if not args.shadow_stride:
        return None, {"seconds": 0, "work": {}}
    if not 1 <= args.shadow_stride <= 32:
        raise ValueError("bounded transfer spacing required")
    width, height = box[1] - box[0]
    stride_y = (
        args.shadow_stride if args.shadow_stride_y is None else args.shadow_stride_y
    )
    stride = [args.shadow_stride, stride_y]
    if not 1 <= stride[1] <= 32:
        raise ValueError("bounded vertical transfer spacing required")
    planning_started = time.perf_counter()
    axes, planning = None, {}
    if args.adaptive_shadow:
        axes, planning = adaptive_axes(parent, view, box, profile, stride)
        shape = np.array([len(axis) for axis in axes])
        origins, directions = rays_at(view, box, axes)
    else:
        shape = np.ceil(np.array([width, height]) / stride).astype(int)
        origins, directions = image_rays(view, box, *shape, 1)
        directions /= np.linalg.norm(directions, axis=1)[:, None]
    planning["seconds"] = time.perf_counter() - planning_started
    started = time.perf_counter()
    moments, work = collect_transfer(
        parent, np.array(origins), directions, profile, side=args.transfer_phase
    )
    moments = moments.reshape(shape[1], shape[0], *moments.shape[1:])
    return ShadowTransfer(moments, macro_shape, phase_side, axes=axes), {
        "seconds": time.perf_counter() - started,
        "work": work,
        "grid_shape": shape.tolist(),
        "spacing_xy": stride,
        "planning": planning,
        "phase_samples_per_axis": args.transfer_phase,
    }


def paired_control(
    parent, origins, directions, profile, shape, phase_side, crop, shadow_transfer=None
):
    width, height, macro_x, macro_y = shape
    started = time.perf_counter()
    rgb, alpha, work = render_phase(
        parent,
        origins,
        directions,
        profile,
        side=phase_side,
        shadow_transfer=shadow_transfer,
    )
    seconds = time.perf_counter() - started
    exact = rgb.reshape(height, macro_y, width, macro_x, 3).mean((1, 3))
    coverage = alpha.reshape(height, macro_y, width, macro_x).mean((1, 3))
    return (
        exact,
        coverage,
        {
            "seconds": seconds,
            "work": work,
            "linear_rgb_difference_rmse": float(np.sqrt(np.mean((exact - crop) ** 2))),
        },
    )


def uniform_control(parent, view, box, origins, directions, profile, shape, crop, args):
    if not 1 <= args.paired_uniform <= 32:
        raise ValueError("positive bounded uniform reference spacing required")
    reference_args = argparse.Namespace(**vars(args))
    reference_args.adaptive_shadow = False
    reference_args.shadow_stride = args.paired_uniform
    reference_args.shadow_stride_y = args.paired_uniform_y
    width, height, macro_x, macro_y = shape
    started = time.perf_counter()
    transfer, build = build_shadow_transfer(
        parent,
        view,
        box,
        profile,
        (width * macro_x, height * macro_y),
        args.phase,
        reference_args,
    )
    image, coverage, report = paired_control(
        parent, origins, directions, profile, shape, args.phase, crop, transfer
    )
    report["total_seconds"] = time.perf_counter() - started
    report["transfer_build"] = build
    return image, coverage, report


def write_outputs(output, reference, box, crop, coverage, paired, transfer):
    output.mkdir(parents=True, exist_ok=False)
    with Image.open(reference / "resolved.png") as source:
        image = np.array(source)
    low, high = box
    image[low[1] : high[1], low[0] : high[0]] = display(crop)
    Image.fromarray(image).save(output / "filtered.png")
    if paired is not None:
        image[low[1] : high[1], low[0] : high[0]] = display(paired)
        Image.fromarray(image).save(output / "paired.png")
    np.savez_compressed(
        output / "samples.npz",
        colour=crop,
        coverage=coverage,
        **({"paired_colour": paired} if paired is not None else {}),
    )
    if transfer is not None:
        extra = (
            {}
            if transfer.axes is None
            else {"x_nodes": transfer.axes[0], "y_nodes": transfer.axes[1]}
        )
        np.savez_compressed(output / "transfer.npz", moments=transfer.moments, **extra)


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("fresh system-temp output required")
    control = json.loads((args.reference / "report.json").read_text())
    if control["working_tree_dirty"] or control["profile"] != [1280, 720]:
        raise ValueError("frozen full-size control required")
    with np.load(args.reference / "parent.npz", allow_pickle=False) as archive:
        parent = UrbanParent(**{k: archive[k] for k in UrbanParent.__annotations__})
    extent = parent.frame[2] * parent.frame[3]
    view = camera(
        extent, control["distance_m"], control["motion_m"], control["altitude_m"]
    )
    np.testing.assert_allclose(view[0], control["eye_m"], rtol=0, atol=1e-9)
    np.testing.assert_allclose(view[1], control["camera_basis"], rtol=0, atol=1e-14)
    box = np.array(control["roi"])
    period_range = footprint_periods(parent, view, box)
    if period_range[0] < 1:
        raise ValueError("resolved recipe cells require the detailed consumer")
    width, height = box[1] - box[0]
    macro_x = args.macro_x if args.macro_x is not None else args.macro
    macro_y = args.macro_y if args.macro_y is not None else args.macro
    if not all(type(n) is int and 1 <= n <= 32 for n in [macro_x, macro_y]):
        raise ValueError("bounded macro quadrature required")
    origins, directions = image_rays(view, box, width * macro_x, height * macro_y, 1)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    origins = np.array(origins)
    candidate_started = time.perf_counter()
    transfer, transfer_report = build_shadow_transfer(
        parent,
        view,
        box,
        control["lighting"],
        (width * macro_x, height * macro_y),
        args.phase,
        args,
    )
    started = time.perf_counter()
    rgb, alpha, work = render_phase(
        parent,
        origins,
        directions,
        control["lighting"],
        side=args.phase,
        shadow_transfer=transfer,
    )
    seconds = time.perf_counter() - started
    candidate_seconds = time.perf_counter() - candidate_started
    crop = rgb.reshape(height, macro_y, width, macro_x, 3).mean((1, 3))
    coverage = alpha.reshape(height, macro_y, width, macro_x).mean((1, 3))
    paired, paired_report = None, None
    if args.paired_uniform is not None:
        paired, paired_coverage, paired_report = uniform_control(
            parent,
            view,
            box,
            origins,
            directions,
            control["lighting"],
            (width, height, macro_x, macro_y),
            crop,
            args,
        )
        paired_report["coverage_bit_equal"] = np.array_equal(coverage, paired_coverage)
    elif args.paired:
        paired, paired_coverage, paired_report = paired_control(
            parent,
            origins,
            directions,
            control["lighting"],
            (width, height, macro_x, macro_y),
            args.phase,
            crop,
        )
        paired_report["coverage_bit_equal"] = np.array_equal(coverage, paired_coverage)
    write_outputs(args.output, args.reference, box, crop, coverage, paired, transfer)
    with np.load(args.reference / "linear-crops.npz", allow_pickle=False) as reference:
        errors = {
            key: float(np.sqrt(np.mean((crop - reference[key]) ** 2)))
            for key in ["resolved", "conditional"]
        }
    with np.load(args.reference / "coverage.npz", allow_pickle=False) as reference:
        coverage_error = {
            key: float(np.abs(coverage - reference[key]).sum())
            for key in ["resolved", "conditional"]
        }
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "working_tree_dirty": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "control_producer": control["producer"],
        "control_report_sha256": hashlib.sha256(
            (args.reference / "report.json").read_bytes()
        ).hexdigest(),
        "profile": [1280, 720],
        "ground_pixel_period_range": period_range,
        "macro_samples_xy": [macro_x, macro_y],
        "phase_samples_per_axis": args.phase,
        "seconds": seconds,
        "candidate_seconds": candidate_seconds,
        "paired_exact": paired_report if args.paired else None,
        "paired_uniform": paired_report if args.paired_uniform is not None else None,
        "transfer_build": transfer_report,
        "work": work,
        "linear_rgb_roi_rmse": errors,
        "coverage_l1_px": coverage_error,
        "coverage_sum_px": float(coverage.sum()),
        "raw_process_peak_rss_platform_units": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "scope": "Local stationary phase approximation with actual per-event lighting; not a near-detail path, baked colour, device or world admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--macro", type=int, default=2)
    parser.add_argument("--macro-x", type=int)
    parser.add_argument("--macro-y", type=int)
    parser.add_argument("--phase", type=int, default=4)
    parser.add_argument("--shadow-stride", type=int, default=0)
    parser.add_argument("--shadow-stride-y", type=int)
    parser.add_argument("--adaptive-shadow", action="store_true")
    parser.add_argument("--transfer-phase", type=int, default=6)
    paired_group = parser.add_mutually_exclusive_group()
    paired_group.add_argument("--paired", action="store_true")
    paired_group.add_argument("--paired-uniform", type=int)
    parser.add_argument("--paired-uniform-y", type=int)
    run(parser.parse_args())
