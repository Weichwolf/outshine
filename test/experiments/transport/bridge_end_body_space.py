"""Existing affine cap envelopes, either per triangle or over a qualified deck."""

import numpy as np
from mvt_body_occupancy import body_occupancy
from stair_occupancy_geometry import AffineOccupancy
from stair_occupancy_inputs import outward_plane


def occupied_space(
    surface, body, domain, owner, source_id, clearance, method, maximum_volumes
):
    count, faces = len(surface.vertices), len(surface.triangles)
    if method == "faces":
        data = {
            "body_vertices": body.vertices,
            "top_faces": body.triangles[:faces],
            "bottom_faces": body.triangles[faces : 2 * faces],
            "face_sources": np.ones(faces, dtype=np.uint32),
        }
        rows, origins, records, work = body_occupancy(
            data,
            [source_id],
            {source_id: clearance},
            owner_id=owner,
            max_volumes=maximum_volumes,
        )
        return rows, origins, records, work | {"method": method}
    if method != "whole":
        raise ValueError("Known deck occupancy method required")
    if maximum_volumes < 1:
        raise ValueError("Deck occupancy budget exhausted")
    triangles = surface.vertices[surface.triangles]
    projected = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )[:, 2]
    # Fit the largest projected triangle, then certify *all* supplied corners.
    # Exact outward_plane checks cap padding at the existing 1e-8m contract;
    # unrepresentable/non-affine decks fail without changing their geometry.
    selected = surface.triangles[int(projected.argmax())]
    points = surface.vertices[selected]
    origin = tuple(map(float, points[0, :2]))
    gradient = np.linalg.solve(points[1:, :2] - origin, points[1:, 2] - points[0, 2])
    planes, errors = [], []
    for vertices, delta, sign in (
        (body.vertices[count:], 0.0, -1),
        (body.vertices[:count], clearance, 1),
    ):
        plane, error = outward_plane(
            vertices,
            origin,
            (
                float(vertices[int(selected[0]), 2] + delta),
                *map(float, gradient),
            ),
            delta,
            sign,
        )
        planes.append(plane)
        errors.append(error)
    row = AffineOccupancy(
        owner + ":whole",
        domain,
        "transport.body-and-headroom" if clearance else "transport.support",
        origin,
        *planes,
    )
    return (
        (row,),
        ((source_id,),),
        [{"source_ids": (source_id,), "clearance_m": clearance}],
        {
            "method": method,
            "actual_cap_pairs": faces,
            "source_face_memberships": faces,
            "emitted_volumes": 1,
            "plane_fits": 1,
            "maximum_outward_height_padding_m": max(errors),
            "exact_cap_vertices_checked": 2 * count,
            "indexed_cap_coordinate_bytes": triangles.nbytes,
            "scope": "One conservative whole-domain envelope on unchanged indexed caps; exact affine corner coverage implies whole triangle coverage. Existing query/topology limits retained; no general World/clearance admission. Indexed array gather bytes are not total copied bytes.",
        },
    )
