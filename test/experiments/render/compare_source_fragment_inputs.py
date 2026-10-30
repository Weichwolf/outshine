"""Source-bound raw fragment inputs at actual native World sample positions."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
import source_fragment_inputs as fragments
import triangle_columns as columns
from compare_field_depth_reprojection import image_panel
from compare_native_world_fields import capture_region, pixels, reduce_samples
from compare_triangle_columns import reference
from experiment_output import create_output
from native_world_inputs import DATA, POSES, clip_from_original, load
from triangle_image_reference import prepare_source


def same_inputs(a, b):
    if a.keys() != b.keys():
        raise ValueError("Compared paths must return the same raw input contract")
    for key in a:
        np.testing.assert_array_equal(a[key], b[key])


def panel_values(value, data, crop, region):
    shape = region[3] - region[1], region[2] - region[0]
    local = np.zeros((shape[0] * shape[1], 4))
    local[data["sample_indices"], :3] = value
    local[data["sample_indices"], 3] = 1
    image = np.zeros((crop[3] - crop[1], crop[2] - crop[0], 4))
    image[
        region[1] - crop[1] : region[3] - crop[1],
        region[0] - crop[0] : region[2] - crop[0],
    ] = local.reshape(*shape, 4)
    return reduce_samples(image, crop, 2)


def pose_result(args, m, arrays, cases, source, tree, materials, pose):
    world = clip_from_original(cases["original", pose])
    crop = np.array(m["crops_xyxy"][pose])
    region = capture_region(materials.geometry.capture, world, m["raster"])
    if np.any(region[:2] < crop[:2]) or np.any(region[2:] > crop[2:]):
        raise ValueError("Whole capture footprint must fit the native crop")
    points = pixels(region)
    geometry = columns.query(materials.geometry, world, points, m["raster"])
    expected_z, expected_faces = reference(source, tree, world, points, m["raster"])
    np.testing.assert_array_equal(geometry[1], expected_faces)
    np.testing.assert_allclose(geometry[0], expected_z, atol=1e-18, rtol=1e-12)
    if not geometry[2].all():
        raise ValueError("Whole group retains original on unresolved geometry")
    sparse, sw = fragments.read(materials, world, points, m["raster"], geometry)
    dense, dw = fragments.read(
        materials, world, points, m["raster"], geometry, method="dense"
    )
    same_inputs(sparse, dense)
    if sw["unknown_attribute_hits"]:
        raise ValueError("Raw input comparison remains unresolved")
    triangles = materials.geometry.triangles[sparse["original_triangle_ordinals"]]
    u = materials.source["uv"][triangles, 0]
    codes = np.where(u < 0, u, np.floor(u / np.float32(256)))
    record = {
        "samples": len(points),
        "geometry_work": geometry[3],
        "sparse_work": sw,
        "dense_work": dw,
        "original_part_hit_counts": np.bincount(
            sparse["part_ordinals"], minlength=len(source["parts"])
        ).tolist(),
        "raw_outputs_bit_identical": True,
        "hit_triangles_with_nonuniform_vertex_facade_code": int(
            np.count_nonzero(np.any(codes != codes[:, :1], axis=1))
        ),
        "shader_flat_facade_code_or_derivatives_admitted": False,
    }
    native = arrays["original/" + pose + "/depth"][
        region[1] - crop[1] : region[3] - crop[1],
        region[0] - crop[0] : region[2] - crop[0],
    ].ravel()
    record["native_geometry_mask_differences"] = int(
        np.count_nonzero((native > 0) != (geometry[0] > 0))
    )
    part_colour = np.repeat(
        (sparse["part_ordinals"][:, None] + 1) / (len(source["parts"]) + 1), 3, axis=1
    )
    part_image = panel_values(part_colour, sparse, crop, region)
    for key in ("normal", "colour"):
        values = [
            (d[key] + 1) / 2 if key == "normal" else d[key][:, :3]
            for d in (dense, sparse)
        ]
        images = [
            panel_values(v, d, crop, region)
            for v, d in zip(values, (dense, sparse), strict=True)
        ]
        image_panel(
            args.output / (pose + "-" + key + "-inputs.png"),
            *images,
            part_image,
            labels=(
                "dense raw " + key + " inputs",
                "sparse raw " + key + " inputs",
                "original part ordinal (debug)",
                "raw input difference",
            ),
            scale=12,
        )
    np.savez_compressed(
        args.output / (pose + "-raw-inputs.npz"),
        **sparse,
        raw_depth=geometry[0],
        geometry_evaluated=geometry[2],
        region=region,
    )
    if args.cost:
        from field_view_cost import measured

        costs = {}
        for method in ("dense", "sparse"):
            value, cost = measured(
                lambda method=method: fragments.read(
                    materials, world, points, m["raster"], geometry, method=method
                ),
                args.output,
                pose + "-" + method,
            )
            same_inputs(value[0], sparse)
            costs[method] = cost
        record["costs"] = costs
    return record


def run(args):
    names = (
        "compare_source_fragment_inputs.py",
        "source_fragment_inputs.py",
        "native_capture_meshes.py",
        "native_world_inputs.py",
        "building_sample_originals.py",
        "triangle_columns.py",
        "triangle_query_support.py",
        "triangle_image_reference.py",
        "compare_triangle_columns.py",
        "compare_native_world_fields.py",
        "compare_field_depth_reprojection.py",
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
        raise ValueError("Freeze source input study before reporting")
    create_output(args.output)
    m, arrays, cases, source = load(args.input, attributes=True)
    if m["raster"] != [2560, 1440]:
        raise ValueError("Shared quarter samples must resolve to1280x720")
    field = columns.prepare(
        source, clip_from_original(cases["capture", "near-direction"]), 64
    )
    materials = fragments.prepare(field, source)
    tree = prepare_source(source)
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
        "original_geometry": m["original_geometry"],
        "host": platform.platform(),
        "loadavg": os.getloadavg(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "original_parts": source["parts"],
        "attribute_payload_bytes": materials.attribute_payload_bytes,
        "shared_geometry_payload_bytes": field.payload_bytes,
        "known_prepare_attribute_copy_bytes_at_least": 2
        * sum(
            materials.source[k].nbytes
            for k in ("vertex_normal", "uv", "metres", "colour")
        )
        + materials.part_ends.nbytes
        + materials.materials.nbytes,
        "poses": {
            pose: pose_result(args, m, arrays, cases, source, tree, materials, pose)
            for pose in POSES[:2]
        },
        "runtime_world_from_field": m["runtime_world_from_field"],
        "runtime_world_revision": m["runtime_world_revision"],
        "scope": "Raw original-frame part/vertex inputs only, before native normal packing, vertex transforms, flat facade code, procedural material/derivative evaluation and lighting. No native RGB fills proxy samples; all pictures show raw attributes. Full fine geometry/attributes remain resident. Native PBR/World/IO/cache/UMA/device/p99 unqualified.",
    }
    if args.cost:
        from field_view_cost import measured

        value, cost = measured(
            lambda: fragments.prepare(field, source), args.output, "prepare-attributes"
        )
        for key in materials.source:
            np.testing.assert_array_equal(value.source[key], materials.source[key])
        report["prepare_cost"] = cost
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DATA / "ss2")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cost", action="store_true")
    run(parser.parse_args())
