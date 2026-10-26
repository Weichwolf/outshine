"""Replay actual saved tilted rail geometry without reconstructing it with the builder."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import bake_rail_sleepers
from rail_place_experiment import packed_mesh
from tilted_rail_geometry import TiltedRail
from tilted_rail_inputs import affine_pieces, inputs
from tilted_rail_verify import GroundContact, admit, metric_audit


def load_product(path, triangle):
    with np.load(path) as d:
        rails = Mesh(d["vertices"], d["triangles"])
        transforms, origin, frame = (
            d["sleeper_transforms"],
            d["plane_origin"],
            d["plane_frame"],
        )
    normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
    normal *= np.sign(normal[2])
    normal /= np.linalg.norm(normal)
    if (
        not np.array_equal(origin, triangle[0])
        or np.max(abs(frame.T @ frame - np.eye(3))) > 1e-12
        or np.max(abs(frame[:, 2] - normal)) > 1e-12
        or np.linalg.det(frame) <= 0
    ):
        raise ValueError("saved bed frame is not the selected metric plane")
    rings = rails.vertices.reshape(2, -1, 12, 3)
    rail_faces = rails.triangles.reshape(2, -1, 3)
    rail_shapes = []
    offset = rings.shape[1] * 12
    for index in range(2):
        polygons = shapely.polygons(
            rings[index].reshape(-1, 3)[rail_faces[index] - index * offset, :2]
        )
        rail_shapes.append(shapely.union_all(polygons[shapely.area(polygons) > 0]))
    # Independently reconstruct the eight physical corners of every box.
    unit = np.array(
        [
            [-0.5, -0.5, 0],
            [0.5, -0.5, 0],
            [0.5, 0.5, 0],
            [-0.5, 0.5, 0],
            [-0.5, -0.5, 1],
            [0.5, -0.5, 1],
            [0.5, 0.5, 1],
            [-0.5, 0.5, 1],
        ]
    )
    corners = np.array([unit @ t[:, :3].T + t[:, 3] for t in transforms])
    boxes = bake_rail_sleepers(transforms)
    if np.max(abs(boxes.vertices.reshape(-1, 8, 3) - corners), initial=0) > 1e-9:
        raise ValueError("saved instances disagree with actual baked boxes")
    shapes = rail_shapes + [shapely.MultiPoint(p[:, :2]).convex_hull for p in corners]
    return TiltedRail(
        rails, transforms, tuple(shapes), tuple(corners[:, :4]), origin, frame, None
    )


def verify_display(args, report, packed):
    if (
        digest(args.products / "after-body.npz")
        != report["render_inputs"][1]["export"]["sha256"]
    ):
        raise ValueError("actual rendered mesh changed")
    with (
        np.load(args.baseline / "after-body.npz") as base,
        np.load(args.products / "after-body.npz") as after,
    ):
        n, f = len(base["vertices"]), len(base["triangles"])
        for key in ["vertices", "triangles", "materials", "normals"]:
            size = n if key == "vertices" else f * 3 if key == "normals" else f
            if not np.array_equal(after[key][:size], base[key]):
                raise ValueError("old network/bridge/tunnel/piers changed")
        for actual, expected in [
            (after["vertices"][n:], packed["vertices"]),
            (after["triangles"][f:] - n, packed["triangles"]),
            (after["materials"][f:], np.where(packed["materials"] == 6, 8, 9)),
        ]:
            if not np.array_equal(actual, expected):
                raise ValueError("rendered body differs from actual hardware")
        p = packed["vertices"][packed["triangles"]]
        normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        normalized = (normal / np.linalg.norm(normal, axis=1)[:, None]).astype(
            np.float32
        )
        if not np.array_equal(
            after["normals"][f * 3 :].reshape(-1, 3, 3),
            np.repeat(normalized[:, None], 3, axis=1),
        ):
            raise ValueError(
                "provided shader normals differ from actual hardware faces"
            )
        q = packed["vertices"].astype(np.float32).astype(float)[packed["triangles"]]
        actual = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
        if np.any(np.linalg.norm(actual, axis=1) == 0) or np.any(
            np.einsum("ij,ij->i", normal, actual) <= 0
        ):
            raise ValueError("actual float32 hardware degenerates or inverts")
    return {
        "hardware_faces_float32_preserved": len(packed["triangles"]),
        "provided_normals_exact": True,
    }


def replay(args):
    report = json.loads((args.products / "comparison.json").read_text())
    if (
        report["dirty"]
        or digest(args.baseline / "comparison.json") != report["baseline_report_sha256"]
    ):
        raise ValueError("clean bound baseline required")
    candidates, _chains, _work, _raw, ground, road, field, bed, bindings = inputs(
        args.network
    )
    pieces, _cuts = affine_pieces(candidates, field)
    if bindings != report["bindings"] or len(pieces) != len(report["pieces"]):
        raise ValueError("source/field/station ledger changed")
    reference = GroundContact(ground, road)
    products, rows, proofs = [], [], []
    for candidate, row in zip(pieces, report["pieces"], strict=True):
        if (
            row["sources"] != candidate["sources"]
            or row["plane_face"] != candidate["plane_face"]
            or not np.array_equal(row["xy"], candidate["xy"])
        ):
            raise ValueError("rail source or affine-piece ownership changed")
        if not row["valid"]:
            if not row["reason"]:
                raise ValueError("missing optional rail omission reason")
            continue
        path = args.products / "pieces" / f"piece-{row['product']}.npz"
        if digest(path) != row["mesh_sha256"] or row["product"] != len(products):
            raise ValueError("saved actual piece changed")
        triangle = field.vertices[field.triangles[row["plane_face"]]]
        product = load_product(path, triangle)
        shape = shapely.union_all(product.footprints)
        if shape.difference(bed).area > 1e-7:
            raise ValueError("actual hardware outside selected finite bed")
        proof = reference.verify(shape, triangle)
        proof.update(metric_audit(product, row["gauge_m"]))
        rings = product.rails.vertices.reshape(2, -1, 12, 3)
        axis = (rings[:, :, 0] + rings[:, :, 1]).mean(axis=0) / 2 - product.plane_frame[
            :, 2
        ] * 0.18
        if (
            shapely.LineString(axis[:, :2]).hausdorff_distance(
                shapely.LineString(row["xy"])
            )
            > 1e-7
        ):
            raise ValueError("actual rail centreline differs from source piece")
        products.append(product)
        rows.append(dict(row))
        proofs.append(proof)
    kept, mutual = admit(products, rows)
    if len(kept) != len(products):
        raise ValueError("actual hardware pieces overlap")
    packed = packed_mesh(products)
    if digest(args.products / "hardware.npz") != report["hardware_sha256"]:
        raise ValueError("packed hardware changed")
    with np.load(args.products / "hardware.npz") as d:
        for key in d.files:
            if not np.array_equal(d[key], packed[key]):
                raise ValueError("packed geometry differs from actual pieces")
    display = verify_display(args, report, packed)
    maximum = {key: max(p[key] for p in proofs) for key in proofs[0]}
    original = json.loads((args.network / "comparison.json").read_text())
    clearance = (
        original["verification"]["whole_bridge_clearance"]["minimum_clearance_m"]
        - maximum["maximum_height_above_actual_bed_m"]
    )
    if clearance < 4.5 - 1e-7:
        raise ValueError("actual added hardware consumes the reserved bridge clearance")
    return {
        "verifier_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "report_sha256": digest(args.products / "comparison.json"),
        "pieces": len(products),
        "sleepers": len(packed["sleeper_transforms"]),
        "maximum_errors": maximum,
        "mutual": mutual,
        "preserved_base": True,
        "display": display,
        "conservative_hardware_to_bridge_clearance_m": clearance,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["products", "network", "baseline", "output"]:
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("fresh independent report required")
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    result = replay(args)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
