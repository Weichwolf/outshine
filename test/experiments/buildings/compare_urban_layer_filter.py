"""Joint phase/opaque composition against jittered pixel rays in the same scene."""

import argparse
import functools
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_urban_layers import mixed, ready_subset
from compare_urban_parents import background_image, camera, lighting, opaque_image
from compare_urban_phase import footprint_periods
from PIL import Image
from triangle_image_reference import average_samples, image_rays
from urban_parent_conditional import covered_grid, grid
from urban_parent_light import display
from urban_parent_phase import render_composed_phase
from urban_parent_reference import prepare_resolved
from urban_parent_stats import UrbanParent
from urban_patches import load


def scene_sample(scene, origins, directions):
    rgb, report = scene(origins, directions)
    work = {
        f"{group}_{key}": value
        for group in ["parent_work", "child_primary_work", "child_shadow_work"]
        for key, value in report[group].items()
    }
    for key in ["parent_array_bytes", "background_ray_arrays_bytes"]:
        work[key.replace("bytes", "peak_bytes")] = report[key]
    return rgb, work


def filter_scene(parent, o, d, ro, rd, scene, width, height, whole, args):
    started = time.perf_counter()
    baseline, baseline_report = 0, {}
    if not args.whole_phase:
        background = functools.partial(scene, view_region=whole, caster_region=whole)
        baseline, baseline_report = background(ro, rd)
        baseline = average_samples(baseline, width, height, args.reference_samples)
    baseline_seconds = time.perf_counter() - started
    pg = grid(parent)
    camera_regions = scene.keywords["view_region"]
    caster_regions = scene.keywords["caster_region"]
    replaced = covered_grid(
        pg, () if camera_regions is None else camera_regions
    ) and covered_grid(pg, () if caster_regions is None else caster_regions)
    phase_started = time.perf_counter()
    work = {"phase_rays": 0}
    delta = np.zeros((height, width, 3))
    if not replaced or args.whole_phase:
        sample = functools.partial(
            scene_sample, functools.partial(scene, correction_only=not args.whole_phase)
        )
        phase, work = render_composed_phase(parent, o, d, sample, side=args.phase)
        delta = average_samples(phase, width, height, args.macro)
    return baseline + delta, {
        "phase_seconds": time.perf_counter() - phase_started,
        "total_seconds": time.perf_counter() - started,
        "baseline_seconds": baseline_seconds,
        "baseline_report": baseline_report,
        "phase_work": work,
        "full_ready_bypass": replaced and not args.whole_phase,
    }


def check_reference(parent, resolved, view, box, profile, control, args):
    if not args.reference_check_samples:
        return {}
    width, height = box[1] - box[0]
    o, d = image_rays(
        view,
        box,
        width,
        height,
        args.reference_check_samples,
        jitter_seed=args.seed + 1,
    )
    d /= np.linalg.norm(d, axis=1)[:, None]
    image = opaque_image(resolved, o, d, profile)[0]
    image = average_samples(image, width, height, args.reference_check_samples)
    return {
        "samples_per_axis": args.reference_check_samples,
        "seed": args.seed + 1,
        "linear_rgb_difference_rmse": float(np.sqrt(np.mean((image - control) ** 2))),
        "scope": "Different sampling density/seed, not a certified error bound; bare resolved scene only.",
    }


def compare_states(parent, resolved, view, box, profile, args):
    width, height = box[1] - box[0]
    o, d = image_rays(view, box, width * args.macro, height * args.macro, 1)
    d /= np.linalg.norm(d, axis=1)[:, None]
    ro, rd = image_rays(
        view, box, width, height, args.reference_samples, jitter_seed=args.seed
    )
    rd /= np.linalg.norm(rd, axis=1)[:, None]
    pg = grid(parent)
    whole = np.array([[[0, 0], pg["shape"]]])
    half = np.array([[[0, 0], [pg["shape"][0] // 2, pg["shape"][1]]]])
    subset = ready_subset(resolved, pg, half[0])
    child_control = opaque_image(resolved, ro, rd, profile)[0]
    control = average_samples(child_control, width, height, args.reference_samples)
    arrays, reports = {"resolved": control}, {}
    states = [
        ("pending", None, None, None, None, False),
        ("half_ready", subset, subset, half, half, False),
        ("all_ready", resolved, resolved, whole, whole, False),
        ("foreground", subset, subset, half, half, True),
    ]
    for name, child, caster, region, caster_region, foreground in states:
        scene = functools.partial(
            mixed,
            parent,
            profile=profile,
            camera_child=child,
            caster_child=caster,
            view_region=region,
            caster_region=caster_region,
            foreground=foreground,
            foreground_fraction=args.foreground_fraction,
        )
        crop, filtered_report = filter_scene(
            parent, o, d, ro, rd, scene, width, height, whole, args
        )
        raw, raw_work = scene(o=o, d=d)
        raw = average_samples(raw, width, height, args.macro)
        target = control
        if foreground:
            target, _ = mixed(
                parent,
                ro,
                rd,
                profile,
                resolved,
                resolved,
                whole,
                whole,
                True,
                foreground_fraction=args.foreground_fraction,
            )
            target = average_samples(target, width, height, args.reference_samples)
            arrays["foreground_resolved"] = target
        arrays[name], arrays[f"{name}_raw"] = crop, raw
        reports[name] = {
            **filtered_report,
            "raw_report": raw_work,
            "linear_rgb_rmse": float(np.sqrt(np.mean((crop - target) ** 2))),
            "raw_linear_rgb_rmse": float(np.sqrt(np.mean((raw - target) ** 2))),
            "linear_rgb_max_abs": float(np.max(np.abs(crop - target))),
            "minimum_linear_rgb": float(crop.min()),
            "negative_pixel_count": int(
                np.count_nonzero(np.any(crop < -1e-12, axis=2))
            ),
        }
        if name == "all_ready" and not args.whole_phase:
            np.testing.assert_allclose(crop, target, rtol=0, atol=2e-14)
        if args.paired_dense:
            started = time.perf_counter()
            dense, dense_report = scene(o=ro, d=rd)
            seconds = time.perf_counter() - started
            dense = average_samples(dense, width, height, args.reference_samples)
            arrays[f"{name}_dense"] = dense
            reports[name]["paired_dense"] = {
                "seconds": seconds,
                "report": dense_report,
                "linear_rgb_rmse": float(np.sqrt(np.mean((dense - target) ** 2))),
                "phase_difference_rmse": float(np.sqrt(np.mean((dense - crop) ** 2))),
            }
    return arrays, reports


def run(args):
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        raise ValueError("private system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    with np.load(args.parent, allow_pickle=False) as a:
        parent = UrbanParent(**{k: a[k] for k in UrbanParent.__annotations__})
    resolved = prepare_resolved(load(args.resolved))
    extent = parent.frame[2] * parent.frame[3]
    view, profile = camera(extent, 240000, args.motion), lighting(extent, args.lighting)
    box = np.array([[607, 345], [673, 376]])
    periods = footprint_periods(parent, view, box)
    if periods[0] < 1 or not 0 < args.foreground_fraction < 1:
        raise ValueError(
            "unresolved pixel footprint and bounded foreground fraction required"
        )
    arrays, results = compare_states(parent, resolved, view, box, profile, args)
    reference_check = check_reference(
        parent, resolved, view, box, profile, arrays["resolved"], args
    )
    backdrop = background_image(view, profile)
    for name, crop in arrays.items():
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
        "target_distance_m": 240000,
        "motion_m": args.motion,
        "profile": [1280, 720],
        "roi": box.tolist(),
        "phase_side": args.phase,
        "macro_side": args.macro,
        "reference_samples_per_axis": args.reference_samples,
        "reference_seed": args.seed,
        "footprint_recipe_periods": periods,
        "foreground_fraction": args.foreground_fraction,
        "lighting": profile,
        "reference_check": reference_check,
        "mode": "whole_phase" if args.whole_phase else "signed_parent_correction",
        "results": results,
        "scope": "Joint composition along shifted phase rays; local stationary approximation, not exact footprint/transition/native/world/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--resolved", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--lighting", choices=["day", "back", "night"], default="day")
    parser.add_argument("--motion", type=float, default=0)
    parser.add_argument("--macro", type=int, choices=range(1, 9), default=2)
    parser.add_argument("--phase", type=int, choices=range(3, 17), default=6)
    parser.add_argument(
        "--reference-samples", type=int, choices=range(1, 33), default=16
    )
    parser.add_argument("--seed", type=int, default=1113)
    parser.add_argument("--foreground-fraction", type=float, default=0.47)
    parser.add_argument("--paired-dense", action="store_true")
    parser.add_argument("--whole-phase", action="store_true")
    parser.add_argument(
        "--reference-check-samples", type=int, choices=[0, 16, 32], default=0
    )
    run(parser.parse_args())
