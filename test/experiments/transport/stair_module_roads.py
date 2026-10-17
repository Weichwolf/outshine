"""Explicit minor-road assumptions and bounded longitudinal/crossfall frames."""

import numpy as np


def road_profile(source, profile, kind):
    props = source["properties"]
    if kind != "unclassified" or props.get("class") != "minor" or props.get("subclass"):
        raise ValueError("explicit ordinary minor-road interpretation required")
    oneway = props.get("oneway", 0)
    if oneway not in (-1, 0, 1):
        raise ValueError("known source direction required")
    lanes = profile["lanes"]
    if lanes != 2:
        raise ValueError("two-lane procedural reference profile required")
    return {
        "source_recipe": "minor_ground_carriageway",
        "profile_kind": kind,
        "crossfall_limit": 0.03,
        "traffic_profile": {
            "width_m": profile["widthM"],
            "lane_width_m": profile["widthM"] / lanes,
            "forward_lanes": 0 if oneway == -1 else lanes if oneway == 1 else 1,
            "reverse_lanes": 0 if oneway == 1 else lanes if oneway == -1 else 1,
            "clearance_m": profile["clearanceM"],
            "speed_mps": profile["speedMps"],
            "source_oneway": props.get("oneway"),
            "origin": "Width, lanes, clearance, speed and crossfall are procedural assumptions; missing direction tags use a procedural two-way default. Original class and any oneway tag retained.",
        },
    }


def tangents(row, points):
    xy = np.asarray(row.get("surface_axis_xy_m", row["xy"]))
    origin = xy[:-1]
    axis = np.diff(xy, axis=0)
    norm2 = np.einsum("ij,ij->i", axis, axis)
    keep = norm2 > 0
    origin, axis, norm2 = origin[keep], axis[keep], norm2[keep]
    if not len(axis) or len(points) * len(axis) > 2000000:
        raise ValueError("bounded nonempty original road-axis segments required")
    delta = points[:, None] - origin
    fraction = np.clip(np.einsum("pij,ij->pi", delta, axis) / norm2, 0, 1)
    residual = delta - fraction[:, :, None] * axis
    chosen = np.argmin(np.einsum("pij,pij->pi", residual, residual), axis=1)
    return axis[chosen] / np.sqrt(norm2[chosen, None])


def face_frames(rows, xy, faces, owners, *, reserve=0):
    center = xy[faces].mean(axis=1)
    along = np.min(np.where(owners, [r["gradient"] for r in rows], np.inf), axis=1)
    cross = np.min(
        np.where(
            owners, [r.get("crossfall_limit", r["gradient"]) for r in rows], np.inf
        ),
        axis=1,
    )
    limit = np.minimum(along, cross)
    if np.any(limit <= reserve):
        raise ValueError(
            "positive source profile limits after numeric reserve required"
        )
    result = np.eye(2)[None] / (limit[:, None, None] - reserve)
    single = owners.sum(axis=1) == 1
    for i, row in enumerate(rows):
        selected = single & owners[:, i]
        if "crossfall_limit" not in row or not selected.any():
            continue
        axis = tangents(row, center[selected])
        result[selected, 0] = axis / (row["gradient"] - reserve)
        result[selected, 1] = np.c_[-axis[:, 1], axis[:, 0]] / (
            row["crossfall_limit"] - reserve
        )
    return result


def inspect_frames(data, rows, cast):
    from stair_module_materials import surface_slots
    from surface_height_checks import normal_gradients

    vertices = (
        data["surface_vertices"].astype("f4").astype(float)
        if cast
        else data["surface_vertices"]
    )
    faces, owners = data["surface_faces"], data["owners"]
    if not np.array_equal(data["surface_materials"], surface_slots(rows, owners)):
        raise ValueError(
            "actual surface material lost original source profile ownership"
        )
    nominal = face_frames(rows, data["surface_vertices"][:, :2], faces, owners)
    expected = face_frames(
        rows, data["surface_vertices"][:, :2], faces, owners, reserve=0.0002
    )
    if not np.array_equal(expected, data["grade_frames"]):
        raise ValueError("source-derived anisotropic profile frames changed")
    gradients = normal_gradients(vertices[:, :2], faces, vertices[:, 2])
    maximum = float(
        np.linalg.norm(np.einsum("fij,fj->fi", nominal, gradients), axis=1).max()
    )
    if maximum > 1:
        raise ValueError("actual road longitudinal/crossfall frame exceeded")
    reports = []
    for i, row in enumerate(rows):
        if "crossfall_limit" not in row:
            continue
        selected = owners[:, i]
        axis = tangents(row, data["surface_vertices"][faces[selected], :2].mean(axis=1))
        normal = np.c_[-axis[:, 1], axis[:, 0]]
        reports.append(
            {
                "source_id": row["source_id"],
                "maximum_longitudinal_grade": float(
                    abs(np.einsum("ij,ij->i", axis, gradients[selected])).max()
                ),
                "maximum_crossfall": float(
                    abs(np.einsum("ij,ij->i", normal, gradients[selected])).max()
                ),
            }
        )
    return {
        "float32": cast,
        "maximum_normalized_profile_grade": maximum,
        "roads": reports,
    }
