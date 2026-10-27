"""Existing closed-prism builder plus independent actual floor-mesh checks."""

import numpy as np
import shapely
from building_mesh import solid
from indoor_floor_checks import floor_clearance
from indoor_floor_outlines import adapt_outlines
from indoor_floor_plan import FloorLimits, plan_floors
from indoor_mesh_checks import closed_mesh
from outshine_ref.contracts import HeightInterval, Reservation, SourceFeature


def mesh_floor(floor):
    storage = [], [], [], []
    solid(storage, floor.polygon, floor.lower_z_m, floor.upper_z_m, 0)
    vertices = np.asarray(storage[0], dtype=np.float64).reshape(-1, 3)
    faces = np.asarray(storage[1], dtype=np.uint32).reshape(-1, 3)
    parts = np.asarray(storage[3], dtype=np.uint32)
    checked = closed_mesh({"vertices": vertices, "faces": faces, "parts": parts})
    triangles = checked["triangles"]
    top = np.all(triangles[:, :, 2] == floor.upper_z_m, axis=1)
    cover = shapely.union_all(shapely.polygons(triangles[top, :, :2]))
    area_error = float(shapely.symmetric_difference(cover, floor.polygon).area)
    scale = max(1.0, float(abs(vertices[:, :2]).max()))
    numerical_area = 128 * np.spacing(scale) * floor.polygon.length
    if area_error > numerical_area:
        raise ValueError("floor_mesh_top_domain_mismatch")
    volume = checked["signed_volume_m3"]
    expected = floor.polygon.area * (floor.upper_z_m - floor.lower_z_m)
    volume_error = abs(volume - expected)
    numerical_volume = (
        256
        * np.finfo(float).eps
        * max(expected, checked["absolute_volume_products_m3"])
    )
    if volume <= 0 or volume_error > numerical_volume:
        raise ValueError("floor_mesh_volume_mismatch")
    return {"vertices": vertices, "faces": faces, "parts": parts}, {
        "triangles": len(faces),
        "vertices": len(vertices),
        "payload_bytes": vertices.nbytes + faces.nbytes + parts.nbytes,
        "top_domain_error_m2": area_error,
        "signed_volume_m3": volume,
        "volume_error_m3": volume_error,
        "closed_plate_parts": len(np.unique(parts)),
        "oriented_edge_pairs_within_plate_parts": checked["oriented_edge_pairs"],
        "scope": "Actual emitted separately closed plate parts and top domain, numerical arithmetic model. Touching parts keep separate face ownership; a shared floor label supplies no route connection. No terrain, room/door/route, structural-statical or native World/device certificate.",
    }


def produce_floor_contacts(
    parts, requests, ground, *, limits=None, method="structural", constraints=None
):
    limits = FloorLimits() if limits is None else limits
    if method == "adapted":
        if constraints is None:
            raise ValueError(
                "Fixed source constraints required for footprint adaptation"
            )
        parts, adjustment = adapt_outlines(parts, constraints, limits)
        selected_method = "constrained"
    else:
        adjustment, selected_method = {"adjustments": []}, method
    result = plan_floors(
        parts,
        requests,
        ground,
        limits=limits,
        method=selected_method,
        constraints=constraints,
    )
    meshes, audits, failures = {}, {}, {}
    volumes = {p.identifier: p for p in parts}
    bases = {tuple(p["source_ids"]): p["base_z_m"] for p in result["programmes"]}
    for floor in result["floors"]:
        try:
            meshes[floor.identifier], audits[floor.identifier] = mesh_floor(floor)
            audits[floor.identifier]["source_height_clearance"] = floor_clearance(
                floor,
                meshes[floor.identifier],
                tuple(volumes[s] for s in floor.source_ids),
                bases[floor.source_ids],
                limits,
            )
        except ValueError as error:
            failures[floor.identifier] = str(error)
            meshes.pop(floor.identifier, None)
    originals = {r.identifier: r for r in requests}
    reservations = []
    for row in result["choices"]:
        request = originals[row["source_id"]]
        row.update(original=request.original, frame=request.frame.identifier)
        if row["status"] == "ready" and row["floor_id"] in failures:
            row.update(status="pending", reason=failures[row["floor_id"]])
            row.pop("xyz")
            row.pop("floor_id")
        if row["status"] == "ready":
            row.update(
                contact_kind="chosen_procedural_building_floor",
                qualification="source-snapshot-floor-and-plate-only",
            )
            z = float(row["xyz"][0, 2])
            reservations.append(
                Reservation(
                    request.identifier,
                    request.corridor,
                    HeightInterval(z, z + limits.headroom_m),
                    "traffic_clearance",
                )
            )
    return result | {
        "meshes": meshes,
        "mesh_audits": audits,
        "mesh_failures": failures,
        "adjusted_volumes": tuple(parts),
        "outline_adaptation": adjustment,
        "frame": requests[0].frame if requests else None,
        "reservations": tuple(reservations),
        "source_features": tuple(
            SourceFeature(r.identifier, r.axis, r.original.get("properties", {}))
            for r in requests
        ),
    }
