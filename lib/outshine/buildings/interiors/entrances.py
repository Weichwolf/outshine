"""The shared first-storey entrance selector, with bounded existing-cell candidates."""

import math

import numpy as np
import shapely

from .geometry import edges, rectangle


def owner(rooms, centre, tangent):
    normal = np.array([tangent[1], -tangent[0]])
    point = shapely.Point(np.asarray(centre) - normal * (0.24 + 0.12 / 2 + 0.16))
    return next((i for i, room in enumerate(rooms) if room.covers(point)), None)


def aperture_fits(room, centre, tangent, width):
    centre, tangent = np.asarray(centre), np.asarray(tangent)
    normal = np.array([tangent[1], -tangent[0]])
    middle = centre - normal * (0.24 + 0.12 / 2 + 0.16)
    return room.covers(
        shapely.LineString([middle - tangent * width / 2, middle + tangent * width / 2])
    )


def entrance(
    polygon, rooms, context, *, fractions_by_edge=None, accept=None, max_candidates=1024
):
    if (
        not isinstance(polygon, shapely.Polygon)
        or not polygon.is_valid
        or polygon.is_empty
        or polygon.has_z
    ):
        raise ValueError("A valid local XY source polygon is required")
    if not np.isfinite(shapely.get_coordinates(polygon)).all():
        raise ValueError("Finite source coordinates required")
    if len(polygon.exterior.coords) - 1 > 256:
        raise ValueError("Entrance source exceeds 256 exterior edges")
    if type(max_candidates) is not int or not 1 <= max_candidates <= 4096:
        raise ValueError("Bounded entrance candidate capacity required")
    boundary = edges(polygon, holes=False)
    if fractions_by_edge is not None and (
        not isinstance(fractions_by_edge, dict)
        or any(
            type(i) is not int or not 0 <= i < len(boundary) for i in fractions_by_edge
        )
    ):
        raise ValueError("Existing exterior edge identities required")
    if accept is not None and not callable(accept):
        raise ValueError("Entrance acceptance must be callable")
    fractions = [
        (
            fractions_by_edge.get(i, ())
            if fractions_by_edge is not None
            else (0.5, 0.25, 0.75)
        )
        for i in range(len(boundary))
    ]
    if any(
        not isinstance(row, tuple)
        or len(row) > 32
        or len(set(row)) != len(row)
        or any(
            isinstance(f, bool) or not math.isfinite(f) or not 0 < f < 1 for f in row
        )
        for row in fractions
    ):
        raise ValueError("Unique finite immutable edge fractions in (0,1) required")
    if sum(map(len, fractions)) > max_candidates:
        raise ValueError("Entrance candidates exceed capacity before expansion")
    candidates = []
    frontage = [
        shapely.Point(row["position_xy"])
        for row in context.get("evidence", [])
        if row.get("usable_frontage")
    ]
    for index, ((a, b), positions) in enumerate(zip(boundary, fractions, strict=True)):
        width = np.linalg.norm(b - a)
        if width < 1.10 + 0.8:
            continue
        tangent = (b - a) / width
        normal = np.array([tangent[1], -tangent[0]])
        street = [
            p
            for p in frontage
            if (np.array(p.coords[0]) - (a + b) / 2) @ normal > 0
            and shapely.LineString([a, b]).distance(p) <= 12.0
        ]
        for fraction in positions:
            if min(fraction, 1 - fraction) * width < 1.10 / 2 + 0.30:
                continue
            centre = a + (b - a) * fraction
            room = owner(rooms, centre, tangent)
            if room is None or not aperture_fits(rooms[room], centre, tangent, 1.10):
                continue
            opening = rectangle(centre, tangent, 1.10, 0.12, 0.24 + 0.12 / 2 + 0.20)
            landing = rectangle(centre, tangent, 1.80, 1.20, 0.06)
            if landing.intersection(polygon).area > 0.12:
                continue
            rank = (
                not bool(street),
                min((shapely.Point(centre).distance(p) for p in street), default=0.0),
                -width,
                index,
                fraction,
            )
            door = {
                "id": "entrance",
                "rooms": [room],
                "polygon": opening,
                "centre": centre.tolist(),
                "tangent": tangent.tolist(),
                "height_m": 2.15,
                "kind": "entrance",
                "landing": landing,
                "origin": "context-frontage"
                if street
                else "procedural longest usable facade",
            }
            if accept is None or accept(door):
                candidates.append((rank, door))
    return min(candidates, key=lambda row: row[0])[1] if candidates else None
