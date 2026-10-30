"""Exact attribute payload and selected-reader costs on whole native source inputs."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path
from unittest.mock import patch

import compact_fragment_inputs as compact
import numpy as np
import source_fragment_inputs as fragments
import triangle_columns as columns
from compare_field_depth_reprojection import image_panel
from compare_native_world_fields import capture_region, pixels
from compare_source_fragment_inputs import panel_values, same_inputs
from compare_triangle_columns import reference
from experiment_output import create_output
from native_world_inputs import DATA, POSES, clip_from_original, load
from triangle_image_reference import prepare_source


def describe(materials):
    return {
        k: {
            "mode": v.mode,
            "bytes": v.nbytes,
            "shape": v.shape,
            "palette_rows": len(v.table) if v.indices is not None else None,
            "index_bytes": 0 if v.indices is None else v.indices.nbytes,
        }
        for k in compact.KEYS
        for v in (materials.source[k],)
    }


def pose_result(args, m, arrays, cases, source, tree, raw, candidates, pose):
    world = clip_from_original(cases["original", pose])
    crop = np.array(m["crops_xyxy"][pose])
    region = capture_region(raw.geometry.capture, world, m["raster"])
    if np.any(region[:2] < crop[:2]) or np.any(region[2:] > crop[2:]):
        raise ValueError("Complete capture region must fit native crop")
    points = pixels(region)
    geo = columns.query(raw.geometry, world, points, m["raster"])
    z, face = reference(source, tree, world, points, m["raster"])
    np.testing.assert_array_equal(geo[1], face)
    np.testing.assert_allclose(geo[0], z, atol=1e-18, rtol=1e-12)
    if not geo[2].all():
        raise ValueError("Unresolved geometry keeps original group")
    expected, ew = fragments.read(raw, world, points, m["raster"], geo)
    native = arrays["original/" + pose + "/depth"][
        region[1] - crop[1] : region[3] - crop[1],
        region[0] - crop[0] : region[2] - crop[0],
    ].ravel()
    result = {
        "original_work": ew,
        "native_geometry_mask_differences": int(
            np.count_nonzero((native > 0) != (geo[0] > 0))
        ),
        "variants": {},
    }
    selected = None
    for mode, material in candidates.items():
        value, work = fragments.read(material, world, points, m["raster"], geo)
        same_inputs(value, expected)
        corners = work["interpolated_corner_rows"]
        work["known_corner_value_payload_bytes"] = corners * 44
        work["known_palette_index_gather_bytes"] = sum(
            corners * v.indices.dtype.itemsize
            for k in compact.KEYS
            for v in (material.source[k],)
            if v.indices is not None
        )
        result["variants"][mode] = {"work": work, "raw_outputs_bit_identical": True}
        if args.cost:
            from field_view_cost import measured

            final, cost = measured(
                lambda material=material: fragments.read(
                    material, world, points, m["raster"], geo
                ),
                args.output,
                pose + "-" + mode,
            )
            same_inputs(final[0], expected)
            result["variants"][mode]["cost"] = cost
        if mode == "adaptive":
            selected = value
    if args.cost:
        from field_view_cost import measured

        final, cost = measured(
            lambda: fragments.read(raw, world, points, m["raster"], geo),
            args.output,
            pose + "-original",
        )
        same_inputs(final[0], expected)
        result["original_cost"] = cost
    part_colour = np.repeat(
        (selected["part_ordinals"][:, None] + 1) / (len(source["parts"]) + 1), 3, axis=1
    )
    part_image = panel_values(part_colour, selected, crop, region)
    for key in ("normal", "colour"):
        values = [
            (v[key] + 1) / 2 if key == "normal" else v[key][:, :3]
            for v in (expected, selected)
        ]
        images = [
            panel_values(v, data, crop, region)
            for v, data in zip(values, (expected, selected), strict=True)
        ]
        image_panel(
            args.output / (pose + "-" + key + "-words.png"),
            *images,
            part_image,
            labels=(
                "original raw " + key,
                "selected word rows " + key,
                "original part ordinal (debug)",
                "raw input difference",
            ),
            scale=12,
        )
    np.savez_compressed(
        args.output / (pose + "-word-inputs.npz"),
        **selected,
        raw_depth=geo[0],
        geometry_evaluated=geo[2],
        region=region,
    )
    return result


def run(args):
    names = (
        "compare_attribute_words.py",
        "attribute_words.py",
        "compact_fragment_inputs.py",
        "vertex_colour_palette.py",
        "source_fragment_inputs.py",
        "native_capture_meshes.py",
        "native_world_inputs.py",
        "building_sample_originals.py",
        "triangle_columns.py",
        "triangle_pair_batches.py",
        "triangle_query_support.py",
        "triangle_image_reference.py",
        "compare_source_fragment_inputs.py",
        "compare_native_world_fields.py",
        "compare_field_depth_reprojection.py",
        "compare_triangle_columns.py",
        "field_view_cost.py",
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
        raise ValueError("Freeze exact attribute comparison first")
    create_output(args.output)
    m, arrays, cases, source = load(args.input, attributes=True)
    if m["raster"] != [2560, 1440]:
        raise ValueError("Actual shared quarter samples for1280x720 required")
    field = columns.prepare(
        source, clip_from_original(cases["capture", "near-direction"]), 64
    )
    raw = fragments.prepare(field, source)
    tree = prepare_source(source)
    candidates = {
        mode: compact.compact(raw, mode)
        for mode in ("direct", "adaptive", "palette16", "palette32")
    }
    inventory = {}
    for mode, material in candidates.items():
        inventory[mode] = {
            "attribute_payload_bytes": material.attribute_payload_bytes,
            "fields": describe(material),
            "whole_words_equal": True,
        }
        for key in compact.KEYS:
            actual = material.source[key][np.arange(len(field.positions))]
            np.testing.assert_array_equal(
                actual.view(np.uint32), raw.source[key].view(np.uint32)
            )
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
        "original_attribute_payload_bytes": raw.attribute_payload_bytes,
        "shared_geometry_payload_bytes": field.payload_bytes,
        "variants": inventory,
        "poses": {
            pose: pose_result(
                args, m, arrays, cases, source, tree, raw, candidates, pose
            )
            for pose in POSES[:2]
        },
        "runtime_world_from_field": m["runtime_world_from_field"],
        "runtime_world_revision": m["runtime_world_revision"],
        "scope": "Exact reference attribute representation only. Encoded payload includes legacy headers/padding; Python object overhead excluded. All original fine geometry remains. Full word verification/decoding is outside query timing; query resolves only requested corners. Source loading, geometry queries/control, pictures, imports and held snapshots excluded from query scope. No native PBR/World/cache-hit/IO/UMA/device/p99 claim; null revisions stay null.",
    }
    if args.cost:
        from field_view_cost import measured

        costs = {}
        for mode, call in (
            ("original", lambda: fragments.prepare(field, source)),
            ("adaptive", lambda: compact.prepare(field, source)),
            ("direct", lambda: compact.prepare(field, source, "direct")),
        ):
            value, cost = measured(call, args.output, "prepare-" + mode)
            for key in compact.KEYS:
                actual = (
                    value.source[key]
                    if mode == "original"
                    else value.source[key][np.arange(len(field.positions))]
                )
                np.testing.assert_array_equal(
                    actual.view(np.uint32), raw.source[key].view(np.uint32)
                )
            costs[mode] = cost
        report["prepare_costs"] = costs
        with patch(
            "vertex_colour_palette.unique_rows",
            lambda words: np.unique(words, axis=0, return_inverse=True),
        ):
            value, cost = measured(
                lambda: compact.prepare(field, source),
                args.output,
                "prepare-adaptive-axis-control",
            )
        for key in compact.KEYS:
            np.testing.assert_array_equal(
                value.source[key][np.arange(len(field.positions))].view(np.uint32),
                raw.source[key].view(np.uint32),
            )
        report["prepare_costs"]["adaptive_axis_control"] = cost
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DATA / "ss2")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cost", action="store_true")
    run(parser.parse_args())
