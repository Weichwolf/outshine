"""Consume frozen native background opponents without identifying two cameras."""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from field_background_inputs import ATTRIBUTES, DATA, original_inputs, source_bounds
from field_composition import (
    OriginalBounds,
    compose_points,
    depth_order,
    forward_interval,
)
from field_view_sampling import ImageView
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
INPUTS = ROOT / "test/outshine/data/render/building-field-composition"
MODES = ("separated", "interleaved")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_inputs(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    packed = directory / "captures.npz"
    if (
        manifest["version"] != 1
        or packed.stat().st_size != manifest["npz_bytes"]
        or digest(packed) != manifest["npz_sha256"]
    ):
        raise ValueError("Frozen native capture size/checksum mismatch")
    for name, expected in manifest["input_files"].items():
        if digest(DATA / name) != expected:
            raise ValueError("Original source input differs from native capture")
    with np.load(packed, allow_pickle=False) as data:
        arrays = {name: data[name] for name in data.files}
    for channel in ("colour", "depth"):
        if (
            hashlib.sha256(arrays["field_capture_" + channel].tobytes()).hexdigest()
            != manifest["field_capture"]["raw_" + channel + "_sha256"]
        ):
            raise ValueError(
                "Raw readback does not match its bound native field capture"
            )
    if sum(a.nbytes for a in arrays.values()) != manifest["native_array_bytes"] or any(
        not np.isfinite(a).all() for a in arrays.values()
    ):
        raise ValueError("Incomplete/nonfinite frozen native arrays")
    parts, provenance, _ = original_inputs()
    if provenance != manifest["source"]:
        raise ValueError("Original source provenance differs from native capture")
    for i, part in enumerate(parts):
        for key in (*ATTRIBUTES, "binding"):
            if (
                hashlib.sha256(part[key].tobytes()).hexdigest()
                != manifest["geometry_arrays_sha256"][f"part{i}_{key}"]
            ):
                raise ValueError("Native original geometry/lineage hash mismatch")
    for name, actual in source_bounds(parts).items():
        if not np.array_equal(arrays[name], actual):
            raise ValueError("Complete source hull differs from original primitives")
    if (
        len(arrays["source_ids"]) != manifest["geometry_sources"]
        or sum(len(p["indices"]) for p in parts) != manifest["geometry_triangles"]
    ):
        raise ValueError("Native whole-source accounting differs")
    return manifest, arrays, parts


def rgba_error(original, candidate):
    difference = abs(original.astype(float) - candidate)
    return {
        "rgb_l1": float(difference[:, :, :3].sum()),
        "rgb_max_abs": float(difference[:, :, :3].max(initial=0)),
        "coverage_l1": float(difference[:, :, 3].sum()),
        "coverage_max_abs": float(difference[:, :, 3].max(initial=0)),
    }


def original_bound(parts, translation, binding):
    moved = np.concatenate(
        [(p["positions"].astype(float) + translation).astype(np.float32) for p in parts]
    )
    return OriginalBounds(tuple(moved.min(0)), tuple(moved.max(0)), binding)


def pixel_patch(arrays):
    mask = np.zeros(arrays["foreground_depth"].shape, bool)
    for name in (
        "foreground",
        "separated-background",
        "separated-original",
        "interleaved-background",
        "interleaved-original",
    ):
        mask |= arrays[name + "_ss16"][:, :, 3] > 0
        mask |= arrays[name + "_linear"][:, :, 3] > 0
    y, x = np.nonzero(mask)
    if not len(x):
        raise ValueError("A positive native original opponent is required")
    return slice(max(0, y.min() - 1), min(mask.shape[0], y.max() + 2)), slice(
        max(0, x.min() - 1), min(mask.shape[1], x.max() + 2)
    )


def native_case(mode, manifest, arrays, parts, patch):
    view = manifest["viewer"]
    camera = ImageView(
        tuple(view["eye"]),
        tuple(map(tuple, view["basis"])),
        view["focal_px"],
        view["width_px"],
        view["height_px"],
    )
    foreground = arrays["foreground_linear"][patch]
    background = arrays[mode + "-background_linear"][patch]
    truth = arrays[mode + "-original_linear"][patch]
    a, b = arrays["foreground_depth"][patch], arrays[mode + "-background_depth"][patch]
    common = (a > 0) & (b > 0)
    nearest = np.where((a >= b)[:, :, None], foreground, background)
    if not np.array_equal(nearest, truth) or not np.array_equal(
        np.maximum(a, b), arrays[mode + "-original_depth"][patch]
    ):
        raise ValueError("Native original union differs from nearest source hits")
    translations = manifest["controlled_translations_m"]
    front = original_bound(
        parts, translations["foreground"][0], "foreground-instance-0"
    )
    back = original_bound(
        parts, translations[mode + "-background"][0], "neighbour-instance-1"
    )
    order = depth_order(front, back, camera.eye, camera.basis[2])
    ab, ba = (
        compose_points(foreground, background),
        compose_points(background, foreground),
    )
    if mode == "separated" and (
        order[0] != "front_before_back" or not np.array_equal(ab, truth)
    ):
        raise ValueError("Ordered native original control failed")
    if mode == "interleaved" and (
        order[0] != "originals"
        or not (np.any(common & (a > b)) and np.any(common & (a < b)))
    ):
        raise ValueError("Native interleave must contain both depth orders")
    af, bf = arrays["foreground_ss16"][patch], arrays[mode + "-background_ss16"][patch]
    separate = af + (1 - af[:, :, 3:4]) * bf
    averaged = arrays[mode + "-original_ss16"][patch]
    try:
        compose_points(af, bf)
    except ValueError:
        pass
    else:
        raise ValueError(
            "Native averaged coverage was falsely accepted as point samples"
        )
    return {
        "mode": mode,
        "source_instances": 2,
        "supported_originals_per_instance": len(arrays["source_ids"]),
        "unresolved_per_instance": manifest["source"]["unresolved_originals"],
        "whole_hull_order": order,
        "whole_hull_forward_intervals_m": [
            forward_interval(hull, camera.eye, camera.basis[2])
            for hull in (front, back)
        ],
        "common_centres": int(common.sum()),
        "foreground_first": int((common & (a > b)).sum()),
        "background_first": int((common & (a < b)).sum()),
        "joint_point_a_over_b": rgba_error(truth, ab),
        "joint_point_b_over_a": rgba_error(truth, ba),
        "independently_filtered_ss16": rgba_error(averaged, separate),
        "averaged_coverage_rejected_as_point_samples": True,
    }, (averaged, separate, truth, ab)


def capture_control(manifest, arrays):
    camera = manifest["capture_camera"]
    shape = camera["height_px"], camera["width_px"]
    original, field = arrays["original_capture_colour"], arrays["field_capture_colour"]
    if original.shape != (*shape, 4) or field.shape != original.shape:
        raise ValueError("Capture camera and Raw C/T dimensions differ")
    a, b = arrays["original_capture_depth"], arrays["field_capture_depth"]
    if (
        a.shape != shape
        or b.shape != shape
        or np.any((a < 0) | (a > 1) | (b < 0) | (b > 1))
    ):
        raise ValueError("Capture depth and declared reverse-Z camera differ")
    common = (a > 0) & (b > 0)
    difference = abs(a.astype(float) - b) * (camera["far_m"] - camera["near_m"])
    viewer = manifest["viewer"]
    differences = [
        name
        for name, different in (
            ("projection", camera["orthographic"] != viewer["orthographic"]),
            ("raster", shape != (viewer["height_px"], viewer["width_px"])),
            ("eye", camera["eye_m"] != viewer["eye"]),
            (
                "basis",
                [camera[k] for k in ("right", "up", "forward")] != viewer["basis"],
            ),
        )
        if different
    ]
    return {
        "capture_colour": rgba_error(original, field),
        "coverage_mask_disagreements": int(
            ((original[:, :, 3] > 0) != (field[:, :, 3] > 0)).sum()
        ),
        "common_hit_forward_depth_max_abs_m": float(difference[common].max(initial=0)),
        "raw_and_viewer_samples_registered": False if differences else None,
        "known_ray_grid_differences": differences,
        "direct_raw_plus_viewer_background_admission": "originals",
        "reason": "different_projection_and_sampling; no joint subpixel observations",
    }


def image_panel(images, labels, path):
    canvas = Image.new("RGB", (1280, 350), "#eeeeee")
    draw = ImageDraw.Draw(canvas)
    for i, (rgba, label) in enumerate(zip(images, labels, strict=True)):
        rgb = rgba[:, :, :3].astype(float) + (1 - rgba[:, :, 3:4]) * [0.08, 0.12, 0.18]
        mapped = np.round(
            (np.maximum(rgb, 0) / (1 + np.maximum(rgb, 0))) ** (1 / 2.2) * 255
        ).astype(np.uint8)
        patch = Image.fromarray(mapped).resize((300, 280), Image.Resampling.NEAREST)
        canvas.paste(patch, (320 * i, 40))
        draw.text((320 * i + 3, 10), label, fill="black")
    draw.text(
        (4, 327),
        "A18 native HDR; shared L/(1+L) diagnostic mapping; enlarged pixel patches; distinct point / SS16 observations",
        fill="black",
    )
    canvas.save(path)


def check(directory, output):
    if not output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.resolve().is_relative_to(ROOT):
        raise ValueError("Use a fresh explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=False)
    manifest, arrays, parts = read_inputs(directory)
    patch = pixel_patch(arrays)
    rows = []
    for mode in MODES:
        row, images = native_case(mode, manifest, arrays, parts, patch)
        rows.append(row)
        image_panel(
            images,
            (
                "native joint SS16",
                "separate SS16 then over",
                "native joint pixel centres",
                "point A over B",
            ),
            output / (mode + ".png"),
        )
    capture = capture_control(manifest, arrays)
    image_panel(
        (arrays["original_capture_colour"], arrays["field_capture_colour"]),
        ("native original capture", "held field capture"),
        output / "raw-capture.png",
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_manifest_sha256": digest(directory / "manifest.json"),
        "native_npz_sha256": manifest["npz_sha256"],
        "native_producer_commit": manifest["native_commit"],
        "native_device": manifest["field_capture"]["device"],
        "geometry_arrays_verified": len(manifest["geometry_arrays_sha256"]),
        "source_bounds_verified": len(arrays["source_ids"]),
        "patch": [[int(p.start), int(p.stop)] for p in patch],
        "rows": rows,
        "capture_control": capture,
        "scope": "Frozen native original controls, not new native execution. Controlled placements, two unsupported originals per instance and cell33 opacity assumption retained. Point ordering/correlation and distinct cameras verified; SS16 is finite, not a continuous reference. Raw-plus-background field admission, whole World, devices/p99/UMA remain unqualified.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=INPUTS)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check(args.data, args.output)
