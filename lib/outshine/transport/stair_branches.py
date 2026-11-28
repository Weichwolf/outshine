"""Fit short, steep links at the exposed side of a mapped stair landing."""

from collections import defaultdict
from dataclasses import dataclass
from itertools import pairwise

import numpy as np
import shapely

from outshine.terrain.ground_surface import at_grade_width, path_material
from outshine.transport.stairs import stairway_from_treads


@dataclass(frozen=True)
class StairApron:
    footprint: shapely.Geometry
    edge: shapely.Geometry
    elevation: float
    reach: float
    material: str

    @property
    def retaining_edge(self):
        return self.footprint.boundary.difference(self.edge, grid_size=1e-6)

    def heights(self, ground):
        distance = shapely.distance(self.edge, shapely.points(ground[:, :2]))
        falloff = np.clip(1 - distance / self.reach, 0, 1)
        weight = falloff * falloff * (3 - 2 * falloff)
        return ground[:, 2] + weight * (self.elevation - ground[:, 2])


def exposed_sections(stair, end, width):
    port = stair.ports[end]
    tread = stair.treads[0 if end == 0 else -1]
    perimeter = stair.footprint.boundary.buffer(2e-6)
    node = shapely.Point(port.point)
    for a, b in pairwise(np.asarray(tread.exterior.coords)):
        edge = shapely.LineString([a, b])
        if edge.length < width - 2e-6 or not perimeter.covers(edge):
            continue
        half = min(width, edge.length) / 2
        station = np.clip(edge.project(node), half, edge.length - half)
        section = shapely.line_interpolate_point(edge, [station - half, station + half])
        xy = shapely.get_coordinates(section)
        if np.linalg.norm(xy.mean(axis=0) - port.point) <= width:
            yield xy


def fit_branch(stair, end, target, elevation, width, blocked, domain, material):
    port = stair.ports[end]
    rise = abs(elevation - port.elevation)
    count = int(np.ceil(rise / 0.20))
    for section in exposed_sections(stair, end, width):
        origin = section.mean(axis=0)
        offset = target - origin
        axis = (section[1] - section[0]) / np.linalg.norm(section[1] - section[0])
        run = abs(axis[0] * offset[1] - axis[1] * offset[0])
        depth = np.clip(0.63 - 2 * rise / count, 0.27, 0.40)
        if count * depth > run:
            continue
        ticks = (run - count * depth) / 2 + (np.arange(count) + 0.5) * depth
        sections = section + np.r_[0, ticks / run, 1][:, None, None] * offset
        corners = np.concatenate((sections[:-1], sections[1:, ::-1]), axis=1)
        heights = np.linspace(port.elevation, elevation, count + 1)
        flight = stairway_from_treads(
            np.stack((origin, target)), corners, heights, material
        )
        if flight is None or not domain.covers(flight.footprint):
            continue
        if blocked.intersection(flight.footprint).area > 1e-6:
            continue
        edge = shapely.LineString(flight.ports[1].edge)
        reach = width / 2
        collar = edge.buffer(reach, quad_segs=4).difference(
            flight.footprint, grid_size=1e-6
        )
        if not domain.covers(collar) or collar.intersects(blocked):
            continue
        yield flight, StairApron(collar, edge, elevation, reach, material)


def local_stair_branches(features, stairs, domain, blocked, height):
    ports = defaultdict(list)
    for stair in stairs:
        for end, port in enumerate(stair.ports):
            ports[port.point].append((stair, end))
    if not ports:
        return [], [], set()
    blocked = blocked.union(shapely.union_all([s.footprint for s in stairs]))
    flights, aprons = [], []
    consumed = set()
    for name, geom, tags in features:
        width = at_grade_width(tags)
        if (
            name != "transportation"
            or tags.get("subclass") != "footway"
            or width is None
        ):
            continue
        for line in shapely.get_parts(geom):
            if line.geom_type != "LineString" or len(line.coords) != 2:
                continue
            run = line.length
            if not width <= run <= width * 4:
                continue
            ends = [ports.get(point, ()) for point in line.coords]
            if sorted(map(len, ends)) != [0, 1]:
                continue
            source = 0 if ends[0] else 1
            [(stair, end)] = ends[source]
            target = np.asarray(line.coords[1 - source])
            elevation = float(height(target[None])[0])
            if abs(elevation - stair.ports[end].elevation) <= 0.12 * run:
                continue
            candidate = next(
                fit_branch(
                    stair,
                    end,
                    target,
                    elevation,
                    width,
                    blocked,
                    domain,
                    path_material(tags),
                ),
                None,
            )
            if candidate is not None:
                flight, apron = candidate
                flights.append(flight)
                aprons.append(apron)
                consumed.add(shapely.normalize(line))
                blocked = shapely.union_all(
                    [blocked, flight.footprint, apron.footprint]
                )
    return flights, aprons, consumed
