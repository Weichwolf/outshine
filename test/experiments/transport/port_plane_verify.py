"""Independently audit saved whole-tile C1 plans against the immutable captured input."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.transport.port_heights._contacts import contact_audit
from outshine_ref.transport.port_heights._layout import prepare_plane_field
from outshine_ref.transport.port_heights._system import (
    continuous_matrix,
    vertex_matrices,
)
from outshine_ref.transport.port_heights._topology import topology_audit
from port_plane_adaptive import initial_mesh, quality
from port_plane_fixture import load_reference


def verify_coverage(original, mesh):
    polygons = shapely.polygons(mesh["xy"][mesh["faces"]])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("independent GEOS coverage validation failed")
    domain = shapely.coverage_union_all(
        shapely.polygons(original["xy"][original["faces"]])
    )
    error = shapely.symmetric_difference(
        shapely.coverage_union_all(polygons), domain
    ).area
    if error > 1e-6:
        raise ValueError("saved plan lost original whole-tile coverage")
    return error


def verify_implementation(report):
    for name, digest in report["implementation_sha256"].items():
        if (
            hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
            != digest
        ):
            raise ValueError(f"verification source differs from generation: {name}")
    root = Path(__file__).resolve().parents[3]
    for relative, digest in report.get("library_sha256", {}).items():
        if hashlib.sha256((root / relative).read_bytes()).hexdigest() != digest:
            raise ValueError(f"library differs from generation: {relative}")


def verify(directory, *, allow_dirty=False):
    began = time.monotonic()
    report = json.loads((directory / "report.json").read_text())
    if not allow_dirty and not report.get("valid_geometry", False):
        raise ValueError(
            "no accepted source-fitted geometry result in generation report"
        )
    path = directory / report.get("mesh_file", "diagnostic.npz")
    if hashlib.sha256(path.read_bytes()).hexdigest() != report["mesh_sha256"]:
        raise ValueError("saved plan changed after generation")
    if report["working_tree_dirty"] and not allow_dirty:
        raise ValueError("release verification requires a clean generation commit")
    if not allow_dirty:
        verify_implementation(report)
    product, captured, metadata, _sources = load_reference()
    model, _ = prepare_plane_field(product)
    with np.load(path, allow_pickle=False) as source:
        data = {key: source[key] for key in source.files}
    mesh = {
        key: data[key]
        for key in (
            "xy",
            "faces",
            "parts",
            "frames",
            "targets",
            "roots",
            "original_vertex_map",
            "intervals",
            "heights",
        )
    }
    mesh["port_roots"] = {
        tuple(edge): int(i) for edge, i in zip(data["port_edges"], data["port_ids"])
    }
    original = initial_mesh(product, captured)
    topology = topology_audit(original, mesh)
    displacement = np.linalg.norm(
        mesh["xy"][mesh["original_vertex_map"]] - original["xy"], axis=1
    )
    tolerance = 64 * np.finfo(float).eps * max(1, abs(original["xy"]).max())
    if displacement.max() > tolerance:
        raise ValueError("source anchors moved beyond floating point normalization")
    ports = mesh["original_vertex_map"][
        captured["road_vertex_map"][product["port_edges"]]
    ]
    np.testing.assert_array_equal(
        mesh["xy"][ports], product["vertices"][product["port_edges"]]
    )
    np.testing.assert_array_equal(mesh["parts"], original["parts"][mesh["roots"]])
    for name in (
        "plane_origins",
        "plane_ids",
        "cut_centres",
        "inward_normals",
        "scales",
    ):
        actual = getattr(model, "origins" if name == "plane_origins" else name)
        np.testing.assert_array_equal(actual, data[name])
    coverage_error = verify_coverage(original, mesh)
    h, equal, _, _ = vertex_matrices(model, mesh["xy"], mesh["faces"], mesh["parts"])
    reconstruction = float(
        abs(h @ data["coefficients"] + data["datum_m"] - mesh["heights"]).max()
    )
    equality_error = float(abs(equal @ data["coefficients"]).max(initial=0))
    if max(reconstruction, equality_error) > 1e-7:
        raise ValueError("saved heights do not reproduce the common plane plan")
    planes = data["coefficients"][: 3 * len(model.origins)]
    certificate, _ = continuous_matrix(
        model,
        product["vertices"],
        product["indices"],
        product["face_owners"],
        captured["road_frames"],
    )
    original_bound = float(
        np.linalg.norm((certificate @ planes).reshape(-1, 2), axis=1).max()
    )
    _, audit, _ = quality(model, planes, mesh, product["part_roles"])
    if (
        not audit["geometry_complete"]
        or not audit["whole_world_valid"]
        or original_bound > 1.0000001
    ):
        raise ValueError("saved whole plan fails independent geometry acceptance")
    contacts = contact_audit(
        model,
        planes,
        product,
        mesh["heights"][mesh["original_vertex_map"][captured["road_vertex_map"]]],
    )
    result = {
        "valid": True,
        "generation_commit": report["generation_commit"],
        "diagnostic_dirty_source_allowed": allow_dirty,
        "mesh_sha256": report["mesh_sha256"],
        "frame": metadata["frame"],
        "topology": topology,
        "coverage_error_m2": coverage_error,
        "maximum_anchor_displacement_m": float(displacement.max()),
        "height_reconstruction_error_m": reconstruction,
        "point_equality_error": equality_error,
        "original_continuous_gradient_bound": original_bound,
        "analytic_contacts": contacts,
        "quality": audit,
        "verification_ms": (time.monotonic() - began) * 1000,
        "scope": "Geometry/topology only; separate image, comparison, cost and library/native acceptance required.",
    }
    (directory / "verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--allow-dirty", action="store_true")
    args = parser.parse_args()
    verify(args.directory, allow_dirty=args.allow_dirty)
