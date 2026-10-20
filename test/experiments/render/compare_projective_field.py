"""Reject or retain an analytic field by cost before a full-world or device gate."""

import argparse
import hashlib
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from compare_attributed_prisms import diagnostic_poses, picture
from compare_layered_tree import one_view
from compare_plane_cloud import select_full_poses
from full_view_reference import load, views
from projective_field_bake import cook
from projective_field_query import load_product, query
from triangle_image_reference import prepare_source


def factorized_control(source, field_arrays):
    if any(
        source[key].dtype != np.float32
        for key in ("positions", "uv", "metres", "colour")
    ):
        raise ValueError("control requires the actual Float32 native input fields")
    inputs = native_inputs(source)
    positions, position_map = np.unique(
        source["positions"].view(np.uint32), axis=0, return_inverse=True
    )
    original_attributes = np.column_stack((inputs["uv"], source["metres"]))
    if not np.all(source["colour"] == 1):
        original_attributes = np.column_stack((original_attributes, source["colour"]))
    attributes, attribute_map = np.unique(
        original_attributes.view(np.uint32), axis=0, return_inverse=True
    )
    ids = source["indices"]
    position_ids, attribute_ids = position_map[ids], attribute_map[ids]
    np.testing.assert_array_equal(
        positions[position_ids], source["positions"][ids].view(np.uint32)
    )
    np.testing.assert_array_equal(
        attributes[attribute_ids], original_attributes[ids].view(np.uint32)
    )
    np.testing.assert_array_equal(
        source["normal"][ids], np.broadcast_to(source["normal"][ids[:, :1]], ids.shape)
    )
    arrays = {
        "positions": positions,
        "attributes": attributes,
        "position_indices": position_ids.astype(
            np.uint16 if len(positions) <= 65536 else np.uint32
        ),
        "attribute_indices": attribute_ids.astype(
            np.uint16 if len(attributes) <= 65536 else np.uint32
        ),
        "normal": source["normal"][ids[:, 0]],
        "code": inputs["flat_codes"],
        **{
            key: field_arrays[key]
            for key in ("lineage", "lineage_index", "original_face", "native_face_tag")
        },
    }
    return arrays, {
        "arrays": {key: a.nbytes for key, a in arrays.items()},
        "total_array_bytes": sum(a.nbytes for a in arrays.values()),
        "position_rows": len(positions),
        "attribute_rows": len(attributes),
        "roundtrip": "all represented position and post-vertex attribute corner bits equal",
        "scope": "reachable exact storage control, same provenance arrays; no native layout/API or frame timing",
    }


def variant(
    source, reference, inputs, up, profile, provenance, poses, suns, dtype, output
):
    output.mkdir()
    start = time.perf_counter()
    header = cook(source, up, profile, output / "product", provenance, dtype=dtype)
    bake_s = time.perf_counter() - start
    start = time.perf_counter()
    product = load_product(
        output / "product", [v[0] for v in poses.values()], provenance
    )
    prepare_s = time.perf_counter() - start
    reports = {}
    for name, view in poses.items():
        reports[name] = one_view(
            source,
            reference,
            inputs,
            product,
            view,
            suns,
            output / (name + ".npz"),
            query_fn=query,
        )
    shown = diagnostic_poses(reports, poses)
    picture(shown, output, output / "material-ids.png")
    picture(shown, output, output / "normals.png", normals=True)
    request_name = next(iter(poses))
    view = poses[request_name]
    request = {
        "case": request_name,
        "view": [view[0].tolist(), view[1].tolist(), *view[2:]],
        "roi": reports[request_name]["roi"],
        "provenance": provenance,
    }
    (output / "request.json").write_text(json.dumps(request, indent=2) + "\n")
    result = {
        "product": header,
        "bake_s": bake_s,
        "prepare_s": prepare_s,
        "derived_cpu_bytes": product["derived_cpu_bytes"],
        "work": product["work"],
        "poses": reports,
        "shown": shown,
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    return result, product


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    temp, repo = (
        Path(tempfile.gettempdir()).resolve(),
        Path(__file__).resolve().parents[3],
    )
    if not args.output.resolve().is_relative_to(
        temp
    ) or args.output.resolve().is_relative_to(repo):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    poses = select_full_poses(source, views(document, context))
    first = next(iter(poses.values()))
    profile = {"width": int(first[3]), "height": int(first[4]), "focal_px": first[2]}
    provenance = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "parent": document["parent"],
        "view": document["view_key"],
    }
    up, east, north = (
        np.array(context["scene_frame"][key]) for key in ("y_ecef", "x_ecef", "z_ecef")
    )
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    reference, inputs = prepare_source(source), native_inputs(source)
    results = {}
    for name, dtype in (
        ("float64-control", np.float64),
        ("float32-product", np.float32),
    ):
        results[name], product = variant(
            source,
            reference,
            inputs,
            up,
            profile,
            provenance,
            poses,
            suns,
            dtype,
            args.output / name,
        )
    factor_arrays, factor_report = factorized_control(source, product["arrays"])
    np.savez(args.output / "factorized-storage-control.npz", **factor_arrays)
    geometry = document["results"][0]["geometry"]
    native = 24 * len(source["positions"]) + 12 * len(source["indices"])
    native += 4 * (
        len(geometry["wall_colours"]) + len(geometry["wall_material_metres"])
    )
    result = {
        "provenance": provenance,
        "profile": profile,
        "binding_check": receipt,
        "variants": results,
        "native_stored_core_bytes": native,
        "native_exact_u16_core_bytes": native - 6 * len(source["indices"]),
        "factorized_storage_control": factor_report,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [
                Path(__file__),
                *Path(__file__).parent.glob("projective_field_*.py"),
            ]
        },
        "scope": "three supplied probe poses only; stop on representation cost, no full120/world/device or native port admission",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                key: value
                for key, value in result.items()
                if key not in ("variants", "binding_check", "source_sha256")
            }
        )
    )


if __name__ == "__main__":
    main()
