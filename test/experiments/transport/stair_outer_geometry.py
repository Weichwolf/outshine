"""Full actual former-port contacts and the remaining unbound source endpoints."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_junction_layout import landings


def outer_ends(rows, nodes):
    domains = [corridor(r) for r in rows]
    _points, owners, masks = landings(rows, domains, "width", 0.75, nodes)
    result = []
    for i, row in enumerate(rows):
        for end in (0, -1):
            point = shapely.Point(row["xy"][end])
            joined = any(
                i in ids
                and mask.distance(point) < 1e-7
                and any(domains[j].distance(point) < 1e-7 for j in ids if j != i)
                for ids, mask in zip(owners, masks, strict=True)
            )
            if not joined:
                result.append((i, end))
    return result


def bound_contacts(data, rows):
    result = []
    vertices = data["body_vertices"]
    for owner, other, end in ((4, 15, -1), (6, 17, 0)):
        row = rows[owner]
        tangent = (
            row["xy"][1] - row["xy"][0] if end == 0 else row["xy"][-1] - row["xy"][-2]
        )
        tangent /= np.linalg.norm(tangent)
        normal = np.array([-tangent[1], tangent[0]])
        xy = (
            row["xy"][end] + np.array([-1, 0, 1])[:, None] * row["width_m"] / 2 * normal
        )
        selected = ((data["face_sources"] & (1 << owner)) != 0) & (
            (data["face_sources"] & (1 << other)) != 0
        )
        faces = data["top_faces"][selected]
        if not len(faces):
            raise ValueError("former port lacks an actual shared whole cap")
        errors, heights = [], []
        for cast in (False, True):
            actual = vertices.astype(np.float32).astype(float) if cast else vertices
            points = xy.astype(np.float32).astype(float) if cast else xy
            cap = actual[faces]
            if np.ptp(cap[:, :, 2]) > 1e-7:
                raise ValueError("former-port shared cap is not flat")
            cover = shapely.union_all(shapely.polygons(cap[:, :, :2]))
            guard = (
                float(2 * abs(np.spacing(points.astype(np.float32))).max())
                if cast
                else 1e-7
            )
            error = float(
                shapely.LineString(points).difference(cover.buffer(guard)).length
            )
            if error > 1e-7:
                raise ValueError(
                    "actual neighbour does not cover the whole former-port profile"
                )
            errors.append(
                {"quantized": cast, "xy_guard_m": guard, "uncovered_length_m": error}
            )
            heights.append(float(cap[0, 0, 2]))
        result.append(
            {
                "source_ids": [rows[i]["source_id"] for i in (owner, other)],
                "profile_xy_m": xy.tolist(),
                "heights_double_float32_m": heights,
                "whole_profile_coverage": errors,
                "status": "bound_to_actual_selected_neighbour_cap; terrain_and_remaining_context_not_admitted",
            }
        )
    return result
