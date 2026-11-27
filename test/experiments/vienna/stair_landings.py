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


def fit_landing(points, ports, width, height, domain, water, stairs, material):
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

    port_edges = shapely.union_all(
        [shapely.LineString(port.edge) for port in ports if port is not None]
    )
    footprint = outline.difference(stairs, grid_size=1e-6)
    if footprint.geom_type != "Polygon" or not footprint.covers(
        shapely.line_interpolate_point(shapely.LineString(points), 0.5, normalized=True)
    ):
        return None
    if not footprint.buffer(2e-6).covers(port_edges):
        return None
    retaining_edge = footprint.boundary.difference(port_edges, grid_size=1e-6)
    corners = dict(zip(map(tuple, nodes), np.concatenate((za, zb[::-1])), strict=True))
    pieces = []
    for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(outline)):
        xy = np.asarray(triangle.exterior.coords)[:3]
        clipped = triangle.intersection(footprint, grid_size=1e-6)
        if clipped.area <= 1e-9:
            continue
        pieces.append(
            LandingPiece(
                clipped,
                retaining_edge,
                xy[0],
                np.linalg.inv(xy[1:] - xy[0]),
                np.stack([corners[tuple(p)] for p in xy]),
                material,
            )
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
