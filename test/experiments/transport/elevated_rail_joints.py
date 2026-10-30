"""Whole rail profiles at supplied vector endpoints, without invented routing IDs."""

import numpy as np
import shapely


def joints(rows):
    incidence = {}
    for i, row in enumerate(rows):
        nodes = row["original_mvt_xy"]
        for index, node in enumerate(nodes):
            incidence.setdefault(tuple(node), []).append((i, index))
    result = []
    for node, references in incidence.items():
        if len(references) == 1:
            continue
        if len(references) != 2 or len({i for i, _ in references}) != 2:
            raise ValueError("Only two-source whole rail endpoint joints supported")
        points, away, widths = [], [], []
        for i, index in references:
            row = rows[i]
            if index not in (0, len(row["xy"]) - 1):
                raise ValueError("Interior rail branching needs its own whole recipe")
            points.append(row["xy"][index])
            other = row["xy"][1 if index == 0 else -2]
            vector = other - points[-1]
            away.append(vector / np.linalg.norm(vector))
            widths.append(row["width_m"])
        if not np.array_equal(points[0], points[1]):
            raise ValueError(
                "Same supplied vector endpoint has different frame positions"
            )
        if away[0] @ away[1] > -np.cos(np.deg2rad(30)):
            raise ValueError("Rail endpoint turn exceeds bounded simple joint family")
        tangent = away[0] - away[1]
        tangent /= np.linalg.norm(tangent)
        normal = np.array([-tangent[1], tangent[0]])
        point = points[0]
        radius = min(widths) / 2
        section = np.array([point - radius * normal, point + radius * normal])
        corners = [*section]
        for axis, width in zip(away, widths, strict=True):
            n = np.array([-axis[1], axis[0]]) * width / 2
            corners.extend((point - n, point + n))
        result.append(
            {
                "indices": [i for i, _ in references],
                "mvt_xy": list(node),
                "xy": point,
                "section": section,
                "pad": shapely.convex_hull(shapely.multipoints(corners)),
            }
        )
    return result


def apply(rows):
    for joint in joints(rows):
        if joint["pad"].geom_type != "Polygon":
            continue
        coordinates = shapely.get_coordinates(joint["pad"])[:-1].tolist()
        for i in joint["indices"]:
            pads = rows[i].setdefault("junction_pads_xy_m", [])
            if not isinstance(pads, list) or len(pads) >= 8:
                raise ValueError("Bounded explicit rail endpoint pad list required")
            pads.append(coordinates)


def inspect(rows, actual):
    result = []
    for joint in joints(rows):
        section = shapely.LineString(joint["section"])
        if not actual.covers(section):
            raise ValueError("Whole shared rail endpoint cross-section is not covered")
        result.append(
            {
                "sources": [rows[i]["source_id"] for i in joint["indices"]],
                "supplied_mvt_xy": joint["mvt_xy"],
                "whole_profile_width_m": section.length,
                "uncovered_length_m": section.difference(actual).length,
                "basis": "Supplied generalized vector endpoint and explicit procedural physical pad; no original OSM node or routing/outer continuation authority.",
            }
        )
    return result
