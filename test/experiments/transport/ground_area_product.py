"""Fresh bounded diagnostic product/reader; this is no native cache format."""

import argparse
import json
from dataclasses import asdict
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from experiment_output import create_output
from ground_area_bounds import verify_geometry
from ground_area_inputs import FIXTURE, load
from ground_area_recipe import construct, prepare
from outshine_ref.contracts import Mesh
from stair_terrain_experiment import frozen

FILES = {
    "body-vertices.f64": ("<f8", "body_vertices", 2),
    "body-triangles.u32": ("<u4", "body_triangles", 6),
    "upper-vertices.f64": ("<f8", "upper_vertices", 1),
    "lower-vertices.f64": ("<f8", "lower_vertices", 1),
    "cap-triangles.u32": ("<u4", "cap_triangles", 2),
}


def source_reference(inputs):
    row = inputs.case["source"]
    return {
        "dataset_sha256": row["receipt"]["sha256"],
        "tile": row["receipt"]["tile"],
        "layer": "transportation",
        "feature_ordinal": row["feature_ordinal"],
        "feature_id": row["feature"]["id"],
        "geometry_type": row["feature"]["geometry"]["type"],
    }


def produce(args):
    commit = frozen()
    inputs = load(args.case)
    prepared = prepare(inputs.original, inputs.surface, inputs.recipe)
    body, upper, lower, plane = construct(prepared, inputs.recipe, args.method)
    proof = verify_geometry(
        body, upper, lower, inputs.surface, inputs.original, inputs.recipe
    )
    create_output(args.output)
    values = {
        "body_vertices": body.vertices,
        "body_triangles": body.triangles,
        "upper_vertices": upper.vertices,
        "lower_vertices": lower.vertices,
        "cap_triangles": upper.triangles,
    }
    artifacts = {}
    for name, (dtype, key, _factor) in FILES.items():
        path = args.output / name
        values[key].astype(dtype).tofile(path)
        artifacts[name] = {"bytes": path.stat().st_size, "sha256": digest(path)}
    input_copy = args.output / "input.json"
    input_copy.write_bytes(args.case.read_bytes())
    artifacts["input.json"] = {
        "bytes": input_copy.stat().st_size,
        "sha256": digest(input_copy),
    }
    report = {
        "version": 1,
        "kind": "whole-ground-area-diagnostic",
        "producer_commit": commit,
        "input_sha256": digest(args.case),
        "source": source_reference(inputs),
        "frame": inputs.case["frame"],
        "recipe": asdict(inputs.recipe),
        "method": args.method,
        "artifacts": artifacts,
        "physical_clearance_m": inputs.clearance_m,
        "body_vertices": len(body.vertices),
        "body_triangles": len(body.triangles),
        "geometry_payload_bytes": body.vertices.nbytes + body.triangles.nbytes,
        "model_area_m2": prepared["polygon"].area,
        "original_area_m2": inputs.original.area,
        "plane": {
            k: v.tolist() if isinstance(v, np.ndarray) else v for k, v in plane.items()
        },
        "geometry_proof": proof,
        "scope": "Whole actual source floor on unchanged held terrain. Procedural paving dimensions; opaque geometry only. No source/routing/accessibility/vehicle/World/GPU/device or cache-hit admission. Raw files are diagnostic buffers, not a proposed asset DB format.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def read_product(root, expected=FIXTURE):
    if expected.stat().st_size > 1024**2:
        raise ValueError("Bounded requested source input required before hashing")
    if (root / "report.json").stat().st_size > 1024**2:
        raise ValueError("Bounded area product metadata required")
    report = json.loads((root / "report.json").read_text())
    if (
        report.get("version") != 1
        or report.get("kind") != "whole-ground-area-diagnostic"
        or report.get("input_sha256") != digest(expected)
    ):
        raise ValueError("Same actual requested complete source/terrain input required")
    if set(report["artifacts"]) != set(FILES) | {"input.json"}:
        raise ValueError("Exact whole area artifact inventory required")
    for name, record in report["artifacts"].items():
        path = root / name
        size = path.stat().st_size
        limit = (
            1024**2
            if name == "input.json"
            else FILES[name][2] * 4096 * 3 * np.dtype(FILES[name][0]).itemsize
        )
        if not 0 < size <= limit:
            raise ValueError("Bounded whole artifact size required before hashing")
        if size != record["bytes"] or digest(path) != record["sha256"]:
            raise ValueError("Exact stored whole-area bytes changed")
    if digest(root / "input.json") != digest(expected):
        raise ValueError("Stored whole original input differs from requested source")
    inputs = load(root / "input.json")
    if (
        report["source"] != source_reference(inputs)
        or report["frame"] != inputs.case["frame"]
        or report["recipe"] != asdict(inputs.recipe)
        or report["physical_clearance_m"] != inputs.clearance_m
    ):
        raise ValueError("Actual source/frame/profile/paving recipe binding changed")
    arrays = {}
    for name, (dtype, key, factor) in FILES.items():
        path = root / name
        row_bytes = 3 * np.dtype(dtype).itemsize
        if (
            not 0
            < path.stat().st_size
            <= factor * inputs.recipe.coordinates_most * row_bytes
            or path.stat().st_size % row_bytes
        ):
            raise ValueError(
                "Bounded exact primitive rows required before array allocation"
            )
        value = np.fromfile(path, dtype=dtype).reshape(-1, 3)
        if not np.isfinite(value).all():
            raise ValueError("Finite complete stored area geometry required")
        arrays[key] = value
    body = Mesh(arrays["body_vertices"], arrays["body_triangles"])
    upper = Mesh(arrays["upper_vertices"], arrays["cap_triangles"])
    lower = Mesh(arrays["lower_vertices"], arrays["cap_triangles"])
    proof = verify_geometry(
        body, upper, lower, inputs.surface, inputs.original, inputs.recipe
    )
    if (
        report["body_vertices"] != len(body.vertices)
        or report["body_triangles"] != len(body.triangles)
        or report["geometry_payload_bytes"]
        != body.vertices.nbytes + body.triangles.nbytes
    ):
        raise ValueError("Actual whole product counts changed")
    return inputs, report, body, upper, lower, proof


def read(args):
    inputs, report, _body, _upper, _lower, proof = read_product(
        args.products, args.case
    )
    create_output(args.output)
    note = {
        "reader_commit": frozen(),
        "producer_commit": report["producer_commit"],
        "report_sha256": digest(args.products / "report.json"),
        "requested_input_sha256": digest(args.case),
        "source": source_reference(inputs),
        "whole_stored_area_verified": True,
        "geometry": proof,
        "scope": "Fresh independent saved-geometry/source binding proof, not a runtime cache-hit or native/GPU/world acceptance.",
    }
    (args.output / "verification.json").write_text(json.dumps(note, indent=2) + "\n")
    print(json.dumps(note))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("produce", "read"):
        command = commands.add_parser(name)
        command.add_argument("--case", type=Path, default=FIXTURE)
        command.add_argument("--output", type=Path, required=True)
        if name == "produce":
            command.add_argument(
                "--method", choices=("affine", "highs", "facet"), default="affine"
            )
            command.set_defaults(action=produce)
        else:
            command.add_argument("products", type=Path)
            command.set_defaults(action=read)
    args = parser.parse_args()
    args.action(args)
