"""Finite registered-point opponents; current native PBR/World admission stays separate."""

import argparse
import hashlib
import json
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from building_sample_inputs import digest
from experiment_output import create_output
from field_external_depth import (
    PointSamples,
    SampleBinding,
    compose_registered,
    over_constant_background,
)
from field_external_opponents import (
    canonical,
    foreign_ring,
    join,
    load,
    point_image,
    terrain,
)
from field_view_integrals import box_integrals, summed_field
from PIL import Image, ImageDraw

DATA = (
    Path(__file__).resolve().parents[2] / "outshine/data/render/building-depth-samples"
)


def bound(values, binding):
    revision = tuple(
        hashlib.sha256(np.ascontiguousarray(value)).hexdigest() for value in values[:2]
    )
    return PointSamples(
        replace(binding, sample_revision=revision),
        *values,
        np.ones(values[1].shape, bool),
    )


def compose(a, b):
    return compose_registered(
        a, b, wanted_a=a.binding, wanted_b=b.binding, diagnostic_fixture=True
    )


def reduced(image):
    h, w = image.shape[:2]
    return image.reshape(h // 4, 4, w // 4, 4, 4).mean((1, 3), dtype=np.float64)


def prefix_reduced(image):
    h, w = image.shape[:2]
    y, x = np.mgrid[0:h:4, 0:w:4]
    lo = np.column_stack((x.ravel(), y.ravel())).astype(float)
    field = summed_field(image, dtype=np.float64)
    return (box_integrals(field, lo, lo + 4) / 16).reshape(h // 4, w // 4, 4)


def fallback_controls(a, native, capture):
    binding = a.binding
    changed_capture = dict(
        capture, eye_m=[capture["eye_m"][0] + 1, *capture["eye_m"][1:]]
    )
    changes = {
        "capture": canonical(changed_capture),
        "lighting": "changed-sun-direction",
        "pbr_view": "changed-BRDF-eye",
        "world_revision": "changed-world",
        "sources": ("missing-whole-source",),
    }
    results = {"unknown_native_coverage_or_depth": compose(native, a)}
    for key, value in changes.items():
        wanted = replace(binding, **{key: value})
        results[key] = compose_registered(
            a, a, wanted_a=wanted, wanted_b=wanted, diagnostic_fixture=True
        )
    invalid = a.rgba.copy()
    invalid[0, 0, 0] = np.nan
    results["invalid_sample_value"] = compose(replace(a, rgba=invalid), a)
    results["equal_depth"] = compose(a, a)
    if any(result.rgba is not None or result.bindings for result in results.values()):
        raise ValueError("Invalid/stale/unknown points silently replaced parent")
    return {key: result.reason for key, result in results.items()}


def panel(path, truth, actual, wrong, *, labels=None):
    images = [truth, actual, wrong, abs(wrong - truth)]
    canvas = Image.new("RGB", (1024, 280), "#171717")
    labels = labels or [
        "Whole original union",
        "Registered points, then filter",
        "Separate filtering / global order",
        "Absolute error, x4",
    ]
    draw = ImageDraw.Draw(canvas)
    for i, (value, label) in enumerate(zip(images, labels, strict=True)):
        if i == 3:
            rgb = np.clip(value[:, :, :3] * 4, 0, 1)
        else:
            rgb = value[:, :, :3] + (1 - value[:, :, 3:]) * [0.1, 0.15, 0.2]
            rgb = np.power(rgb / (1 + rgb), 1 / 2.2)
        image = Image.fromarray(np.uint8(np.clip(rgb, 0, 1) * 255))
        canvas.paste(image.resize((256, 256), Image.Resampling.NEAREST), (256 * i, 24))
        draw.text((256 * i + 3, 6), label, fill="white")
    canvas.save(path)


def compare_case(
    name, original, external, source, binding, rays, capture, shape, output
):
    start = time.process_time(), time.perf_counter()
    b = bound(
        point_image(external, rays, capture, shape), replace(binding, sources=(name,))
    )
    direct = point_image(join(original, external), rays, capture, shape)
    merged = compose(source, b)
    if merged.rgba is None:
        raise ValueError(
            "Finite separated point-depth opponent unexpectedly refused: "
            + merged.reason
        )
    if not np.array_equal(merged.rgba, direct[0]) or not np.array_equal(
        merged.reverse_z, direct[1]
    ):
        raise ValueError(
            "Point composition differs from independently traced whole union"
        )
    truth, actual = reduced(direct[0]), prefix_reduced(merged.rgba)
    if not np.allclose(truth, actual, rtol=0, atol=1e-10):
        raise ValueError("Joint prefix loses finite-sample energy/coverage")
    af, bf = reduced(source.rgba), reduced(b.rgba)
    wrong = af + (1 - af[:, :, 3:]) * bf
    common = (source.reverse_z > 0) & (b.reverse_z > 0)
    constant_control = None
    if np.all(b.rgba == b.rgba[0, 0]) and not np.any(
        common & (source.reverse_z <= b.reverse_z)
    ):
        constant = b.rgba[0, 0].astype(np.float64)
        after = over_constant_background(af, constant)
        delta = float(abs(truth - after).max())
        if delta > 1e-10:
            raise ValueError("Ordered constant-background postfilter control failed")
        constant_control = {
            "rgba": constant.tolist(),
            "all_registered_background_points_exactly_constant": True,
            "source_before_background_at_every_common_point": True,
            "maximum_rgba_error": delta,
            "scope": "Only these finite normalized footprints; not a certificate of native/background constancy between samples or arbitrary view validity.",
        }
    np.savez_compressed(
        output / (name + ".npz"),
        a_rgba=source.rgba,
        a_depth=source.reverse_z,
        a_error=source.error,
        b_rgba=b.rgba,
        b_depth=b.reverse_z,
        b_error=b.error,
        original_rgba=direct[0],
        original_depth=direct[1],
    )
    panel(output / (name + ".png"), truth, actual, wrong)
    return {
        "name": name,
        "arrays_sha256": digest(output / (name + ".npz")),
        "external_triangles": len(external["indices"]),
        "point_pairs": int(np.prod(shape)),
        "whole_union_point_rgba_and_depth_exact": True,
        "prefix_rgba_max_abs": float(abs(truth - actual).max()),
        "constant_background_after_filter": constant_control,
        "source_first_common_points": int(
            np.count_nonzero(common & (source.reverse_z > b.reverse_z))
        ),
        "external_first_common_points": int(
            np.count_nonzero(common & (source.reverse_z < b.reverse_z))
        ),
        "separate_filter_rgb_max_abs": float(
            abs(truth[:, :, :3] - wrong[:, :, :3]).max()
        ),
        "separate_filter_coverage_max_abs": float(
            abs(truth[:, :, 3] - wrong[:, :, 3]).max()
        ),
        "preparation_and_independent_trace_cpu_s": time.process_time() - start[0],
        "preparation_and_independent_trace_wall_s": time.perf_counter() - start[1],
    }


def run(args):
    create_output(args.output)
    original, provenance, raw, capture, rays, shape = load(args.inputs)
    binding = SampleBinding(
        "unchanged-native-source-frame",
        canonical(provenance["capture_subset"]),
        None,
        (
            "whole-native-group:"
            + digest(args.inputs / "original-geometry/geometry.npz"),
        ),
        "explicit-uniform-HDR-Lambert-control",
        "directional-0.3-0.8-0.5-plus-ambient",
        canonical(capture["eye_m"]),
        fixture_revision=digest(args.inputs / "original-geometry/manifest.json"),
    )
    a = bound(point_image(original, rays, capture, shape), binding)
    ground = terrain(original, capture)
    cases = [
        compare_case(
            "terrain", original, ground, a, binding, rays, capture, shape, args.output
        )
    ]
    cases.append(
        compare_case(
            "terrain-and-foreign-opening",
            original,
            join(ground, foreign_ring(capture, a.reverse_z)),
            a,
            binding,
            rays,
            capture,
            shape,
            args.output,
        )
    )
    if not all(
        cases[1][key] > 0
        for key in ("source_first_common_points", "external_first_common_points")
    ):
        raise ValueError("Actual opening opponent must contain both depth orders")
    native_rgba = raw["source_lit_rgba"][::4, ::4]
    native_z = raw["source_raster_depth"][::4, ::4, 0]
    common = (native_z > 0) & (a.reverse_z > 0)
    native = PointSamples(
        binding, native_rgba, native_z, np.zeros(shape), np.zeros(shape, bool)
    )
    # No current native coverage/error certificate: retain the parent even if
    # this finite observation looks close. Old capture-view PBR stays explicit.
    controls = fallback_controls(a, native, provenance["capture_subset"])
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "originals": provenance,
        "source_field_manifest_sha256": digest(
            args.inputs / "source-512/manifest.json"
        ),
        "binding": binding.__dict__,
        "cases": cases,
        "fallbacks": controls,
        "old_native_field_observations": {
            "coverage_mismatches": int(
                np.count_nonzero((native_z > 0) != (a.reverse_z > 0))
            ),
            "common_points": int(common.sum()),
            "maximum_reverse_z_difference": float(
                abs(native_z[common] - a.reverse_z[common]).max(initial=0)
            ),
            "scope": "Finite old native field versus independent CPU original intersections; no continuous/raster error bound or matching PBR claim.",
        },
        "scope": "Whole unchanged native original geometry, exact decimated capture-ray subset, analytic flat terrain and controlled external opening, opaque HDR/Lambert CPU diagnostic. No native PBR/World/pose/main-shadow depth or runtime budget admission.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "whole_originals": provenance["source_count"],
                "cases": len(cases),
                "fallbacks": controls,
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
