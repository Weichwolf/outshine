"""Bounded compact support pods sharing the actual common corridor wall."""

import numpy as np
import shapely
from building_layout import (
    AGENT_RADIUS_M,
    DOOR_M,
    PARTITION_M,
    canonical_geometry,
    room_portals,
)
from building_plan import aperture_fits, edges

DIMENSIONS = {
    "sanitary": ((2.7, 2.4), (3.0, 2.1), (2.4, 2.7), (2.1, 3.0), (1.8, 3.6)),
    "tea": (
        (3.0, 2.6),
        (3.6, 2.1),
        (2.6, 3.0),
        (2.4, 3.2),
        (2.1, 3.6),
        (2.1, 2.6),
        (1.8, 3.6),
    ),
}
MAX_CANDIDATES = 48


def window_owners(current, rooms):
    result = []
    for window in current["windows"]:
        centre = np.asarray(window["centre"])
        tangent = np.asarray(window["tangent"])
        normal = np.array([-tangent[1], tangent[0]])
        point = centre + normal * 0.46
        width = float(np.ptp(np.asarray(window["polygon"].exterior.coords) @ tangent))
        matches = [
            i
            for i, room in enumerate(rooms)
            if room.covers(shapely.Point(point))
            and aperture_fits(room, centre, tangent, width)
        ]
        if len(matches) != 1:
            raise ValueError(
                "A support partition crosses or disconnects an existing window"
            )
        result.append(window | {"rooms": matches})
    return result


def retained_approaches(current, index):
    room = current["rooms"][index]
    result = []
    for door in current["doors"]:
        if index not in door["rooms"]:
            continue
        centre = np.asarray(door["centre"])
        tangent = np.asarray(door["tangent"])
        normal = np.array([-tangent[1], tangent[0]])
        inside = next(
            (
                centre + sign * normal * 0.55
                for sign in (-1, 1)
                if room.covers(shapely.Point(centre + sign * normal * 0.55))
            ),
            None,
        )
        if inside is None:
            raise ValueError("Existing room door has no retained room-side approach")
        result.append(
            shapely.LineString([centre, inside]).buffer(0.60, cap_style="square")
        )
    return result


def split(current, index, zone, corridor_line, kind):
    original = current["rooms"][index]
    remainder = original.difference(zone)
    shared = zone.boundary.intersection(remainder.boundary)
    wall = shared.buffer(PARTITION_M / 2, cap_style="flat", join_style="mitre")
    child = canonical_geometry(zone.difference(wall))
    remaining = canonical_geometry(remainder.difference(wall))
    if any(r.geom_type != "Polygon" or r.is_empty for r in (child, remaining)):
        raise ValueError("Support pod leaves a disconnected floor")
    if remaining.area < 8.0:
        raise ValueError("Support pod consumes the remaining usable parent room")
    for room in (child, remaining):
        clear = room.buffer(-AGENT_RADIUS_M, join_style="mitre")
        if clear.geom_type != "Polygon" or clear.is_empty:
            raise ValueError("Support partition loses full-radius connected space")
    if any(zone.intersects(r) for r in retained_approaches(current, index)):
        raise ValueError("Support pod replaces an existing door approach")
    rooms = list(current["rooms"])
    rooms[index] = remaining
    rooms.append(child)
    doors = room_portals(rooms, [(0, len(rooms) - 1, [corridor_line])])
    if len(doors) != 1:
        raise ValueError(
            "Support pod lacks an independent full-width common-corridor door"
        )
    windows = window_owners(current, rooms)
    if kind == "sanitary" and any(len(rooms) - 1 in w["rooms"] for w in windows):
        raise ValueError(
            "Sanitary privacy requires no unmodified clear exterior window"
        )
    had_daylight = any(index in w["rooms"] for w in current["windows"])
    if had_daylight and not any(index in w["rooms"] for w in windows):
        raise ValueError(
            "Support pod removes all daylight from its remaining parent room"
        )
    return rooms, doors[0], windows


def candidates(current, index, kind, method):
    if kind not in DIMENSIONS or method not in ("wall-ends", "sliding-wall"):
        raise ValueError("Unknown support room programme or partition method")
    host = current["rooms"][index]
    corridor = current["rooms"][0]
    if len(current["rooms"]) >= 24:
        return [], [
            {
                "host": index,
                "kind": kind,
                "reason": "Support space exceeds the 24-space floor capacity",
            }
        ]
    if current["room_kinds"][index] != "generic-room":
        return [], []
    fractions = (
        (0.0, 1.0, 0.5) if method == "wall-ends" else tuple(np.linspace(0, 1, 7))
    )
    accepted, rejected = [], []
    count = 0
    boundary = host.simplify(1e-7, preserve_topology=True)
    for a, b in sorted(
        edges(boundary), key=lambda e: (-np.linalg.norm(e[1] - e[0]), tuple(e[0]))
    ):
        length = float(np.linalg.norm(b - a))
        tangent = (b - a) / length
        normal = np.array([-tangent[1], tangent[0]])
        probe = shapely.LineString([a - normal * 0.5, b - normal * 0.5])
        if probe.intersection(corridor).length < DOOR_M + 0.4:
            continue
        for width, depth in DIMENSIONS[kind]:
            if width > length - 0.12:
                continue
            for fraction in fractions:
                if count >= MAX_CANDIDATES:
                    return accepted, rejected
                count += 1
                start = a + tangent * (0.06 + fraction * (length - width - 0.12))
                zone = shapely.Polygon(
                    [
                        start,
                        start + tangent * width,
                        start + tangent * width + normal * depth,
                        start + normal * depth,
                    ]
                )
                try:
                    if zone.difference(host).area > 1e-7:
                        raise ValueError("Support pod escapes the whole parent polygon")
                    zone = canonical_geometry(zone.intersection(host))
                    line = shapely.LineString(
                        [
                            start - normal * PARTITION_M / 2,
                            start + tangent * width - normal * PARTITION_M / 2,
                        ]
                    )
                    rooms, door, windows = split(current, index, zone, line, kind)
                    child = rooms[-1]
                    target = 6.0 if kind == "sanitary" else 7.5
                    score = abs(child.area - target) + 0.03 * child.length
                    accepted.append(
                        {
                            "score": score,
                            "host": index,
                            "kind": kind,
                            "rooms": rooms,
                            "door": door,
                            "windows": windows,
                            "candidate": count,
                            "child": child,
                        }
                    )
                except ValueError as error:
                    rejected.append({"host": index, "kind": kind, "reason": str(error)})
    return accepted, rejected
