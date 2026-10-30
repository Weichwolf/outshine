"""Whole source triangle columns against native shared quarter-pixel opponents."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
from compare_field_depth_reprojection import image_panel, nearest_colour
from compare_native_world_fields import (
    capture_for_pose,
    capture_region,
    model_image,
    pixels,
    reduce_samples,
)
from experiment_output import create_output
from native_world_inputs import DATA, POSES, clip_from_original, load
from triangle_columns import ColumnLimit, prepare, query, world_rays
from triangle_image_reference import intersect_source, prepare_source


def reference(source, prepared, world, points, raster, *, ray_batch=None):
    origins, directions = world_rays(world, points, raster)
    if ray_batch is None:
        distance, owner = intersect_source(
            source, origins, directions, prepared=prepared
        )
    else:
        distance, owner = (
            np.full(len(points), np.inf),
            np.full(len(points), -1, np.int64),
        )
        for begin in range(0, len(points), ray_batch):
            end = begin + ray_batch
            distance[begin:end], owner[begin:end] = intersect_source(
                source, origins[begin:end], directions[begin:end], prepared=prepared
            )
    mask = np.isfinite(distance)
    point = (
        np.c_[
            origins[mask] + distance[mask, None] * directions[mask], np.ones(mask.sum())
        ]
        @ world.T
    )
    z = np.zeros(len(points))
    z[mask] = point[:, 2] / point[:, 3]
    return z, owner


def coverage_record(arrays, pose, candidate, first, crop, output):
    source = arrays["original/" + pose + "/rgba"]
    z = arrays["original/" + pose + "/depth"]
    result = {}
    for outer in ("terrain", "external"):
        other, other_z = (
            arrays[outer + "/" + pose + "/" + key] for key in ("rgba", "depth")
        )
        values = [
            reduce_samples(((v > 0) & (v > other_z))[:, :, None].astype(float), crop, 2)
            for v in (z, candidate, first)
        ]
        truth, surface, first_hit = values
        result[outer] = {
            "multiple_surface_coverage_error_max": float(abs(surface - truth).max()),
            "multiple_surface_coverage_error_l1": float(abs(surface - truth).sum()),
            "first_hit_coverage_error_max": float(abs(first_hit - truth).max()),
            "first_hit_coverage_error_l1": float(abs(first_hit - truth).sum()),
            "original_group_retained": True,
            "proxy_material_and_runtime_admitted": False,
        }
        # Geometry-only panels, never shading filled from the native oracle.
        grey = lambda v: np.concatenate((np.repeat(v, 3, 2), np.ones_like(v)), 2)
        image_panel(
            output / (pose + "-" + outer + "-coverage.png"),
            grey(truth),
            grey(surface),
            grey(first_hit),
            labels=(
                "native visible-source coverage",
                "whole triangle columns",
                "first-hit cells",
                "column coverage error",
            ),
            scale=12,
        )
        fallback = reduce_samples(nearest_colour(source, z, other, other_z), crop, 2)
        native = reduce_samples(
            arrays["union-" + outer + "/" + pose + "/rgba"], crop, 2
        )
        fallback_z = np.maximum(z, other_z)
        np.testing.assert_array_equal(fallback, native)
        np.testing.assert_array_equal(
            fallback_z, arrays["union-" + outer + "/" + pose + "/depth"]
        )
        result[outer]["retained_original_rgba_and_sample_depth_exact"] = True
    return result


def pose_result(args, manifest, arrays, cases, source, prepared, fields, pose):
    capture, world = (
        clip_from_original(cases[k, pose]) for k in ("capture", "original")
    )
    raster, crop = manifest["raster"], np.array(manifest["crops_xyxy"][pose])
    region = capture_region(capture, world, raster)
    if np.any(region[:2] < crop[:2]) or np.any(region[2:] > crop[2:]):
        raise ValueError("Complete capture region must fit the native reference crop")
    points = pixels(region)
    expected, expected_face = reference(source, prepared, world, points, raster)
    records, selected = {}, None
    for size, field in fields.items():
        z, face, evaluated, work = query(field, world, points, raster)
        if not evaluated.all():
            raise ValueError(
                "Unresolved samples retain originals; no complete geometry qualification"
            )
        np.testing.assert_array_equal(z > 0, expected > 0)
        np.testing.assert_allclose(z, expected, atol=1e-18, rtol=1e-12)
        np.testing.assert_array_equal(face, expected_face)
        records[str(size)] = {
            "work": work,
            "reference_mask_differences": int(
                np.count_nonzero((z > 0) != (expected > 0))
            ),
            "different_original_triangle_ordinals": int(
                np.count_nonzero(face != expected_face)
            ),
            "raw_depth_reference_error_max": float(abs(z - expected).max()),
        }
        if size == 64:
            selected = z, face, evaluated
    if selected is None:
        raise ValueError("The fixed 64-square comparison is required")
    control = query(fields[64], world, points, raster, whole_support=False)
    for actual, original in zip(selected, control[:3], strict=True):
        np.testing.assert_array_equal(actual, original)
    records["64-unbounded-control"] = {
        "work": control[3],
        "all_geometry_outputs_equal": True,
    }
    shape = crop[3] - crop[1], crop[2] - crop[0]
    candidate = np.zeros(shape)
    tile = np.s_[
        region[1] - crop[1] : region[3] - crop[1],
        region[0] - crop[0] : region[2] - crop[0],
    ]
    candidate[tile] = selected[0].reshape(region[3] - region[1], region[2] - region[0])
    source_z = arrays["original/" + pose + "/depth"]
    outside = np.ones(shape, bool)
    outside[tile] = False
    if np.any(source_z[outside] > 0):
        raise ValueError("Metadata region omits a native source hit")
    first = model_image(
        capture_for_pose(arrays, cases, pose), capture, world, raster, crop, region
    )
    common = (source_z > 0) & (candidate > 0)
    result = {
        "region_xyxy": region.tolist(),
        "queries": records,
        "native_source_samples": int((source_z > 0).sum()),
        "column_samples": int((candidate > 0).sum()),
        "native_source_mask_differences": int(
            np.count_nonzero((source_z > 0) != (candidate > 0))
        ),
        "raw_depth_native_error_max_on_common_hits": float(
            abs(candidate[common] - source_z[common]).max()
        ),
        "first_hit_source_mask_differences": int(
            np.count_nonzero((source_z > 0) != (first.reverse_z > 0))
        ),
        "composition": coverage_record(
            arrays, pose, candidate, first.reverse_z, crop, args.output
        ),
    }
    np.savez_compressed(
        args.output / (pose + "-geometry.npz"),
        column_depth=candidate,
        original_depth=source_z,
        first_hit_depth=first.reverse_z,
        original_triangle_ordinal=selected[1],
        independent_reference_depth=expected,
        independent_reference_triangle=expected_face,
    )
    if args.cost:
        from triangle_columns_cost import query_costs

        result["costs"] = query_costs(
            fields[64],
            world,
            points,
            raster,
            expected,
            expected_face,
            args.output,
            pose,
            {
                "existing-rtree256": lambda: reference(
                    source, prepared, world, points, raster
                ),
                "existing-rtree32": lambda: reference(
                    source, prepared, world, points, raster, ray_batch=32
                ),
            },
        )
    return result


def run(args):
    names = (
        "compare_triangle_columns.py",
        "triangle_columns.py",
        "triangle_query_support.py",
        "triangle_columns_cost.py",
        "native_world_inputs.py",
        "building_sample_originals.py",
        "triangle_image_reference.py",
        "compare_native_world_fields.py",
        "field_depth_reprojection.py",
        "depth_reprojection.py",
        "field_view_cost.py",
        "compare_field_depth_reprojection.py",
    )
    paths = [Path(__file__).with_name(n) for n in names]
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
        raise ValueError("Freeze complete source study before recording evidence")
    create_output(args.output)
    m, a, c, source = load(args.input, attributes=True)
    if m["raster"] != [2560, 1440]:
        raise ValueError("Actual common quarter samples for 1280x720 required")
    capture = clip_from_original(c["capture", "near-direction"])
    fields = {size: prepare(source, capture, size) for size in (32, 64, 128)}
    prepared = prepare_source(source)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "input_manifest_sha256": hashlib.sha256(
            (args.input / "manifest.json").read_bytes()
        ).hexdigest(),
        "input_npz_sha256": m["npz_sha256"],
        "host": platform.platform(),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "original_geometry": m["original_geometry"],
        "original_parts": source["parts"],
        "retained_original_attribute_payload_bytes": sum(
            v.nbytes for p in source["attributes"] for v in p.values()
        ),
        "fields": {
            str(n): {
                "payload_bytes": f.payload_bytes,
                "triangle_references": len(f.faces),
                "original_triangles": len(f.triangles),
                "copied_geometry_bytes": f.positions.nbytes + f.triangles.nbytes,
            }
            for n, f in fields.items()
        },
        "poses": {
            pose: pose_result(args, m, a, c, source, prepared, fields, pose)
            for pose in POSES[:2]
        },
        "scope": "All-source geometry reference, not a distant LOD winner or material renderer. Complete Float32 original positions/topology retained in every field; CSR cells retain all depths, including capture-edge-on surfaces. Original attributes remain in verified part/vertex order outside field payload. No native colour used for proxy pixels. CPU geometric depth and finite GPU samples remain distinct; no certified rounding, World/pose/light admission, fine-IO/residency saving or device/p99 claim.",
    }
    if args.cost:
        from triangle_columns_cost import build_costs

        report["build_costs"] = build_costs(source, capture, args.output)
    try:
        prepare(source, capture, 64, references_most=1)
    except ColumnLimit:
        report["reference_budget_refuses_partial_product"] = True
    else:
        raise ValueError("Reference preflight did not reject insufficient capacity")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DATA / "ss2")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cost", action="store_true")
    run(parser.parse_args())
