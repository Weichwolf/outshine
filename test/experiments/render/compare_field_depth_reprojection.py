"""Different Capture/World raster against the frozen whole native #1716 original.

This is an image-error experiment. Neither cell-model evaluation nor its measured
errors establishes live pose/coverage/light validity or a native terrain union.
"""

import argparse
import gc
import hashlib
import json
import os
import platform
import subprocess
import time
from itertools import product
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from capture_box_reference import submitted_inputs
from experiment_output import create_output
from field_composition import compose_points
from field_depth_reprojection import reproject
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-field-composition"


def viewer_matrix(viewer, basis=None):
    basis = np.asarray(viewer["basis"] if basis is None else basis)
    view = np.eye(4)
    view[:3, :3], view[:3, 3] = basis, -basis @ np.asarray(viewer["eye"])
    focal, width, height = (viewer[k] for k in ("focal_px", "width_px", "height_px"))
    projection = np.array(
        [
            [2 * focal / width, 0, 0, 0],
            [0, 2 * focal / height, 0, 0],
            [0, 0, 0, viewer["near_m"]],
            [0, 0, 1, 0],
        ]
    )
    return projection @ view


def microcamera_basis(viewer, axis):
    right, up, forward = np.asarray(viewer["basis"])
    for y, x in product(range(axis), repeat=2):
        dx, dy = (np.array([x, y]) + 0.5) / axis - 0.5
        f = forward + (right * dx + up * dy) / viewer["focal_px"]
        f /= np.linalg.norm(f)
        r = np.cross(f, up)
        r /= np.linalg.norm(r)
        yield np.asarray([r, np.cross(r, f), f])


def crop(lower, upper, world, raster):
    corners = np.array(list(product(*zip(lower, upper, strict=True))))
    clip = np.c_[corners, np.ones(8)] @ world.T
    if np.any(clip[:, 3] <= 0):
        raise ValueError("Finite whole source in front of viewer required")
    xy = (clip[:, :2] / clip[:, 3:] * [1, -1] + 1) * raster / 2
    low = np.maximum(0, np.floor(xy.min(0) - 2)).astype(int)
    high = np.minimum(raster, np.ceil(xy.max(0) + 2)).astype(int)
    if np.any(high <= low) or np.prod(high - low) > 4096:
        raise ValueError("Finite bounded whole-group diagnostic crop required")
    return low, high


def model_inside_crop(depth, capture, world, raster, low, high):
    """Bound every represented cell corner before allocating World samples."""
    y, x = np.nonzero(depth > 0)
    if not len(x):
        return True
    height, width = depth.shape
    bounds = (
        (2 * int(x.min()) / width - 1, 2 * (int(x.max()) + 1) / width - 1),
        (1 - 2 * (int(y.max()) + 1) / height, 1 - 2 * int(y.min()) / height),
        (float(depth[depth > 0].min()), float(depth.max())),
    )
    corners = np.array(list(product(*bounds)))
    clip = np.c_[corners, np.ones(8)] @ (world @ np.linalg.inv(capture)).T
    if np.any(clip[:, 3] <= 0):
        return False
    xy = (clip[:, :2] / clip[:, 3:] * [1, -1] + 1) * raster / 2
    return bool(np.isfinite(xy).all() and (xy >= low).all() and (xy <= high).all())


def errors(actual, truth):
    delta = abs(actual.astype(float) - truth)
    return {
        "rgb_max": float(delta[:, :, :3].max()),
        "rgb_l1": float(delta[:, :, :3].sum()),
        "coverage_max": float(delta[:, :, 3].max()),
        "coverage_l1": float(delta[:, :, 3].sum()),
        "pixels_different": int(np.count_nonzero(delta.max(2) > 0)),
    }


def nearest_colour(a, az, b, bz):
    first = (az >= bz)[:, :, None]
    return compose_points(np.where(first, a, b), np.where(first, b, a))


def reduce_two(image):
    height, width = image.shape[:2]
    return (
        image[: height // 2 * 2, : width // 2 * 2]
        .reshape(height // 2, 2, width // 2, 2, 4)
        .mean((1, 3), dtype=np.float64)
    )


def external_opponents(arrays, candidate, shape, selection, output):
    """Actual native World point opponents, with a separate 2x2 point filter."""
    source = arrays["foreground_linear"][selection]
    source_z = arrays["foreground_depth"][selection]
    colour, z = candidate.rgba.reshape(shape), candidate.reverse_z.reshape(shape[:2])
    records = {}
    for mode in ("separated", "interleaved"):
        other = arrays[mode + "-background_linear"][selection]
        other_z = arrays[mode + "-background_depth"][selection]
        truth = arrays[mode + "-original_linear"][selection]
        exact = nearest_colour(source, source_z, other, other_z)
        if not np.array_equal(exact, truth):
            raise ValueError(
                "Original World point union does not match native opponent"
            )
        actual = nearest_colour(colour, z, other, other_z)
        a, b = reduce_two(colour), reduce_two(other)
        after = a + (1 - a[:, :, 3:]) * b
        target = reduce_two(truth)
        common = (source_z > 0) & (other_z > 0)
        records[mode] = {
            "native_original_point_union_exact": True,
            "actual_original_source_first": int(
                np.count_nonzero(common & (source_z > other_z))
            ),
            "actual_original_external_first": int(
                np.count_nonzero(common & (source_z < other_z))
            ),
            "reprojected_point_union_error": errors(actual, truth),
            "compose_then_filter_error": errors(reduce_two(actual), target),
            "filter_then_global_over_error": errors(after, target),
            "scope": "Frozen actual native World pixel centres, then a declared 2x2 filter. Not native SS16 sample access, terrain, continuous pixel integration or admission of reprojected coverage/depth.",
        }
        image_panel(
            output / (mode + "-composition.png"),
            target,
            reduce_two(actual),
            after,
            labels=(
                "native originals",
                "compose then filter",
                "filter then over",
                "absolute error",
            ),
        )
    return records


def centre_cost(rgba, depth, flat, capture, world, pixels, raster, output):
    """Held-input point query only; both image approximations remain unqualified."""
    methods = {"depth": depth, "flat": flat}
    records = {name: {"cpu_rounds_s": []} for name in methods}
    expected = {}
    for name, values in methods.items():
        expected[name] = reproject(rgba, values, capture, world, pixels, raster)
    for at in range(7):
        for name in tuple(methods) if at % 2 else tuple(reversed(methods)):
            gc.collect()
            start = time.process_time()
            value = reproject(rgba, methods[name], capture, world, pixels, raster)
            records[name]["cpu_rounds_s"].append(time.process_time() - start)
            np.testing.assert_array_equal(value.rgba, expected[name].rgba)
            np.testing.assert_array_equal(value.reverse_z, expected[name].reverse_z)
    for name, values in methods.items():
        gc.collect()
        value, memory = allocations(
            output / (name + "-centre.memray"),
            lambda values=values: reproject(
                rgba, values, capture, world, pixels, raster
            ),
        )
        np.testing.assert_array_equal(value.rgba, expected[name].rgba)
        np.testing.assert_array_equal(value.reverse_z, expected[name].reverse_z)
        records[name].update(memory)
        records[name].update(
            cpu_median_s=float(np.median(records[name]["cpu_rounds_s"])),
            work=value.work,
            result_payload_bytes=sum(
                a.nbytes for a in (value.rgba, value.reverse_z, value.evaluated)
            ),
        )
    return records


def image_panel(
    output,
    truth,
    actual,
    flat,
    *,
    labels=("native original", "raw-depth cells", "flat cells", "absolute error"),
):
    width, height = truth.shape[1], truth.shape[0]
    images = (truth, actual, flat, abs(actual - truth))
    panel = Image.new("RGB", (4 * width * 16, height * 16 + 32), "#222222")
    draw = ImageDraw.Draw(panel)
    for at, (label, values) in enumerate(zip(labels, images, strict=True)):
        rgb = values[:, :, :3].astype(float)
        rgb = np.power(np.maximum(rgb, 0) / (1 + np.maximum(rgb, 0)), 1 / 2.2)
        tile = Image.fromarray(np.round(rgb * 255).astype(np.uint8)).resize(
            (width * 16, height * 16), Image.Resampling.NEAREST
        )
        panel.paste(tile, (at * width * 16, 32))
        draw.text((at * width * 16 + 2, 8), label, fill="white")
    panel.save(output)


def run(args):
    paths = [
        Path(__file__),
        Path(__file__).with_name("field_depth_reprojection.py"),
        Path(__file__).with_name("depth_reprojection.py"),
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
        raise ValueError(
            "Freeze executed reprojection sources before recording evidence"
        )
    create_output(args.output)
    manifest = json.loads((DATA / "manifest.json").read_text())
    path = DATA / "captures.npz"
    if hashlib.sha256(path.read_bytes()).hexdigest() != manifest["npz_sha256"]:
        raise ValueError("Frozen native whole-source fixture changed")
    with np.load(path, allow_pickle=False) as archive:
        arrays = {
            key: archive[key]
            for key in (
                "field_capture_colour",
                "field_capture_depth",
                "foreground_linear",
                "foreground_depth",
                "foreground_ss16",
                "source_least_m",
                "source_most_m",
                *(
                    mode + suffix
                    for mode in ("separated", "interleaved")
                    for suffix in (
                        "-background_linear",
                        "-background_depth",
                        "-original_linear",
                    )
                ),
            )
        }
    operands_manifest, _arrays, (model, view, shift) = submitted_inputs()
    if operands_manifest["source"] != manifest["source"]:
        raise ValueError(
            "Actual packed capture operands have different original provenance"
        )
    model = model.copy()
    model[:3, 3] = np.add(model[:3, 3], shift, dtype=np.float32)
    capture = view.astype(float) @ model.astype(float)
    viewer = manifest["viewer"]
    raster = (viewer["width_px"], viewer["height_px"])
    world = viewer_matrix(viewer)
    low, high = crop(
        arrays["source_least_m"].min(0), arrays["source_most_m"].max(0), world, raster
    )
    if not model_inside_crop(
        arrays["field_capture_depth"], capture, world, raster, low, high
    ):
        raise ValueError(
            "Complete reprojected cell hull leaves the original diagnostic crop"
        )
    y, x = np.mgrid[low[1] : high[1], low[0] : high[0]]
    pixels = np.c_[x.ravel(), y.ravel()] + 0.5
    shape = (*x.shape, 4)
    rgba, depth = arrays["field_capture_colour"], arrays["field_capture_depth"]
    flat = np.where(
        depth > 0, (float(depth[depth > 0].min()) + float(depth.max())) / 2, 0
    ).astype(depth.dtype)
    records, images, centres = {}, {}, {}
    for name, depths in (("depth", depth), ("flat", flat)):
        centre = reproject(rgba, depths, capture, world, pixels, raster)
        if not centre.evaluated.all():
            raise ValueError(
                "Unresolved centre samples require originals, not clear pixels"
            )
        centres[name] = centre
        total = np.zeros((len(pixels), 4))
        steps, elapsed = 0, time.process_time()
        for basis in microcamera_basis(viewer, 4):
            sample = reproject(
                rgba, depths, capture, viewer_matrix(viewer, basis), pixels, raster
            )
            if not sample.evaluated.all():
                raise ValueError("Unresolved microcamera samples require originals")
            total += sample.rgba
            steps += sample.work["visited_cells"]
        elapsed = time.process_time() - elapsed
        image = (total / 16).reshape(shape)
        images[name] = image
        selection = np.s_[low[1] : high[1], low[0] : high[0]]
        records[name] = {
            "centre_error": errors(
                centre.rgba.reshape(shape), arrays["foreground_linear"][selection]
            ),
            "microcamera_error": errors(image, arrays["foreground_ss16"][selection]),
            "evaluated_samples": 16 * len(pixels),
            "visited_cells": steps,
            "one_observed_cpu_s": elapsed,
        }
    truth = arrays["foreground_ss16"][selection]
    image_panel(args.output / "comparison.png", truth, images["depth"], images["flat"])
    np.savez_compressed(args.output / "comparison.npz", original=truth, **images)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "fixture_sha256": manifest["npz_sha256"],
        "capture_operand_manifest_sha256": hashlib.sha256(
            (
                ROOT
                / "test/outshine/data/render/building-capture-operands/manifest.json"
            ).read_bytes()
        ).hexdigest(),
        "capture_from_field": capture.tolist(),
        "world_from_field": world.tolist(),
        "crop_xyxy": [*low.tolist(), *high.tolist()],
        "raster": raster,
        "raw_capture_bytes": rgba.nbytes + depth.nbytes,
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in paths
        },
        "host": platform.platform(),
        "processor": platform.processor(),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "source_provenance": manifest["source"],
        "methods": records,
        "external_composition": external_opponents(
            arrays, centres["depth"], shape, selection, args.output
        ),
        "scope": "Frozen native different-raster original opponent. Actual packed capture coefficients interpreted in real arithmetic; World matrix from finite manifest camera, not submitted GPU operands. Discrete constant-depth cell representation only. Native microcamera rotations reproduced geometrically; held C/T does not resample observer-dependent radiance. No live pose/lighting/error admission, continuous coverage, native terrain union, IO/cache or device-cost proof.",
    }
    if args.cost:
        report["centre_cost"] = centre_cost(
            rgba, depth, flat, capture, world, pixels, raster, args.output
        )
        report["cost_scope"] = (
            "Seven normal-priority held-input rounds and separate native Memray traces, World centre samples only. Field validation/transfer inversion included, imports/held native input loading/image checks excluded. Result payload is owned output, not physical traffic; library temporaries remain in Memray totals. Neither approximation meets a declared image budget here, so no equal-quality winner, cache/native/UMA/frame inference."
        )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(records))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cost", action="store_true")
    run(parser.parse_args())
