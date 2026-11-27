"""Fit dry lower routes and the height of the decks that cover them."""

from dataclasses import dataclass, replace
from itertools import pairwise

import numpy as np
import shapely
from bridge_junctions import bridge_junctions, connected_bridge_crossings
from ground_surface import at_grade_width, path_material, route_area
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from scipy.optimize import linprog
from scipy.sparse import coo_matrix
from shapely.ops import substring

CLEARANCE_M = 2.60
FLOOR_ABOVE_WATER_M = 0.25
MAXIMUM_GRADE = 0.08
GRADE_CHANGE_PER_M = 0.006
PROFILE_REACH_M = 55
PROFILE_SPACING_M = 1.75


@dataclass(frozen=True)
class StairContact:
    end: int
    sections: np.ndarray
    elevation: float
    opening: shapely.Geometry


@dataclass(frozen=True)
class PassageRoute:
    line: shapely.Geometry
    station: np.ndarray
    sections: np.ndarray
    footprint: shapely.Geometry
    ceilings: tuple
    floor: np.ndarray
    material: str
    stair_contacts: tuple[StairContact, ...]


@dataclass(frozen=True)
class Passage:
    route: PassageRoute
    surface: SurfaceSampler
    breaks: np.ndarray

    @property
    def footprint(self):
        return self.route.footprint

    @property
    def retaining_edge(self):
        return self.footprint.boundary

    def heights(self, ground):
        xy = ground[:, :2]
        results = self.surface.sample(xy)
        if not np.isfinite(results.points_enu_m[:, 2]).all():
            raise ValueError("Lower crossing must have a complete floor")
        s = shapely.line_locate_point(self.route.line, shapely.points(xy))
        distance = [s, self.route.station[-1] - s]
        for contact in self.route.stair_contacts:
            distance[contact.end] = np.full(len(s), 3.0)
        weight = np.clip(np.minimum(*distance) / 3, 0, 1)
        weight *= weight * (3 - 2 * weight)
        return ground[:, 2] + weight * (results.points_enu_m[:, 2] - ground[:, 2])


def water_level(body, xy, height):
    samples = xy[shapely.contains_xy(body, *xy.T)]
    if not len(samples):
        samples = np.asarray(body.representative_point().coords)
    return float(np.median(height(samples)))


def transverse_contacts(line, crossings, water):
    for i, crossing in enumerate(crossings):
        span = crossing.span
        for part in shapely.get_parts(line.intersection(crossing.footprint)):
            if (
                part.geom_type != "LineString"
                or part.length < 2
                or part.intersects(water)
            ):
                continue
            ends = np.asarray(part.coords)[[0, -1]]
            along = abs((ends[1] - ends[0]) @ span.direction)
            side = (ends - span.origin) @ (span.direction[[1, 0]] * [1, -1])
            if along > part.length / 2 or side.prod() >= 0:
                continue
            a, b = np.sort(shapely.line_locate_point(line, shapely.points(ends)))
            if a >= 4 and line.length - b >= 4:
                yield i, a, b


def route_sections(line, width):
    knots = np.r_[
        0, np.linalg.norm(np.diff(np.asarray(line.coords), axis=0), axis=1).cumsum()
    ]
    regular = np.linspace(
        0, knots[-1], 1 + int(np.ceil(knots[-1] / PROFILE_SPACING_M))
    )[1:-1]
    nearest = np.minimum(
        abs(regular - knots[np.searchsorted(knots, regular)]),
        abs(regular - knots[np.searchsorted(knots, regular) - 1]),
    )
    station = np.sort(np.r_[knots, regular[nearest > 0.025]])
    xy = shapely.get_coordinates(shapely.line_interpolate_point(line, station))
    delta = np.diff(xy, axis=0)
    distances = np.linalg.norm(delta, axis=1)
    if not len(distances) or distances.min() < 0.01:
        return None
    direction = delta / distances[:, None]
    lateral = direction[:, [1, 0]] * [1, -1]
    cosine = 1 + (direction[1:] * direction[:-1]).sum(axis=1)
    if np.any(cosine < 0.5):
        return None
    side = np.vstack(
        (lateral[0], (lateral[:-1] + lateral[1:]) / cosine[:, None], lateral[-1])
    )
    sections = np.round(
        xy[:, None, :]
        + (width / 2) * np.array([1, -1])[None, :, None] * side[:, None, :],
        6,
    )
    cells = shapely.polygons(
        np.concatenate((sections[:-1], sections[1:, ::-1]), axis=1)
    )
    if not shapely.is_valid(cells).all():
        return None
    footprint = shapely.union_all(cells, grid_size=1e-6)
    if (
        footprint.geom_type != "Polygon"
        or footprint.area + 1e-5 < shapely.area(cells).sum()
    ):
        return None
    return station, sections, cells, footprint


def clear_approach_tails(line, width, occupied, crossings, water, contacts):
    if occupied.is_empty or line.distance(occupied) > width:
        return line
    required = {i for i, _, _ in contacts}
    available = line.difference(occupied.buffer(width / 2 + 0.05))
    return max(
        (
            part
            for part in shapely.get_parts(available)
            if part.geom_type == "LineString"
            and {i for i, _, _ in transverse_contacts(part, crossings, water)}
            == required
        ),
        key=lambda part: part.length,
        default=None,
    )


def stair_contacts(line, cells, footprint, stairs):
    contacts = []
    ends = shapely.get_point(line, [0, -1])
    for stair in stairs:
        touched = stair.footprint.intersection(footprint)
        if touched.area <= 1e-5:
            continue
        for tread, port in zip(stair.treads[[0, -1]], stair.ports, strict=True):
            nearby = shapely.dwithin(ends, shapely.Point(port.point), 0.002)
            shared = touched.intersection(tread, grid_size=1e-6)
            if not nearby.any() or shared.area < 1e-5:
                continue
            if (
                line.intersection(stair.footprint).difference(tread.buffer(2e-6)).length
                > 0.002
            ):
                continue
            cells_at_port = shapely.intersects(cells, tread)
            nodes = np.flatnonzero(
                np.r_[cells_at_port, False] | np.r_[False, cells_at_port]
            )
            contacts.append(
                StairContact(int(np.argmax(nearby)), nodes, port.elevation, shared)
            )
            break
        else:
            return None
    return contacts


def passage_routes(features, crossings, domain, water, floors, blocked, stairs):
    if not len(floors):
        return []
    routes = []
    seen = set()
    occupied = shapely.Polygon()
    for name, geom, tags in features:
        if (
            name != "transportation"
            or tags.get("class") != "path"
            or tags.get("subclass") == "steps"
        ):
            continue
        width = at_grade_width(tags)
        if width is None or not geom.geom_type.endswith("LineString"):
            continue
        for path in shapely.get_parts(geom.intersection(domain.buffer(-1))):
            if path.geom_type != "LineString":
                continue
            contacts = list(transverse_contacts(path, crossings, water))
            if not contacts:
                continue
            start = max(0, min(a for _, a, _ in contacts) - PROFILE_REACH_M)
            end = min(path.length, max(b for _, _, b in contacts) + PROFILE_REACH_M)
            line = substring(path, start, end)
            line = clear_approach_tails(
                line, width, occupied, crossings, water, contacts
            )
            if line is None:
                continue
            key = shapely.normalize(line)
            if key in seen:
                continue
            seen.add(key)
            sampled = route_sections(line, width)
            if sampled is None:
                continue
            station, sections, cells, footprint = sampled
            if (
                footprint.intersects(water)
                or footprint.intersection(blocked.union(occupied)).area > 1e-5
            ):
                continue
            ports = stair_contacts(line, cells, footprint, stairs)
            if ports is None:
                continue
            ceilings = []
            for i, bridge in enumerate(crossings):
                cells_under = shapely.intersects(cells, bridge.footprint)
                if not cells_under.any():
                    continue
                columns = np.flatnonzero(
                    np.r_[cells_under, False] | np.r_[False, cells_under]
                )
                top = bridge.span.height(sections[columns]).min(axis=1)
                ceilings.append(
                    (i, columns, top - bridge.thickness - CLEARANCE_M - 0.02)
                )
            if not ceilings:
                continue
            _, level = min(floors, key=lambda item: item[0].distance(line))
            lower = np.full(len(station), -np.inf)
            near = shapely.dwithin(shapely.points(sections.mean(axis=1)), water, 12)
            lower[near] = level + FLOOR_ABOVE_WATER_M
            for _, columns, _ in ceilings:
                lower[columns] = level + FLOOR_ABOVE_WATER_M
            routes.append(
                PassageRoute(
                    line,
                    station,
                    sections,
                    footprint,
                    tuple(ceilings),
                    lower,
                    path_material(tags),
                    tuple(ports),
                )
            )
            occupied = occupied.union(footprint, grid_size=1e-6)
    return routes


def solve_profiles(routes, priors, crossings, minimum):
    count = sum(map(len, priors))
    variables = count * 2 + len(crossings)
    weights = np.r_[
        np.zeros(count), np.ones(count), [c.footprint.area for c in crossings]
    ]
    limits = [(None, None)] * count + [(0, None)] * count
    limits += [(float(z), None) for z in minimum]
    rhs, rows, columns, values = [], [], [], []

    def constraint(index, coefficients, value):
        rows.extend([len(rhs)] * len(index))
        columns.extend(index)
        values.extend(coefficients)
        rhs.append(value)

    first = 0
    for route, z in zip(routes, priors, strict=True):
        end = first + len(z)
        for i in np.flatnonzero(np.isfinite(route.floor)):
            limits[first + i] = (float(route.floor[i]), None)
        pins = {first: z[0], end - 1: z[-1]}
        for contact in route.stair_contacts:
            pins[first if contact.end == 0 else end - 1] = contact.elevation
            pins.update((first + node, contact.elevation) for node in contact.sections)
        for i, height in pins.items():
            if limits[i][0] is not None and height < limits[i][0]:
                return None
            limits[i] = (float(height), float(height))
        for i, h in enumerate(z, start=first):
            constraint([i, count + i], [1, -1], h)
            constraint([i, count + i], [-1, -1], -h)
        step = np.diff(route.station)
        for i, length in enumerate(step, start=first):
            for sign in (1, -1):
                constraint([i, i + 1], [-sign, sign], MAXIMUM_GRADE * length)
        for i, (a, b) in enumerate(pairwise(step), start=first):
            for sign in (1, -1):
                constraint(
                    [i, i + 1, i + 2],
                    np.array([1 / a, -1 / a - 1 / b, 1 / b]) * sign,
                    GRADE_CHANGE_PER_M * (a + b) / 2,
                )
        for crossing, nodes, ceiling in route.ceilings:
            for node, zmax in zip(nodes, ceiling, strict=True):
                constraint([first + node, 2 * count + crossing], [1, -1], zmax)
        first = end
    matrix = coo_matrix((values, (rows, columns)), shape=(len(rhs), variables)).tocsr()
    result = linprog(
        weights,
        A_ub=matrix,
        b_ub=rhs,
        bounds=limits,
        method="highs",
        options={"time_limit": 2.0},
    )
    if not result.success:
        return None
    profiles = np.split(result.x[:count], np.cumsum([len(z) for z in priors[:-1]]))
    return profiles, result.x[2 * count :]


def floor_mesh(route, z):
    xy = route.sections.reshape(-1, 2)
    cells = np.arange(0, len(xy) - 2, 2)[:, None] + [0, 1, 3, 0, 3, 2]
    triangles = cells.reshape(-1, 3)
    mesh = Mesh(np.c_[xy, np.repeat(z, 2)], triangles)
    breaks = shapely.boundary(shapely.polygons(xy[triangles]))
    return Passage(
        route,
        SurfaceSampler(mesh, xy_tolerance_m=2e-6, height_tolerance_m=3e-6),
        breaks,
    )


def fit_bridge_levels(
    source, crossings, domain, water, roads, vertices, terrain, stairs
):
    eligible = connected_bridge_crossings(source.features, crossings)
    if not eligible or water.is_empty:
        return None
    floors = [
        (body, water_level(body, vertices[:, :2], source.height))
        for body in shapely.get_parts(water)
        if body.geom_type == "Polygon"
    ]
    roads_and_rails = shapely.union_all(
        [
            route_area(geom, width)
            for name, geom, tags in source.features
            if name == "transportation"
            and tags.get("class") != "path"
            and (
                width := at_grade_width(tags)
                or (
                    2.1
                    if tags.get("subclass") == "tram" and not tags.get("brunnel")
                    else 0
                )
            )
        ]
    )
    blocked = roads_and_rails.difference(
        shapely.union_all([c.footprint for c in eligible])
    )
    routes = passage_routes(
        source.features, eligible, domain, water, floors, blocked, stairs
    )
    if not routes:
        return None
    xy = [route.sections.mean(axis=1) for route in routes]
    raw = [terrain(points) for points in xy]
    first = solve_profiles(routes, raw, eligible, np.zeros(len(eligible)))
    if first is None:
        return None
    _, lift = first
    for _ in range(6):
        elevated = {
            id(c): replace(
                c, span=replace(c.span, elevation=c.span.elevation + float(z))
            )
            for c, z in zip(eligible, lift, strict=True)
        }
        planned = [elevated.get(id(c), c) for c in crossings]
        junctions, spans = bridge_junctions(
            source.features, planned, domain, water, roads, vertices, terrain
        )
        if not spans:
            return None
        priors = [height.copy() for height in raw]
        for junction in junctions:
            sampler = SurfaceSampler(
                junction.mesh, xy_tolerance_m=2e-6, height_tolerance_m=3e-6
            )
            for points, height in zip(xy, priors, strict=True):
                z = sampler.sample(points).points_enu_m[:, 2]
                found = np.isfinite(z)
                height[found] = z[found]
        fitted = solve_profiles(routes, priors, eligible, lift)
        if fitted is None:
            return None
        profiles, needed = fitted
        if np.max(needed - lift) < 1e-6:
            passages = [
                floor_mesh(route, z) for route, z in zip(routes, profiles, strict=True)
            ]
            return planned, junctions, spans, passages
        lift = needed + np.where(needed > lift + 1e-6, 0.05, 0)
    return None


def bridge_terrain(source, crossings, domain, water, roads, vertices, terrain, stairs):
    plan = fit_bridge_levels(
        source, crossings, domain, water, roads, vertices, terrain, stairs
    )
    if plan is not None:
        return plan
    junctions, spans = bridge_junctions(
        source.features, crossings, domain, water, roads, vertices, terrain
    )
    return crossings, junctions, spans, []
