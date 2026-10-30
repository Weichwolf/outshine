"""Reproject the whole native field opponents and retain unqualified originals."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from itertools import product
from pathlib import Path

import numpy as np
from compare_field_depth_reprojection import errors, image_panel, nearest_colour
from experiment_output import create_output
from field_depth_reprojection import Reprojected, reproject
from native_world_inputs import DATA, POSES, clip_from_original, load


def capture_region(capture, world, raster):
    """Full declared capture volume, independent of fine geometry or hit samples."""
    corners = np.array(list(product((-1, 1), (-1, 1), (0, 1))))
    clip = np.c_[corners, np.ones(8)] @ (world @ np.linalg.inv(capture)).T
    if not np.isfinite(clip).all() or np.any(clip[:, 3] <= 0):
        raise ValueError("Capture volume crosses World eye; retain original group")
    xy = (clip[:, :2] / clip[:, 3:] * [1, -1] + 1) * raster / 2
    low = np.maximum(0, np.floor(xy.min(0) - 2)).astype(int)
    high = np.minimum(raster, np.ceil(xy.max(0) + 2)).astype(int)
    if np.any(high <= low) or np.prod(high - low) > 65536:
        raise ValueError("Bounded visible capture footprint required")
    return np.r_[low, high]


def capture_for_pose(arrays, cases, wanted, available=None):
    available = wanted if available is None else available
    actual, expected = (cases["capture", p] for p in (available, wanted))
    if (
        actual["capture"] != expected["capture"]
        or actual["lighting_camera"] != expected["lighting_camera"]
    ):
        raise ValueError("Changed capture/PBR observer retains original group")
    return tuple(arrays["capture/" + available + "/" + c] for c in ("rgba", "depth"))


def pixels(crop):
    x0, y0, x1, y1 = crop
    y, x = np.mgrid[y0:y1, x0:x1]
    return np.c_[x.ravel(), y.ravel()] + 0.5


def model_image(field, capture, world, raster, outer_crop, region):
    points = pixels(region)
    value = reproject(*field, capture, world, points, tuple(raster))
    x0, y0, x1, y1 = outer_crop
    shape = y1 - y0, x1 - x0
    colour, depth, evaluated = (
        np.zeros((*shape, 4), field[0].dtype),
        np.zeros(shape),
        np.ones(shape, bool),
    )
    target = np.s_[region[1] - y0 : region[3] - y0, region[0] - x0 : region[2] - x0]
    local_shape = region[3] - region[1], region[2] - region[0]
    colour[target], depth[target], evaluated[target] = (
        value.rgba.reshape(*local_shape, 4),
        value.reverse_z.reshape(local_shape),
        value.evaluated.reshape(local_shape),
    )
    work = dict(value.work)
    work["known_region_scatter_bytes"] = sum(
        v.nbytes for v in (value.rgba, value.reverse_z, value.evaluated)
    )
    work["result_payload_bytes"] = colour.nbytes + depth.nbytes + evaluated.nbytes
    return Reprojected(colour, depth, evaluated, work)


def reduce_samples(image, crop, axis):
    """Preserve global pixel phase; outside the lossless native crop is clear."""
    low, high = np.asarray(crop[:2]), np.asarray(crop[2:])
    start, end = low // axis * axis, (high + axis - 1) // axis * axis
    padded = np.zeros(
        (end[1] - start[1], end[0] - start[0], image.shape[2]), dtype=image.dtype
    )
    padded[
        low[1] - start[1] : high[1] - start[1], low[0] - start[0] : high[0] - start[0]
    ] = image
    height, width = padded.shape[:2]
    return padded.reshape(
        height // axis, axis, width // axis, axis, image.shape[2]
    ).mean((1, 3), dtype=np.float64)


def composition(arrays, pose, model, crop, axis, output, rgb_limit, coverage_limit):
    records = {}
    source = arrays["original/" + pose + "/rgba"]
    source_z = arrays["original/" + pose + "/depth"]
    for outer in ("terrain", "external"):
        other, other_z = (
            arrays[outer + "/" + pose + "/" + c] for c in ("rgba", "depth")
        )
        truth = arrays["union-" + outer + "/" + pose + "/rgba"]
        actual = nearest_colour(model.rgba, model.reverse_z, other, other_z)
        target, candidate = (reduce_samples(v, crop, axis) for v in (truth, actual))
        original_visible = ((source_z > 0) & (source_z > other_z)).astype(float)
        model_visible = ((model.reverse_z > 0) & (model.reverse_z > other_z)).astype(
            float
        )
        visibility_error = float(
            abs(
                reduce_samples(model_visible[:, :, None], crop, axis)
                - reduce_samples(original_visible[:, :, None], crop, axis)
            ).max()
        )
        error = errors(candidate, target)
        passes = bool(
            model.evaluated.all()
            and error["rgb_max"] <= rgb_limit
            and visibility_error <= coverage_limit
        )
        records[outer] = {
            "composed_image_error": error,
            "visible_source_coverage_error_max": visibility_error,
            "inside_configured_finite_error_limits": passes,
            "original_group_retained": not passes,
            "runtime_admission": False,
        }
        # Re-evaluate the retained whole original group at the same samples.
        # The union oracle is used only to check it, never to fill failed pixels.
        chosen_points = nearest_colour(
            model.rgba if passes else source,
            model.reverse_z if passes else source_z,
            other,
            other_z,
        )
        chosen = reduce_samples(chosen_points, crop, axis)
        if not passes and not np.array_equal(chosen, target):
            raise ValueError("Retained original group does not reproduce native union")
        records[outer]["chosen_path_error"] = errors(chosen, target)
        records[outer]["fallback_scope"] = (
            "affected whole source group; all other native point products unchanged"
        )
        np.savez_compressed(
            output / (pose + "-" + outer + "-composition.npz"),
            candidate=candidate,
            chosen=chosen,
            original_union=target,
        )
        image_panel(
            output / (pose + "-" + outer + "-fallback.png"),
            target,
            chosen,
            candidate,
            labels=(
                "native original union",
                "chosen source-group path",
                "unqualified candidate",
                "chosen-path error",
            ),
            scale=12,
        )
        # Independent filtering loses both subpixel correlation and depth ordering.
        front, back = (reduce_samples(v, crop, axis) for v in (model.rgba, other))
        after = front + (1 - front[:, :, 3:]) * back
        records[outer]["independent_filter_then_global_over_error"] = errors(
            after, target
        )
        image_panel(
            output / (pose + "-" + outer + ".png"),
            target,
            candidate,
            after,
            labels=(
                "native original union",
                "joint sample order",
                "filter then global over",
                "absolute error",
            ),
            scale=12,
        )
    return records


def pose_result(args, manifest, arrays, cases, pose):
    capture, world = (
        clip_from_original(cases[layer, pose]) for layer in ("capture", "original")
    )
    raster, outer_crop = manifest["raster"], np.asarray(manifest["crops_xyxy"][pose])
    region = capture_region(capture, world, raster)
    if np.any(region[:2] < outer_crop[:2]) or np.any(region[2:] > outer_crop[2:]):
        raise ValueError(
            "Complete declared capture footprint leaves native reference crop"
        )
    field = capture_for_pose(arrays, cases, pose)
    full = model_image(field, capture, world, raster, outer_crop, outer_crop)
    selected = model_image(field, capture, world, raster, outer_crop, region)
    for name in ("rgba", "reverse_z", "evaluated"):
        if not np.array_equal(getattr(full, name), getattr(selected, name)):
            raise ValueError("Metadata region changed the complete model image")
    source = arrays["original/" + pose + "/rgba"]
    source_z = arrays["original/" + pose + "/depth"]
    # The finite original opponent must also be clear outside selected metadata.
    outside = np.ones(source_z.shape, bool)
    outside[
        region[1] - outer_crop[1] : region[3] - outer_crop[1],
        region[0] - outer_crop[0] : region[2] - outer_crop[0],
    ] = False
    if np.any(source_z[outside] != 0):
        raise ValueError("Metadata region omits an actual native original hit")
    result = {
        "capture_region_xyxy": region.tolist(),
        "full_crop_xyxy": outer_crop.tolist(),
        "full_crop_work": full.work,
        "metadata_region_work": selected.work,
        "complete_model_outputs_bit_identical": True,
        "source_error": errors(
            reduce_samples(selected.rgba, outer_crop, args.sample_axis),
            reduce_samples(source, outer_crop, args.sample_axis),
        ),
        "composition": composition(
            arrays,
            pose,
            selected,
            outer_crop,
            args.sample_axis,
            args.output,
            args.max_rgb_error,
            args.max_coverage_error,
        ),
    }
    np.savez_compressed(
        args.output / (pose + ".npz"),
        candidate=selected.rgba,
        candidate_depth=selected.reverse_z,
        evaluated=selected.evaluated,
        original=source,
        original_depth=source_z,
    )
    if args.cost:
        from field_view_cost import measured

        costs = {}
        for name, query in (("full-crop", outer_crop), ("metadata-region", region)):
            value, cost = measured(
                lambda query=query: model_image(
                    field, capture, world, raster, outer_crop, query
                ),
                args.output,
                pose + "-" + name,
            )
            np.testing.assert_array_equal(value.rgba, selected.rgba)
            np.testing.assert_array_equal(value.reverse_z, selected.reverse_z)
            costs[name] = cost
        result["costs"] = costs
    return result


def run(args):
    if (
        args.sample_axis not in (1, 2)
        or min(args.max_rgb_error, args.max_coverage_error) < 0
        or not np.isfinite([args.max_rgb_error, args.max_coverage_error]).all()
        or args.max_coverage_error > 1
    ):
        raise ValueError(
            "Explicit bounded reference sample axis and finite image error limits required"
        )
    paths = [
        Path(__file__),
        Path(__file__).with_name("native_world_inputs.py"),
        Path(__file__).with_name("field_depth_reprojection.py"),
        Path(__file__).with_name("compare_field_depth_reprojection.py"),
    ]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, paths),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze executed World comparison sources first")
    manifest, arrays, cases, original = load(args.input)
    capture = clip_from_original(cases["capture", "near-direction"])
    submitted = (
        np.c_[original["positions"], np.ones(len(original["positions"]))] @ capture.T
    )
    ndc = submitted[:, :3] / submitted[:, 3:]
    if (
        not np.isfinite(ndc).all()
        or np.any(submitted[:, 3] <= 0)
        or np.any(abs(ndc[:, :2]) > 1)
        or np.any((ndc[:, 2] < 0) | (ndc[:, 2] > 1))
    ):
        raise ValueError("A whole original leaves the declared capture volume")
    if manifest["raster"] != [1280 * args.sample_axis, 720 * args.sample_axis]:
        raise ValueError("Reference samples must resolve to exactly 1280x720")
    create_output(args.output)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_manifest_sha256": hashlib.sha256(
            (args.input / "manifest.json").read_bytes()
        ).hexdigest(),
        "input_npz_sha256": manifest["npz_sha256"],
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "whole_original_capture_volume_check": {
            "vertices": len(ndc),
            "ndc_minimum": ndc.min(0).tolist(),
            "ndc_maximum": ndc.max(0).tolist(),
            "scope": "Finite metadata/source operand check in real-coefficient binary64 evaluation; no GPU rounding admission or pre-IO verification claim.",
        },
        "host": platform.platform(),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "reference_raster": manifest["raster"],
        "output_raster": [n // args.sample_axis for n in manifest["raster"]],
        "sample_axis": args.sample_axis,
        "finite_error_limits": {
            "max_linear_hdr_rgb_component": args.max_rgb_error,
            "max_visible_source_coverage": args.max_coverage_error,
            "scope": "Declared research comparison limits, not a global product budget or bitwise-reconstruction requirement. Alpha of the opaque final union cannot measure source coverage.",
        },
        "runtime_world_from_field": manifest["runtime_world_from_field"],
        "runtime_world_revision": manifest["runtime_world_revision"],
        "poses": {
            pose: pose_result(args, manifest, arrays, cases, pose) for pose in POSES[:2]
        },
        "scope": "Actual native original/terrain/opening points, declared sample reduction and whole capture-volume region selection. Lens values are native recomputation, not uniform readback or GPU-rounding bounds. Clear model misses do not establish original visibility. Native timing, IO, cache, physical copies/UMA and runtime field admission unqualified.",
    }
    try:
        capture_for_pose(arrays, cases, "revealing-side", "near-direction")
    except ValueError:
        report["changed_pbr_view_rejected_before_reprojection"] = True
    else:
        raise ValueError("Stale capture lighting was accepted")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {pose: value["composition"] for pose, value in report["poses"].items()}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-axis", type=int, default=1)
    parser.add_argument("--max-rgb-error", type=float, default=0.25)
    parser.add_argument("--max-coverage-error", type=float, default=0.125)
    parser.add_argument("--cost", action="store_true")
    run(parser.parse_args())
