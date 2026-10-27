"""A closed extrusion of an explicit flight/tread/landing/waist profile."""

from itertools import pairwise

import numpy as np
import shapely
from building_mesh import solid
from indoor_mesh_checks import closed_mesh
from indoor_stair_recipe import profile


def stair_mesh(axis, width_m, low_z_m, high_z_m, recipe, limits):
    points = np.asarray(axis.coords)
    if (
        len(points) != 2
        or not np.isfinite((width_m, low_z_m, high_z_m)).all()
        or width_m <= 0
    ):
        raise ValueError("Explicit straight stair axis/width/end heights required")
    delta = points[1] - points[0]
    length = float(np.linalg.norm(delta))
    if length <= 0 or high_z_m <= low_z_m:
        raise ValueError("Ascending physical stair geometry required")
    tangent = delta / length
    normal = np.array([-tangent[1], tangent[0]])
    top, soffit, outline = profile(length, high_z_m - low_z_m, recipe, limits)
    width = width_m + 2 * limits.clearance_m
    storage = [], [], [], []
    solid(storage, outline, -width / 2, width / 2, 0)
    raw = np.asarray(storage[0], dtype=np.float64)
    xy = points[0] + raw[:, :1] * tangent + raw[:, 2:3] * normal
    vertices = np.c_[xy, low_z_m + raw[:, 1]]
    # (longitudinal, vertical, lateral) -> (east, north, up) reverses orientation.
    faces = np.asarray(storage[1], dtype=np.uint32)[:, [0, 2, 1]]
    mesh = {
        "vertices": vertices,
        "faces": faces,
        "parts": np.asarray(storage[3], dtype=np.uint32),
    }
    checked = closed_mesh(mesh)
    expected = outline.area * width
    error = abs(checked["signed_volume_m3"] - expected)
    numerical = (
        128
        * np.spacing(max(1.0, float(abs(vertices).max())))
        * checked["surface_area_m2"]
    )
    if error > numerical:
        raise ValueError("stair_mesh_volume_mismatch")
    surfaces = []
    for first, last in pairwise(top):
        if first[1] != last[1] or last[0] <= first[0]:
            continue
        along = np.array([first[0], last[0], last[0], first[0]])
        across = np.array([-width / 2, -width / 2, width / 2, width / 2])
        polygon = shapely.Polygon(
            points[0] + along[:, None] * tangent + across[:, None] * normal
        )
        surfaces.append(
            {
                "polygon": polygon,
                "z_m": low_z_m + first[1],
                "start_m": float(first[0]),
                "end_m": float(last[0]),
                "body_bottom_z_m": low_z_m
                + float(np.interp(first[0], soffit[:, 0], soffit[:, 1])),
                "minimum_vertical_waist_m": first[1]
                - float(np.interp(last[0], soffit[:, 0], soffit[:, 1])),
            }
        )
    upward = checked["triangles"][checked["crosses"][:, 2] > 0]
    top_error = 0.0
    for z in sorted({s["z_m"] for s in surfaces}):
        actual = upward[np.all(upward[:, :, 2] == z, axis=1)]
        actual_domain = shapely.union_all(shapely.polygons(actual[:, :, :2]))
        expected_domain = shapely.union_all(
            [s["polygon"] for s in surfaces if s["z_m"] == z]
        )
        top_error += actual_domain.symmetric_difference(expected_domain).area
    arithmetic_area = (
        128
        * np.spacing(max(1.0, float(abs(vertices[:, :2]).max())))
        * sum(s["polygon"].length for s in surfaces)
    )
    if top_error > arithmetic_area:
        raise ValueError("emitted_stair_treads_mismatch")
    return (
        mesh,
        surfaces,
        {
            "vertices": len(vertices),
            "triangles": len(faces),
            "payload_bytes": sum(v.nbytes for v in mesh.values()),
            "oriented_edge_pairs": checked["oriented_edge_pairs"],
            "signed_volume_m3": checked["signed_volume_m3"],
            "volume_error_m3": error,
            "volume_arithmetic_bound_m3": float(numerical),
            "constructed_width_m": width,
            "minimum_vertical_waist_m": float(
                min(s["minimum_vertical_waist_m"] for s in surfaces)
            ),
            "emitted_tread_area_error_m2": top_error,
            "tread_arithmetic_area_bound_m2": float(arithmetic_area),
            "scope": "Actual oriented closed waist extrusion; end/landing/tread/headroom and floor-opening checks are separate obligations.",
        },
    )
