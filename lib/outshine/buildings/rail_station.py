"""Infer elevated station openings from continuous rail and mapped platforms."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from shapely.ops import substring

from outshine.transport.elevated_rail import (
    BALLAST_DEPTH,
    BALLAST_WIDTH,
    track_centreline,
)
from outshine.transport.rail_approaches import rail_neighbors
from outshine.transport.rail_hardware import build_rail_hardware

PORTAL_HALF_WIDTH = 1.95
STATION_CLEARANCE = 4.5
MINIMUM_LINTEL = 0.35
CONTACT_TOLERANCE = 2e-6


@dataclass(frozen=True)
class StationHall:
    openings: shapely.Geometry
    ballast: shapely.Geometry
    floor: float
    ceiling: float


def rail_inside_station(raw, track, end, outline, neighbors):
    mouth = shapely.Point(track.coords[end])
    outward = np.subtract(track.coords[end], track.coords[1 if end == 0 else -2])
    at = raw.project(mouth)
    candidates = [substring(raw, at, stop) for stop in (0, raw.length)]
    for tail in candidates:
        if tail.geom_type != "LineString" or tail.length < CONTACT_TOLERANCE:
            continue
        direction = np.subtract(tail.coords[1], tail.coords[0])
        if outward @ direction <= 0:
            continue
        ends = list(tail.coords)
        following = neighbors.get(ends[-1], ())
        if len(following) == 1:
            ends.extend(following[0].coords[1:])
        ends[0] = mouth.coords[0]
        route = shapely.LineString(ends)
        for part in shapely.get_parts(route.intersection(outline)):
            if part.geom_type != "LineString" or part.length <= PORTAL_HALF_WIDTH:
                continue
            if mouth.distance(shapely.Point(part.coords[-1])) < CONTACT_TOLERANCE:
                part = shapely.reverse(part)
            if mouth.distance(shapely.Point(part.coords[0])) < CONTACT_TOLERANCE:
                return shapely.LineString([mouth.coords[0], *part.coords[1:]])
    return None


def track_opening(route, half_width):
    xy = np.asarray(route.coords)
    direction = xy[1] - xy[0]
    outside = xy[0] - direction / np.linalg.norm(direction) * half_width
    extended = shapely.LineString(np.vstack((outside, xy)))
    return extended.buffer(half_width, cap_style="flat", join_style="mitre")


def plan_station_halls(features, decks, forms, approaches):
    if not decks or not forms:
        return {}, {}
    platforms = shapely.STRtree(
        [
            geom
            for name, geom, tags in features
            if name == "transportation" and tags.get("subclass") == "platform"
        ]
    )
    if len(platforms.geometries) == 0:
        return {}, {}
    raw = defaultdict(set)
    for name, geom, tags in features:
        if (
            name == "transportation"
            and tags.get("class") == "rail"
            and tags.get("brunnel") == "bridge"
        ):
            raw[tags.get("layer", 1)].update(
                shapely.normalize(part)
                for part in shapely.get_parts(geom)
                if part.geom_type == "LineString" and not part.is_empty
            )
    neighbors, _ = rail_neighbors(features)
    solid = shapely.STRtree([form.polygon for form in forms])
    halls, routes = {}, {}
    for deck in decks:
        floor = deck.elevation
        ballast = floor + BALLAST_DEPTH
        ceiling = ballast + STATION_CLEARANCE
        for track in deck.tracks:
            originals = [
                line
                for line in raw[deck.layer]
                if line.buffer(CONTACT_TOLERANCE).covers(track)
            ]
            if len(originals) != 1:
                continue
            for end in (0, -1):
                if (track, end) in approaches:
                    continue
                mouth = shapely.Point(track.coords[end])
                targets = solid.query(mouth, "dwithin", CONTACT_TOLERANCE)
                if len(targets) != 1:
                    continue
                form = forms[targets[0]]
                outline = form.polygon
                if (
                    form.passages
                    or not form.bottom < floor
                    or form.top - ceiling < MINIMUM_LINTEL
                    or outline.boundary.distance(mouth) > CONTACT_TOLERANCE
                    or not len(platforms.query(outline, "intersects"))
                ):
                    continue
                existing = halls.get(outline)
                if (
                    existing is not None
                    and abs(existing.floor - floor) > CONTACT_TOLERANCE
                ):
                    continue
                route = rail_inside_station(
                    originals[0], track, end, outline, neighbors
                )
                if route is None:
                    continue
                conflicts = shapely.union_all(
                    [
                        other.polygon
                        for i in solid.query(
                            route.buffer(PORTAL_HALF_WIDTH), "intersects"
                        )
                        if (other := forms[i]) is not form
                        and other.bottom < ceiling
                        and other.top > floor
                    ]
                )
                occupied = conflicts.buffer(PORTAL_HALF_WIDTH + CONTACT_TOLERANCE)
                if existing is not None:
                    occupied = occupied.union(
                        existing.ballast.buffer(BALLAST_WIDTH / 2 + CONTACT_TOLERANCE)
                    )
                blocked = route.intersection(occupied)
                if not blocked.is_empty:
                    first = np.min(
                        shapely.line_locate_point(
                            route, shapely.points(shapely.get_coordinates(blocked))
                        )
                    )
                    route = (
                        substring(route, 0, first - 0.05)
                        if first > PORTAL_HALF_WIDTH + 0.05
                        else None
                    )
                if route is None or route.is_empty:
                    continue
                opening = track_opening(route, PORTAL_HALF_WIDTH)
                if opening.is_empty or opening.intersection(outline).intersects(
                    conflicts
                ):
                    continue
                bearing = track_opening(route, BALLAST_WIDTH / 2).intersection(outline)
                if (
                    existing is not None
                    and bearing.intersection(existing.ballast).area > 1e-6
                ):
                    continue
                centreline = np.asarray(route.coords)
                # Include the existing dry approach when checking the continuous rail sweep.
                start = centreline if end == 0 else routes.get((track, 0))
                stop = centreline if end == -1 else None
                joined = track_centreline(
                    track,
                    approaches.get((track, 0)),
                    approaches.get((track, -1)),
                    (start, stop),
                )
                hardware, _ = build_rail_hardware(joined, ballast)
                if hardware is None:
                    continue
                all_openings = (
                    opening if existing is None else existing.openings.union(opening)
                )
                beds = bearing if existing is None else existing.ballast.union(bearing)
                halls[outline] = StationHall(all_openings, beds, floor, ceiling)
                routes[(track, end)] = centreline
    return halls, routes
