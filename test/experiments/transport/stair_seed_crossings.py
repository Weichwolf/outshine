"""Bounded procedural road/footway crossings before either surface is frozen."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_module_roads import tangents
from stair_module_through import through_sides
from stair_module_trace import compare
from stair_surface_access import contact_access


def crossing_point(source, other):
    lines = []
    for selected in (source, other):
        xy = np.asarray(selected["xy_m"], float)
        if xy.ndim != 2 or xy.shape[1] != 2 or not 2 <= len(xy) <= 256:
            raise ValueError("bounded original crossing axes required")
        if not np.isfinite(xy).all():
            raise ValueError("finite original crossing axes required")
        line = shapely.LineString(xy)
        if not line.is_simple or line.length == 0:
            raise ValueError("simple original crossing axes required")
        lines.append(line)
    point = lines[0].intersection(lines[1])
    if not lines[0].crosses(lines[1]) or point.geom_type != "Point":
        raise ValueError("one transverse original-axis crossing required")
    if set(map(tuple, source["mvt_xy"])) & set(map(tuple, other["mvt_xy"])):
        raise ValueError(
            "original shared nodes must use the original junction contract"
        )
    return np.asarray(point.coords[0])


def prior_at(row, point):
    xy = np.asarray(row["xy"])
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xy, axis=0), axis=1))]
    return float(
        np.interp(
            shapely.LineString(xy).project(shapely.Point(point)), arc, row["prior"]
        )
    )


def crossings_for(rows, sources, recipe):
    declarations = recipe.get("geometric_junctions", [])
    if not isinstance(declarations, list) or len(declarations) > 4:
        raise ValueError("at most four explicit geometric seed junctions required")
    if declarations and (
        recipe.get("contact_mode") != "seed"
        or recipe.get("junction_mode") != "level_box"
    ):
        raise ValueError("geometric seed junctions require joint level-box planning")
    if declarations and not isinstance(sources, dict):
        raise ValueError("original sources required for geometric seed junctions")
    owned = {row["source_id"]: i for i, row in enumerate(rows)}
    result, seen = [], set()
    for declaration in declarations:
        if not isinstance(declaration, dict) or set(declaration) != {
            "source_id",
            "footway_source_id",
            "kind",
            "modes",
            "access_assumption",
        }:
            raise ValueError("complete explicit road/footway crossing binding required")
        key, foot = declaration["source_id"], declaration["footway_source_id"]
        if (
            not isinstance(key, str)
            or not isinstance(foot, str)
            or key == foot
            or key not in owned
            or foot not in owned
            or (key, foot) in seen
            or (foot, key) in seen
        ):
            raise ValueError("distinct unique whole new crossing sources required")
        if declaration["kind"] not in (
            "minor_footway_crossing",
            "footway_footway_crossing",
        ):
            raise ValueError("known explicit geometric ground crossing required")
        road_row, foot_row = rows[owned[key]], rows[owned[foot]]
        if declaration["kind"] == "minor_footway_crossing" and (
            road_row.get("profile_kind") != "unclassified"
            or road_row.get("source_recipe") != "minor_ground_carriageway"
        ):
            raise ValueError("whole carriageway profile required at the crossing")
        point = crossing_point(sources[key], sources[foot])
        access = contact_access(declaration, sources[key], sources[foot], foot_row)
        reach = max(road_row["width_m"], foot_row["width_m"]) / 2 + 0.75
        result.append(
            {
                "source_id": key,
                "footway_source_id": foot,
                "owners": [owned[key], owned[foot]],
                "xy_m": point.tolist(),
                "shape": shapely.box(*(point - reach), *(point + reach)),
                "height_m": (prior_at(road_row, point) + prior_at(foot_row, point)) / 2,
                "mvt_xy": None,
                "access": access,
            }
        )
        seen.add((key, foot))
    return result


def crossing_profiles(data, contact, rows):
    result = []
    for owner in contact["owners"]:
        row = rows[owner]
        point = np.asarray(contact["xy_m"])
        tangent = tangents(row, point[None])[0]
        normal = np.array([-tangent[1], tangent[0]])
        line = point + np.array([-1, 0, 1])[:, None] * row["width_m"] / 2 * normal
        caps = [
            (
                data["surface_vertices"],
                data["surface_faces"][data["owners"][:, owner]],
                None,
            )
        ]
        sides = through_sides(caps, line)
        result.append(
            {
                "source_id": row["source_id"],
                "width_m": row["width_m"],
                "profile_xy_m": line.tolist(),
                "checks": [compare(sides, line, cast) for cast in (False, True)],
            }
        )
    return result


def inspect_crossings(data, contacts, rows):
    reports = []
    original = [
        corridor({k: v for k, v in row.items() if k != "source_grid_m"}) for row in rows
    ]
    grid = rows[0].get("source_grid_m")
    for contact in contacts:
        a, b = contact["owners"]
        selected = data["owners"][:, a] & data["owners"][:, b]
        expected = original[a].intersection(original[b])
        proofs = []
        for cast in (False, True):
            vertices = (
                data["surface_vertices"].astype("f4").astype(float)
                if cast
                else data["surface_vertices"]
            )
            actual = shapely.union_all(
                shapely.polygons(vertices[data["surface_faces"][selected], :2])
            )
            if (
                not actual.is_valid
                or actual.geom_type != "Polygon"
                or expected.geom_type != "Polygon"
                or actual.area <= 1e-7
                or len(actual.interiors) != len(expected.interiors)
            ):
                raise ValueError("whole coowned geometric crossing coverage differs")
            if grid:
                if not actual.covers(expected) or any(
                    not original[i].buffer(6 * grid, quad_segs=16).covers(actual)
                    for i in (a, b)
                ):
                    raise ValueError("whole coowned crossing exceeds its source models")
            elif actual.symmetric_difference(expected).area > 1e-7:
                raise ValueError("whole coowned geometric crossing coverage differs")
            proofs.append(
                {
                    "float32": cast,
                    "coowned_faces": int(selected.sum()),
                    "area_m2": float(actual.area),
                    "whole_original_overlap_m2": float(expected.area),
                    "individual_source_outward_bound_m": 6 * grid if grid else 0.0,
                }
            )
        reports.append(
            {
                "source_id": contact["source_id"],
                "footway_source_id": contact["footway_source_id"],
                "procedural_axis_crossing_xy_m": contact["xy_m"],
                "access": contact["access"],
                "checks": proofs,
                "through_profiles": crossing_profiles(data, contact, rows),
            }
        )
    return reports
