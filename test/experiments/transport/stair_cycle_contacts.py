"""Whole original endpoint profiles on individual actual shared cap components."""

import numpy as np
import shapely

CONTACTS = ((4, 15, -1), (6, 17, 0), (17, 18, 0), (17, 18, -1))


def profile(row, end):
    xy = row["xy"]
    tangent = xy[1] - xy[0] if end == 0 else xy[-1] - xy[-2]
    tangent = tangent / np.linalg.norm(tangent)
    normal = np.array([-tangent[1], tangent[0]])
    return xy[end] + np.array([-1, 0, 1])[:, None] * row["width_m"] / 2 * normal


def contacts(data, rows):
    vertices = data["body_vertices"]
    result = []
    for owner, neighbour, end in CONTACTS:
        selected = ((data["face_sources"] & (1 << owner)) != 0) & (
            (data["face_sources"] & (1 << neighbour)) != 0
        )
        faces = data["top_faces"][selected]
        if not len(faces):
            raise ValueError("actual coowned neighbour cap missing")
        polygons = shapely.polygons(vertices[faces, :2])
        components = shapely.get_parts(shapely.union_all(polygons))
        point = shapely.Point(rows[owner]["xy"][end])
        near = [part for part in components if part.distance(point) < 1e-7]
        if len(near) != 1:
            raise ValueError("endpoint lacks exactly one actual shared component")
        faces = faces[shapely.dwithin(shapely.centroid(polygons), near[0], 1e-8)]
        xy = profile(rows[owner], end)
        checks = []
        for quantized in (False, True):
            actual = (
                vertices.astype(np.float32).astype(float) if quantized else vertices
            )
            line = xy.astype(np.float32).astype(float) if quantized else xy
            cap = actual[faces]
            if np.ptp(cap[:, :, 2]) > 1e-7:
                raise ValueError("whole endpoint cap is not flat")
            cover = shapely.union_all(shapely.polygons(cap[:, :, :2]))
            guard = (
                2 * float(abs(np.spacing(line.astype(np.float32))).max())
                if quantized
                else 1e-7
            )
            missing = shapely.LineString(line).difference(cover.buffer(guard)).length
            if missing > 1e-7:
                raise ValueError("actual shared cap misses a whole endpoint profile")
            checks.append(
                {
                    "float32": quantized,
                    "height_m": float(cap[0, 0, 2]),
                    "xy_guard_m": guard,
                    "uncovered_length_m": float(missing),
                }
            )
        result.append(
            {
                "source_ids": [rows[i]["source_id"] for i in (owner, neighbour)],
                "end": end,
                "profile_xy_m": xy.tolist(),
                "whole_cap_checks": checks,
            }
        )
    return result
