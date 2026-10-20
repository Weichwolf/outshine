"""Stop constrained QEM at the real whole-cell cost preflight (#942)."""

import argparse
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from attributed_prisms import native_fields, native_inputs
from compare_attributed_prisms import picture
from compare_layered_tree import one_view
from compare_plane_cloud import select_full_poses
from constrained_mesh_simplification import installed_library, simplify
from full_view_reference import load, views
from triangle_image_reference import intersect_source, prepare_source


def query_mesh(product, origins, directions):
    source = product["source"]
    depth, face = intersect_source(
        source, origins, directions, prepared=product["prepared"]
    )
    fields = native_fields(product["inputs"], origins, directions, depth, face)
    owners = np.full(len(face), -1, np.int32)
    owners[face >= 0] = source["binding"][face[face >= 0], 1]
    return depth, fields, owners, np.full(len(face), -1, np.int32)


def exact_corners(source, candidate):
    for name in ("positions", "uv", "normal", "metres", "colour"):
        np.testing.assert_array_equal(
            source[name][source["indices"]].view(np.uint32),
            candidate[name][candidate["indices"]].view(np.uint32),
        )
    np.testing.assert_array_equal(source["binding"][:, :5], candidate["binding"][:, :5])
    return "every ordered position/normal/UV/metre/RGBA corner word and categorical lineage equal"


def measure(source, library, permissive):
    began, cpu = time.perf_counter(), time.process_time()
    candidate, report = simplify(source, library=library, permissive=permissive)
    report.update(
        wall_seconds=time.perf_counter() - began, cpu_seconds=time.process_time() - cpu
    )
    tracemalloc.start()
    repeated, counted = simplify(source, library=library, permissive=permissive)
    report["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    for key in candidate:
        np.testing.assert_array_equal(candidate[key], repeated[key])
    for key in counted:
        assert report[key] == counted[key]
    report["corner_identity"] = exact_corners(source, candidate)
    return candidate, report


def run(path, library, output):
    began = time.perf_counter()
    source, document, context, receipt = load(path)
    load_seconds = time.perf_counter() - began
    methods = {}
    for label, permissive in (("strict", False), ("permissive", True)):
        candidate, methods[label] = measure(source, library, permissive)
    # An optimistic work-only control cannot be mistaken for a quality candidate.
    _, optimistic = simplify(
        source, library=library, permissive=True, absolute_error_m=1e20
    )
    optimistic["scope"] = (
        "unbounded-error cost control only; not a rendered/admitted mesh, not an optimal lower bound"
    )
    np.savez_compressed(output / "nominal.npz", **candidate)
    product = {
        "source": candidate,
        "prepared": prepare_source(candidate),
        "inputs": native_inputs(candidate),
    }
    prepared, inputs = prepare_source(source), native_inputs(source)
    poses = select_full_poses(source, views(document, context))
    frame = context["scene_frame"]
    up, east, north = (np.array(frame[k]) for k in ("y_ecef", "x_ecef", "z_ecef"))
    suns = [up + 0.5 * east + 0.3 * north, up - 0.7 * east - 0.3 * north]
    images = {}
    for name, view in poses.items():
        images[name] = one_view(
            source,
            prepared,
            inputs,
            product,
            view,
            suns,
            output / (name + ".npz"),
            query_fn=query_mesh,
        )
    picture(
        list(poses),
        output,
        output / "materials.png",
        candidate_label="unchanged constrained QEM",
    )
    picture(
        list(poses),
        output,
        output / "normals.png",
        normals=True,
        candidate_label="unchanged constrained QEM",
    )
    source_arrays = (
        sum(a.nbytes for key, a in source.items() if key != "indices")
        + source["indices"].size * 4
    )
    report = {
        "decision": "stop: no geometry or neutral-layout residency reduction at the stated metric tolerance",
        "scope": "whole supplied cell, three unchanged-profile diagnostic poses; no full128-pose/native/global LOD admission",
        "input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "receipt": receipt,
        "original_native_built_bytes": document["results"][0]["built_bytes"],
        "reference_load_seconds": load_seconds,
        "neutral_uint32_reference_array_bytes": source_arrays,
        "methods": methods,
        "optimistic_cost_only": optimistic,
        "poses": images,
        "memory_scope": "dense Python diagnostic fields include derived normals/lineage; native asset/GPU layout differs. Native allocator/whole-process counts and copy totals unknown.",
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("constrained_mesh_simplification.py"),
            )
        },
    }
    assert all(v["known_output_array_bytes"] == source_arrays for v in methods.values())
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).resolve().parents[2]
        / "outshine/data/render/building-full-view/cell.json.gz",
    )
    parser.add_argument("--library", type=Path, default=installed_library())
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    result = run(args.input, args.library, args.output)
    print(
        json.dumps(
            {
                "decision": result["decision"],
                "nominal_faces": {
                    k: v["output_faces"] for k, v in result["methods"].items()
                },
                "optimistic_faces": result["optimistic_cost_only"]["output_faces"],
            }
        )
    )
