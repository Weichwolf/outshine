"""Join mapped footways to the full cross sections of fitted stair treads."""

from collections import defaultdict
from dataclasses import dataclass

import numpy as np
import shapely
from ground_surface import at_grade_width, path_material


@dataclass(frozen=True)
class LandingPiece:
    footprint: shapely.Geometry
    retaining_edge: shapely.Geometry
    origin: np.ndarray
    height_weights: np.ndarray
    corner_values: np.ndarray
    material: str

    def heights(self, ground):
        barycentric = (ground[:, :2] - self.origin) @ self.height_weights
        weights = np.c_[1 - barycentric.sum(axis=1), barycentric]
        height, blend = (weights @ self.corner_values).T
        return height + np.square(np.clip(blend, 0, 1)) * (ground[:, 2] - height)


def cross_section(point, port, side, height):
    if port is None:
        edge = np.round(np.array([point + side, point - side]), 6)
        return edge, np.c_[height(edge), np.ones(2)]
    edge = port.edge
    if (edge[0] - edge[1]) @ side < 0:
        edge = edge[::-1]
    return edge, np.array([[port.elevation, 0], [port.elevation, 0]])


def landing_pieces(footprint, sections, values, ports, material):
    edge = shapely.union_all([shapely.LineString(port.edge) for port in ports])
    retaining = footprint.boundary.difference(edge, grid_size=1e-6)
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(footprint))
    coords = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    points = shapely.points(coords.reshape(-1, 2))
    distances = shapely.distance(sections[:, None], points[None, :])
    mix = distances[0] / distances.sum(axis=0)
    offsets = shapely.line_locate_point(
        sections[:, None], points[None, :], normalized=True
    )
    endpoints = (
        values[:, 0, None] + offsets[..., None] * (values[:, 1] - values[:, 0])[:, None]
    )
    interpolated = endpoints[0] + mix[:, None] * (endpoints[1] - endpoints[0])
    for port in ports:
        touches = shapely.dwithin(points, shapely.LineString(port.edge), 2e-6)
        interpolated[touches] = [port.elevation, 0]
    return [
        LandingPiece(
            triangle, retaining, xy[0], np.linalg.inv(xy[1:] - xy[0]), z, material
        )
        for triangle, xy, z in zip(
            triangles, coords, interpolated.reshape(-1, 3, 2), strict=True
        )
    ], edge


def fit_landing(
    points, ports, width, height, domain, water, stairs, material, neighbors
):
    delta = points[1] - points[0]
    run = np.linalg.norm(delta)
    if run < width / 2 or run > width * 10:
        return None
    side = delta[[1, 0]] * [1, -1] * (width / (2 * run))
    a, za = cross_section(points[0], ports[0], side, height)
    b, zb = cross_section(points[1], ports[1], side, height)
    if abs(za[:, 0].mean() - zb[:, 0].mean()) > 0.12 * run:
        return None
    nodes = np.concatenate((a, b[::-1]))
    outline = shapely.Polygon(nodes)
    if not outline.is_valid or not domain.covers(outline) or water.intersects(outline):
        return None

    contacts = [port for port in ports if port is not None]
    footprint = outline.difference(stairs, grid_size=1e-6)
    if footprint.geom_type != "Polygon" or not footprint.covers(
        shapely.line_interpolate_point(shapely.LineString(points), 0.5, normalized=True)
    ):
        return None
    port_boundary = footprint.boundary.buffer(2e-6)
    if not all(
        port_boundary.covers(shapely.LineString(port.edge)) for port in contacts
    ):
        return None
    if len(contacts) == 2:
        contacts.extend(
            port
            for port in neighbors
            if port.point not in (ports[0].point, ports[1].point)
            and abs(port.elevation - za[0, 0]) < 0.10
            and abs(port.elevation - zb[0, 0]) < 0.10
            and port_boundary.covers(shapely.LineString(port.edge))
        )
    pieces, port_edges = landing_pieces(
        footprint, shapely.linestrings([a, b]), np.stack([za, zb]), contacts, material
    )
    return pieces, footprint, port_edges


def local_landings(features, stairs, domain, water, height):
    if not stairs:
        return [], shapely.GeometryCollection()
    ports = defaultdict(list)
    for stair in stairs:
        for port in stair.ports:
            ports[port.point].append(port)
    stair_area = shapely.union_all([stair.footprint for stair in stairs])
    neighbors = [port for group in ports.values() for port in group if len(group) == 1]
    approaches, joined = [], []
    occupied = shapely.GeometryCollection()
    for name, geom, tags in features:
        if name != "transportation" or tags.get("subclass") != "footway":
            continue
        width = at_grade_width(tags)
        if width is None:
            continue
        for line in shapely.get_parts(geom):
            if line.geom_type != "LineString" or len(line.coords) != 2:
                continue
            sections = [ports.get(point, ()) for point in line.coords]
            if not any(sections) or any(len(options) > 1 for options in sections):
                continue
            paired = tuple(section[0] if section else None for section in sections)
            candidate = fit_landing(
                np.asarray(line.coords),
                paired,
                width,
                height,
                domain,
                water,
                stair_area,
                path_material(tags),
                neighbors,
            )
            if candidate is None:
                continue
            pieces, footprint, edges = candidate
            if occupied.intersection(footprint).area > 1e-6:
                continue
            occupied = occupied.union(footprint, grid_size=1e-6)
            approaches.extend(pieces)
            joined.append(edges)
    return approaches, shapely.union_all(joined)
