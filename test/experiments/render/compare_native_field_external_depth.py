"""Frozen native original/sample values against registered analytic outside solids.

This consumes an existing archived capture, not a fresh device execution or the
current MaterialFieldPass. Pointwise measured intervals apply only to these
exact paired points. They are not a producer's general geometry-error bound.
"""

import argparse
import hashlib
import json
import subprocess
from dataclasses import replace
from pathlib import Path

import numpy as np
from building_sample_inputs import digest, readbacks
from building_sample_originals import read_original
from compare_field_external_depth import DATA, compose, panel, reduced
from field_external_depth import PointSamples, SampleBinding, over_constant_background
from field_external_opponents import (
    canonical,
    foreign_ring,
    join,
    point_image,
    quad,
    terrain,
)


def load(directory):
    original, provenance = read_original(directory / "original-geometry")
    manifest, arrays = readbacks(directory / "same-rays")
    source = json.loads((directory / "original-geometry/manifest.json").read_text())
    if manifest["original_geometry_files"] != {
        k: v["sha256"] for k, v in source["files"].items()
    }:
        raise ValueError("The exact whole native original group is required")
    query = manifest["cases"][0]["query"]
    capture = query["capture"]
    axes = np.asarray([capture[k] for k in ("right", "up", "forward")])
    if (
        capture["format"] != "reverse_z_0_1_float32"
        or not capture["orthographic"]
        or [capture["width_px"], capture["height_px"]] != manifest["raster"]
        or not np.isfinite(axes).all()
        or not np.allclose(axes @ axes.T, np.eye(3), rtol=0, atol=1e-12)
        or not 0 < capture["near_m"] < capture["far_m"] < np.inf
    ):
        raise ValueError("Exact finite native orthographic rays and clips required")
    x0, y0, x1, y1 = manifest["crop"]
    y, x = np.mgrid[y0:y1, x0:x1]
    offset = (
        ((x + 0.5) / capture["width_px"] * 2 - 1)[..., None]
        * capture["x_mag_m"]
        * axes[0]
    )
    offset += (
        (1 - (y + 0.5) / capture["height_px"] * 2)[..., None]
        * capture["y_mag_m"]
        * axes[1]
    )
    origins = (capture["eye_m"] + offset).reshape(-1, 3)
    rays = origins, np.broadcast_to(axes[2], origins.shape)
    binding = SampleBinding(
        "exact-native-source-frame:"
        + digest(directory / "original-geometry/manifest.json"),
        canonical(
            {
                "capture": capture,
                "crop": manifest["crop"],
                "samples": "native pixel centres",
            }
        ),
        None,
        ("whole-source-atlas:" + manifest["atlas_sha256"],),
        manifest["native_driver_sha256"],
        canonical(
            dict(manifest["lighting"], sun_elevation_deg=query["sun_elevation_deg"])
        ),
        canonical({"profile": "archived capture observer", "capture": capture}),
        fixture_revision=manifest["npz_sha256"],
    )
    rgba, z = arrays["case0_samples_linear"], arrays["case0_samples_depth"][:, :, 0]
    truth_rgba, truth_z = (
        arrays["case0_original_linear"],
        arrays["case0_original_depth"][:, :, 0],
    )
    known = (z > 0) == (truth_z > 0)
    error = np.where(z > 0, np.nextafter(abs(z.astype(float) - truth_z), np.inf), 0)
    saved = PointSamples(binding, rgba, z, error, known)
    saved = replace(
        saved,
        binding=replace(
            binding,
            sample_revision=(
                hashlib.sha256(rgba).hexdigest(),
                hashlib.sha256(z).hexdigest(),
            ),
        ),
    )
    return original, provenance, manifest, saved, (truth_rgba, truth_z), capture, rays


def compare(name, external, saved, reference, capture, rays, output):
    shape = saved.reverse_z.shape
    rgba, z, error = point_image(external, rays, capture, shape)
    # A declared emitting reference surface supplies constant scene-linear C/T.
    # It shares the scene but makes no claim about native terrain PBR/shadows.
    rgba[z > 0, :3] = [0.2, 0.3, 0.1]
    outside = PointSamples(
        replace(saved.binding, sources=(name,), materials="fixed-radiance-reference"),
        rgba,
        z,
        error,
        np.ones(shape, bool),
    )
    result = compose(saved, outside)
    if result.rgba is None:
        raise ValueError(
            "Whole archived point control retained parent: " + result.reason
        )
    original_rgba, original_z = reference
    original_first = original_z > z
    truth = np.where(original_first[:, :, None], original_rgba, rgba)
    np.testing.assert_array_equal(
        result.rgba, np.where(original_first[:, :, None], saved.rgba, rgba)
    )
    expected_field_depth = np.where(original_first, saved.reverse_z, z)
    np.testing.assert_array_equal(result.reverse_z, expected_field_depth)
    np.testing.assert_array_equal(result.rgba[:, :, 3], truth[:, :, 3])
    source_error = float(
        abs(saved.rgba[:, :, :3].astype(float) - original_rgba[:, :, :3]).max()
    )
    point_error = float(
        abs(result.rgba[:, :, :3].astype(float) - truth[:, :, :3]).max()
    )
    actual, target = reduced(result.rgba), reduced(truth)
    source_l1 = float(
        abs(saved.rgba[:, :, :3].astype(float) - original_rgba[:, :, :3]).sum()
    )
    pixel_l1 = float(abs(actual[:, :, :3] - target[:, :, :3]).sum())
    if (
        point_error > source_error
        or abs(actual[:, :, :3] - target[:, :, :3]).max() > source_error
        or pixel_l1 > source_l1 / 16 + 1e-10
    ):
        raise ValueError(
            "Composition/filter amplified the finite original-point RGB error"
        )
    front_mean = reduced(saved.rgba)
    after = over_constant_background(front_mean, rgba[0, 0].astype(float))
    common = (original_z > 0) & (z > 0)
    outside_first = int(np.count_nonzero(common & ~original_first))
    if outside_first == 0:
        np.testing.assert_array_equal(after, actual)
    np.savez_compressed(
        output / (name + ".npz"),
        original=truth,
        candidate=result.rgba,
        outside_rgba=rgba,
        outside_depth=z,
        finite_source_error=saved.error,
    )
    panel(
        output / (name + ".png"),
        target,
        actual,
        after,
        labels=[
            "Native originals + outside",
            "Saved native field + outside",
            "After filter without depth proof",
            "Wrong-order error, x4",
        ],
    )
    return {
        "case": name,
        "common_source_first": int(np.count_nonzero(common & original_first)),
        "common_outside_first": outside_first,
        "original_ownership_exact": True,
        "coverage_exact": True,
        "finite_source_rgb_max_error": source_error,
        "point_rgb_max_error": point_error,
        "source_rgb_l1": source_l1,
        "filtered_rgb_l1": pixel_l1,
        "filtered_source_rgb_l1_upper_bound": source_l1 / 16,
        "filtered_rgb_max_error": float(abs(actual[:, :, :3] - target[:, :, :3]).max()),
        "constant_after_filter_eligible": outside_first == 0,
        "after_filter_without_order_rgb_max_error": float(
            abs(after[:, :, :3] - target[:, :, :3]).max()
        ),
        "arrays_sha256": digest(output / (name + ".npz")),
    }


def thin_occluder(saved, reference, capture, rays, output):
    original_rgba, original_z = reference
    index = np.unravel_index(np.argmax(saved.error), saved.error.shape)
    flat = np.ravel_multi_index(index, saved.error.shape)
    midpoint = (float(saved.reverse_z[index]) + float(original_z[index])) / 2
    depth_m = capture["far_m"] - midpoint * (capture["far_m"] - capture["near_m"])
    centre = rays[0][flat] + depth_m * np.asarray(capture["forward"])
    right = np.asarray(capture["right"]) * capture["x_mag_m"] / capture["width_px"] / 2
    up = np.asarray(capture["up"]) * capture["y_mag_m"] / capture["height_px"] / 2
    geometry = quad(
        [
            centre - right - up,
            centre + right - up,
            centre + right + up,
            centre - right + up,
        ],
        capture["forward"],
        [0.2, 0.3, 0.1],
    )
    rgba, z, error = point_image(geometry, rays, capture, saved.reverse_z.shape)
    rgba[z > 0, :3] = [0.2, 0.3, 0.1]
    if np.count_nonzero(z) != 1 or not z[index] > 0:
        raise ValueError(
            "Thin geometric control must cover exactly the selected native point"
        )
    outside = PointSamples(
        replace(
            saved.binding,
            sources=("thin-occluder",),
            materials="fixed-radiance-reference",
        ),
        rgba,
        z,
        error,
        np.ones(z.shape, bool),
    )
    safe = compose(saved, outside)
    if safe.rgba is not None or safe.reason != "ambiguous_depth_retains_parent":
        raise ValueError("Actual source-depth uncertainty failed to retain parent")
    unsafe = compose(replace(saved, error=np.zeros(saved.error.shape)), outside)
    truth_first = original_z > z
    expected = np.where(truth_first, saved.reverse_z, z)
    if unsafe.rgba is None:
        raise ValueError("Unbounded-depth counterfactual must actually choose")
    mismatches = int(np.count_nonzero(unsafe.reverse_z != expected))
    if mismatches != 1:
        raise ValueError(
            "Actual native interval counterexample must misorder one point"
        )
    truth = np.where(truth_first[:, :, None], original_rgba, rgba)
    y, x = index
    patch = (
        slice(max(0, y - 3), min(z.shape[0], y + 4)),
        slice(max(0, x - 3), min(z.shape[1], x + 4)),
    )
    panel(
        output / "native-thin-occluder.png",
        truth[patch],
        truth[patch],
        unsafe.rgba[patch],
        labels=[
            "Native original + thin surface",
            "Safe: original parent retained",
            "Ignoring source depth error",
            "Wrong-visibility error, x4",
        ],
    )
    path = output / "native-thin-occluder.npz"
    np.savez_compressed(
        path,
        vertices=geometry["positions"],
        indices=geometry["indices"],
        outside_rgba=rgba,
        outside_depth=z,
        outside_error=error,
        native_point=index,
    )
    return {
        "native_point_yx": list(map(int, index)),
        "native_original_reverse_z": float(original_z[index]),
        "saved_sample_reverse_z": float(saved.reverse_z[index]),
        "occluder_reverse_z": float(z[index]),
        "measured_point_interval": float(saved.error[index]),
        "safe_result": safe.reason,
        "unsafe_ignored_interval_wrong_owners": mismatches,
        "arrays_sha256": digest(path),
    }


def run(args):
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh explicit system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    original, provenance, manifest, saved, reference, capture, rays = load(args.inputs)
    base = terrain(original, capture)
    crop_width = saved.rgba.shape[1]
    ring = foreign_ring(
        capture,
        saved.reverse_z,
        extent_m=capture["x_mag_m"] * crop_width / capture["width_px"],
    )
    cases = [
        compare("native-terrain", base, saved, reference, capture, rays, args.output),
        compare(
            "native-terrain-and-opening",
            join(base, ring),
            saved,
            reference,
            capture,
            rays,
            args.output,
        ),
    ]
    if not all(
        cases[1][key] > 0 for key in ("common_source_first", "common_outside_first")
    ):
        raise ValueError("A real native-point interleave is required")
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "originals": provenance,
        "native_commit": manifest["native_commit"],
        "native_binary_sha256": manifest["native_binary_sha256"],
        "native_driver_sha256": manifest["native_driver_sha256"],
        "manifest_sha256": digest(args.inputs / "same-rays/manifest.json"),
        "capture": capture,
        "binding": saved.binding.__dict__,
        "cases": cases,
        "thin_occluder": thin_occluder(saved, reference, capture, rays, args.output),
        "whole_source_mask_mismatches": int(np.count_nonzero(~saved.known)),
        "paired_points": int(np.prod(saved.known.shape)),
        "maximum_measured_point_depth_interval": float(saved.error.max()),
        "scope": "Existing archived A18 native originals versus stored-sample draws, exact same rays and whole geometry hashes. Outside is analytic fixed-radiance opaque geometry; reference uses native original depths/colours independently of saved sample values. Pointwise error intervals enclose only these frozen original values, not other rays, a continuous region or current native producer. No new GPU execution/current MaterialFieldPass/World/p99 admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(cases))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
