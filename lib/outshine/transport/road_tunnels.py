"""Fit cut-and-cover road tunnels to mapped mouths and a shared terrain surface."""

from collections import defaultdict
from dataclasses import dataclass
from itertools import pairwise, product
from types import SimpleNamespace

import numpy as np
import shapely
from scipy.optimize import linprog
from scipy.sparse import coo_matrix
from shapely.ops import substring

from outshine.terrain.ground_surface import (
    ROAD_WIDTH,
    at_grade_width,
    route_area,
    terrain_triangles,
)
from outshine.transport.bridge_passages import GRADE_CHANGE_PER_M, route_sections
from outshine.transport.elevated_rail import TRACK_HALF_WIDTH
from outshine.transport.passage_pavement import (
    MAXIMUM_GRADE,
    GroundSections,
    sample_profile,
)
from outshine.transport.tunnel_earthworks import (
    CoverLift,
    cover_lift_patch,
    fit_cover_lift,
)

PREFERRED_CLEARANCE = 4.5
MINIMUM_CLEARANCE = 4.0
LINING_DEPTH = 0.30


@dataclass(frozen=True)
class RoadTunnel:
    footprint: shapely.Polygon
    openings: shapely.Geometry
    points: np.ndarray
    faces: np.ndarray
    covered: tuple[int, int]
    clearance: float
    lift: CoverLift | None = None


@dataclass(frozen=True, slots=True)
class CoverOverlay:
    sections: np.ndarray
    road_weights: np.ndarray
    height: np.ndarray
    vertices: np.ndarray
    terrain_weights: np.ndarray


def road_sources(features):
    tunnels = defaultdict(set)
    links = defaultdict(dict)
    surfaces = []
    for name, geometry, tags in features:
        if name != "transportation":
            continue
        kind = tags.get("class")
        ground = at_grade_width(tags)
        track = (
            kind in {"rail", "transit"}
            and not tags.get("brunnel")
            and not tags.get("layer")
        )
        for part in shapely.get_parts(geometry):
            if ground is not None:
                surfaces.append((part, ground))
            elif track and part.geom_type == "LineString":
                surfaces.append((part, TRACK_HALF_WIDTH * 2))
            if (
                part.geom_type != "LineString"
                or part.is_empty
                or kind not in ROAD_WIDTH
                or kind == "path"
            ):
                continue
            key = shapely.normalize(part)
            if ground is not None:
                links[part.coords[0]][kind, key] = kind, part
                links[part.coords[-1]][kind, key] = kind, shapely.reverse(part)
            if tags.get("brunnel") == "tunnel":
                tunnels[(kind, tags.get("layer", -1))].add(key)
    return tunnels, links, surfaces


def _tunnel_approach(links, point, kind, hints):
    arms = links.get(point, {}).values()
    lead = hints.get(tuple(np.round(point, 6)))
    if lead is not None:
        target = shapely.Point(lead)
        matched = [line for _, line in arms if shapely.dwithin(line, target, 2e-6)]
        if len(matched) == 1:
            return matched[0]
    candidates = [line for road, line in arms if road == kind]
    return candidates[0] if len(candidates) == 1 else None


def _triangle_weights(triangles, points):
    a, b, c = triangles.transpose(1, 0, 2)
    u, v, world = b - a, c - a, points - a
    cross = lambda x, y: x[:, 0] * y[:, 1] - x[:, 1] * y[:, 0]
    det = cross(u, v)
    beta, gamma = cross(world, v) / det, cross(u, world) / det
    return np.c_[1 - beta - gamma, beta, gamma]


def ground_cover_constraints(pavement, terrain, height, vertices, faces):
    xy = pavement.points[pavement.triangles]
    panels = shapely.polygons(xy)
    face, ground = terrain.query(panels, "intersects")
    points, owner = shapely.get_coordinates(
        shapely.intersection(panels[face], terrain.geometries[ground]),
        return_index=True,
    )
    weights = _triangle_weights(xy[face[owner]], points)
    road = pavement.triangles[face[owner]]
    ground = faces[ground[owner]]
    offsets = (pavement.offsets[road] * weights).sum(axis=1)
    return CoverOverlay(
        pavement.sections[road],
        weights,
        height(points) - offsets,
        ground,
        _triangle_weights(vertices[ground, :2], points),
    )


def fit_floor(guides, pavement, cover, required, end_grades, terrain, earthwork):
    target = pavement.prior
    count = len(target)
    limits = [(None, None)] * count
    for index, value in ((0, target[0]), (-1, target[-1])):
        limits[index] = (value, value)
    limits.extend([(0, None)] * count)
    limits.append((MINIMUM_CLEARANCE, PREFERRED_CLEARANCE))
    rows, columns, data, values = [], [], [], []

    def bound(indices, coefficients, value):
        rows.extend([len(values)] * len(indices))
        columns.extend(indices)
        data.extend(coefficients)
        values.append(value)

    for i, z in enumerate(target):
        bound([i, count + i], [1, -1], z)
        bound([i, count + i], [-1, -1], -z)
    steps = np.diff(guides.station)
    panels, gains, bias = pavement.longitudinal_grade_terms(guides)
    for panel, gain, slope in zip(panels, gains, bias, strict=True):
        for sign in (-1, 1):
            bound(
                [panel, panel + 1],
                [-sign * gain, sign * gain],
                MAXIMUM_GRADE - sign * slope,
            )
    for i, (a, b) in enumerate(pairwise(steps)):
        rate = np.array((1 / a, -1 / a - 1 / b, 1 / b))
        for sign in (-1, 1):
            bound([i, i + 1, i + 2], sign * rate, GRADE_CHANGE_PER_M * (a + b) / 2)
    for i, slope in ((0, end_grades[0]), (count - 2, end_grades[1])):
        for sign in (-1, 1):
            bound([i, i + 1], [-sign, sign], sign * slope * steps[i] + 1e-6)
    selected = required[cover.sections.min(axis=1)]
    for indices, coefficients, z in zip(
        cover.sections[selected],
        cover.road_weights[selected],
        cover.height[selected],
        strict=True,
    ):
        bound([*indices, 2 * count], [*coefficients, 1], z - LINING_DEPTH)
    matrix = coo_matrix(
        (data, (rows, columns)), shape=(len(values), 2 * count + 1)
    ).tocsr()
    if earthwork is not None:
        bounds = [*limits[:-1], (MINIMUM_CLEARANCE, MINIMUM_CLEARANCE)]
        fitted = fit_cover_lift(
            matrix,
            values,
            bounds,
            *terrain,
            *earthwork,
            cover.vertices[selected],
            cover.terrain_weights[selected],
        )
        if fitted is None:
            return None
        solution, lift = fitted
        return solution[:count], float(solution[-1]), lift

    options = {
        "A_ub": matrix,
        "b_ub": values,
        "method": "highs",
        "options": {"time_limit": 2.0},
    }
    capacity = linprog(np.r_[np.zeros(2 * count), -1], bounds=limits, **options)
    if not capacity.success:
        return None
    clearance = float(capacity.x[-1])
    limits[-1] = (clearance, clearance)
    fit = linprog(np.r_[np.zeros(count), np.ones(count), 0], bounds=limits, **options)
    return (fit.x[:count] if fit.success else capacity.x[:count]), clearance, None


def fit_tunnel(
    line, width, domain, protected, occupied, terrain, ground, height, lattice, frozen
):
    sectioned = route_sections(line, width)
    if sectioned is None:
        return None
    station, sections, cells, footprint = sectioned
    if not domain.covers(footprint) or footprint.intersects(occupied):
        return None
    guides = SimpleNamespace(sections=sections, station=station, stair_contacts=())
    pavement = sample_profile(guides, ground)
    centres = sections.mean(axis=1)
    end_grades = np.clip(
        np.diff(pavement.prior)[[0, -1]] / np.diff(station)[[0, -1]],
        -MAXIMUM_GRADE * 0.8,
        MAXIMUM_GRADE * 0.8,
    )
    must_cover = shapely.intersects(cells, protected)
    midpoint = station[-1] / 2
    must_cover |= (station[1:] > midpoint - width) & (station[:-1] < midpoint + width)
    indices = np.flatnonzero(must_cover)
    if indices[0] == 0 or indices[-1] == len(cells) - 1:
        return None
    required = np.zeros(len(cells), dtype=bool)
    required[indices[0] : indices[-1] + 1] = True
    cover = ground_cover_constraints(pavement, terrain, height, *lattice)
    earthwork = (
        cover_lift_patch(*lattice, required, guides, frozen, width)
        if frozen is not None
        else None
    )
    fit = fit_floor(guides, pavement, cover, required, end_grades, lattice, earthwork)
    if fit is None:
        return None
    z, clearance, lift = fit
    air = cover.height - (z[cover.sections] * cover.road_weights).sum(axis=1)
    if lift is not None:
        displacement = np.zeros(len(lattice[0]))
        displacement[lift.vertices] = lift.displacement
        air += (displacement[cover.vertices] * cover.terrain_weights).sum(axis=1)
    by_panel = np.full(len(cells), np.inf)
    np.minimum.at(by_panel, cover.sections.min(axis=1), air)
    covered = by_panel >= clearance + LINING_DEPTH - 1e-6
    if not np.all(covered[required]):
        return None
    first, last = indices[0], indices[-1] + 1
    while first > 0 and covered[first - 1]:
        first -= 1
    while last < len(cells) and covered[last]:
        last += 1
    openings = shapely.union_all(np.r_[cells[:first], cells[last:]])
    points = np.c_[pavement.points, z[pavement.sections] + pavement.offsets]
    return RoadTunnel(
        footprint,
        openings,
        points.reshape(len(centres), -1, 3),
        pavement.triangles,
        (first, last),
        clearance,
        lift,
    )


def approach_lengths(line, width, reach, protected, domain):
    safe = domain.buffer(-width / 2).difference(protected.buffer(width / 2))
    start = shapely.Point(line.coords[0])
    candidates = shapely.get_parts(
        substring(line, 0, min(reach, line.length)).intersection(safe)
    )
    lengths = [
        part.length
        for part in candidates
        if part.geom_type == "LineString" and part.distance(start) < 1e-6
    ]
    if not lengths:
        return ()
    limit = max(lengths)
    return np.unique(np.r_[np.arange(width, limit, 2 * width), limit])


def tunnel_routes(bore, approaches, left, right):
    for before, after in sorted(product(*approaches), key=sum):
        yield shapely.LineString(
            [
                *shapely.reverse(substring(left, 0, before)).coords[:-1],
                *bore.coords,
                *substring(right, 0, after).coords[1:],
            ]
        )


def covered_ground(vertices, faces, tunnel):
    if tunnel.lift is None:
        return tunnel.footprint
    moved = np.zeros(len(vertices), dtype=bool)
    moved[tunnel.lift.vertices] = True
    patch = shapely.polygons(vertices[faces[moved[faces].any(axis=1)], :2])
    return shapely.union_all([tunnel.footprint, *patch])


def plan_road_tunnels(
    features,
    domain,
    buildings,
    water,
    reservations,
    vertices,
    quads,
    height,
    *,
    portal_approaches=None,
):
    tunnels, links, surfaces = road_sources(features)
    if not tunnels:
        return ()
    protected = shapely.union_all(buildings)
    occupied = shapely.union_all([water, *reservations])
    tram_ground = shapely.union_all(
        [
            route_area(geometry, 2 * TRACK_HALF_WIDTH)
            for name, geometry, tags in features
            if name == "transportation"
            and tags.get("class") == "transit"
            and tags.get("subclass") == "tram"
            and not tags.get("brunnel")
            and not tags.get("layer")
        ]
    )
    faces = terrain_triangles(quads)
    terrain = shapely.STRtree(shapely.polygons(vertices[faces, :2]))
    ground = GroundSections(vertices, quads, height, ())
    roads = [(line, route_area(line, width)) for line, width in surfaces]
    areas = shapely.STRtree([area for _, area in roads])
    reach = (PREFERRED_CLEARANCE + LINING_DEPTH) / MAXIMUM_GRADE
    reach += 2 * MAXIMUM_GRADE / GRADE_CHANGE_PER_M
    hints = portal_approaches or {}
    fitted = []
    for (kind, _), parts in sorted(tunnels.items()):
        merged = shapely.line_merge(
            shapely.MultiLineString(sorted(parts, key=lambda line: line.wkb))
        )
        for bore in shapely.get_parts(merged):
            if not domain.covers(bore):
                continue
            left = _tunnel_approach(links, bore.coords[0], kind, hints)
            right = _tunnel_approach(links, bore.coords[-1], kind, hints)
            if left is None or right is None:
                continue
            width = ROAD_WIDTH[kind]
            route_domain = bore.buffer(reach + width)
            surfaces = shapely.union_all(
                [
                    protected,
                    *(
                        roads[i][1]
                        for i in areas.query(route_domain, "intersects")
                        if not roads[i][0].equals(left)
                        and not roads[i][0].equals(right)
                    ),
                ]
            )
            approaches = [
                approach_lengths(link, width, reach, surfaces, domain)
                for link in (left, right)
            ]
            best = None
            frozen = shapely.union_all(
                [protected, occupied, tram_ground, domain.boundary]
            )
            for support in (None, frozen):
                for route in tunnel_routes(bore, approaches, left, right):
                    result = fit_tunnel(
                        route,
                        width,
                        domain,
                        surfaces,
                        occupied,
                        terrain,
                        ground,
                        height,
                        (vertices, faces),
                        support,
                    )
                    if result is None:
                        continue
                    if best is None or result.clearance > best.clearance + 1e-5:
                        best = result
                    if (
                        support is not None
                        or result.clearance >= PREFERRED_CLEARANCE - 1e-5
                    ):
                        break
                if best is not None:
                    break
            if best is not None:
                fitted.append(best)
                occupied = occupied.union(covered_ground(vertices, faces, best))
    return fitted


def wall_panels(meshes, low, high, *, reverse=False):
    visible = np.any(abs(high[:, :, 2] - low[:, :, 2]) > 1e-6, axis=1)
    low, high = low[visible], high[visible]
    faces = np.arange(len(low) * 4).reshape(-1, 4)[:, [0, 1, 2, 0, 2, 3]].reshape(-1, 3)
    points = np.concatenate((low, high[:, ::-1]), axis=1).reshape(-1, 3)
    meshes.add("retaining wall", points, faces[:, ::-1] if reverse else faces)


def draw_road_tunnel(meshes, tunnel, height):
    points = tunnel.points
    first, last = tunnel.covered
    width = points.shape[1]
    meshes.add("road", points.reshape(-1, 3), tunnel.faces[:, ::-1])
    ceiling = points.copy()
    ceiling[:, :, 2] += tunnel.clearance
    ceiling_faces = tunnel.faces[tunnel.faces.min(axis=1) // width >= first]
    ceiling_faces = ceiling_faces[ceiling_faces.max(axis=1) // width <= last]
    meshes.add("bridge deck", ceiling.reshape(-1, 3), ceiling_faces)
    adjacent = np.arange(len(points) - 1)[:, None] + [0, 1]
    covered = (adjacent[:, 0] >= first) & (adjacent[:, 0] < last)
    for side in (0, -1):
        low = points[adjacent, side]
        high = low.copy()
        high[:, :, 2] = height(low[:, :, :2].reshape(-1, 2)).reshape(-1, 2)
        high[covered, :, 2] = low[covered, :, 2] + tunnel.clearance
        wall_panels(meshes, low, high, reverse=side == 0)
    for station in (first, last):
        rim = ceiling[station, np.arange(width - 1)[:, None] + [0, 1]]
        bank = rim.copy()
        bank[:, :, 2] = height(rim[:, :, :2].reshape(-1, 2)).reshape(-1, 2)
        wall_panels(meshes, rim, bank, reverse=station == last)
