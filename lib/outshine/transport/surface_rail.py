"""Fit mapped surface rail to a continuous formation above any covered road."""

from types import SimpleNamespace

import numpy as np
import shapely
from scipy.optimize import linprog
from scipy.sparse import coo_matrix

from outshine.contracts import Mesh
from outshine.geometry.sampling import SurfaceSampler
from outshine.transport.bridge_passages import route_sections
from outshine.transport.elevated_rail import BALLAST_DEPTH, BALLAST_WIDTH
from outshine.transport.passage_pavement import PavementProfile, profile_triangles
from outshine.transport.rail_approaches import (
    MAXIMUM_GRADE_CHANGE,
    MAXIMUM_RAIL_GRADE,
    RailFormation,
    rail_neighbors,
)
from outshine.transport.rail_hardware import bake_rail_sleepers, build_rail_hardware
from outshine.transport.road_tunnels import LINING_DEPTH


def profile_floor(cells, tunnels):
    floor = np.full(len(cells), -np.inf)
    cells_index = shapely.STRtree(cells)
    for tunnel in tunnels:
        points = tunnel.points
        first, last = tunnel.covered
        width = points.shape[1]
        ids = tunnel.faces
        panel = ids.min(axis=1) // width
        covered = points.reshape(-1, 3)[ids[(panel >= first) & (panel < last)]]
        roof = shapely.polygons(covered[:, :, :2])
        triangle, cell = cells_index.query(roof, "intersects")
        if not len(cell):
            continue
        points, owner = shapely.get_coordinates(
            shapely.intersection(roof[triangle], cells[cell]), return_index=True
        )
        a, b, c = covered[triangle[owner]].transpose(1, 0, 2)
        normal = np.cross(b - a, c - a)
        z = a[:, 2] - ((points - a[:, :2]) * normal[:, :2]).sum(axis=1) / normal[:, 2]
        z += tunnel.clearance + LINING_DEPTH + BALLAST_DEPTH
        np.maximum.at(floor, cell[owner], z)
    return np.maximum(np.r_[-np.inf, floor], np.r_[floor, -np.inf])


def fit_ballast(station, sections, prior, floor):
    count = len(station)
    index = np.arange(count)
    edges = np.c_[index[:-1], index[1:]]
    steps = np.diff(station)
    target = np.c_[index, index + count]
    xyz = sections.reshape(-1, 2)
    faces = profile_triangles(count, width=2)
    pavement = PavementProfile(
        xyz, faces, np.repeat(index, 2), np.zeros(len(xyz)), prior
    )
    guides = SimpleNamespace(sections=sections)
    panels, gains, _ = pavement.longitudinal_grade_terms(guides)
    grade = gains[:, None] * [-1, 1]
    curvature = np.c_[1 / steps[:-1], -1 / steps[:-1] - 1 / steps[1:], 1 / steps[1:]]
    batches = [
        (target, np.tile([1, -1], (count, 1)), prior),
        (target, np.tile([-1, -1], (count, 1)), -prior),
        (edges[panels], grade, np.full(len(panels), MAXIMUM_RAIL_GRADE)),
        (edges[panels], -grade, np.full(len(panels), MAXIMUM_RAIL_GRADE)),
        (
            np.c_[index[:-2], index[1:-1], index[2:]],
            curvature,
            MAXIMUM_GRADE_CHANGE * (steps[:-1] + steps[1:]) / 2,
        ),
        (
            np.c_[index[:-2], index[1:-1], index[2:]],
            -curvature,
            MAXIMUM_GRADE_CHANGE * (steps[:-1] + steps[1:]) / 2,
        ),
    ]
    rows, columns, data, rhs = [], [], [], []
    offset = 0
    for indices, weights, bounds in batches:
        rows.append(np.repeat(offset + np.arange(len(indices)), indices.shape[1]))
        columns.append(indices.ravel())
        data.append(weights.ravel())
        rhs.append(bounds)
        offset += len(bounds)
    matrix = coo_matrix(
        (np.concatenate(data), (np.concatenate(rows), np.concatenate(columns))),
        shape=(offset, 2 * count),
    ).tocsr()
    limits = [(float(z) if np.isfinite(z) else None, None) for z in floor]
    limits.extend([(0, None)] * count)
    fit = linprog(
        np.r_[np.zeros(count), np.ones(count)],
        A_ub=matrix,
        b_ub=np.concatenate(rhs),
        bounds=limits,
        method="highs",
        options={"time_limit": 2.0},
    )
    if not fit.success:
        return None
    return Mesh(np.c_[xyz, np.repeat(fit.x[:count], 2)], faces[:, ::-1])


def _surface_lines(features, extent, decks, routes):
    if routes is not None and not decks:
        return routes
    ports, index = rail_neighbors(features)
    claimed = {
        shapely.normalize(line)
        for deck in decks
        for track in deck.tracks
        for port in (track.coords[0], track.coords[-1])
        for line in ports.get(port, ())
    }
    if routes is not None:
        if not claimed:
            return routes
        reserve = shapely.union_all(list(claimed)).buffer(1e-4)
        return tuple(shapely.get_parts(shapely.difference(routes, reserve)))
    lines = [
        index.geometries[i]
        for i in index.query(extent, "intersects")
        if index.geometries[i] not in claimed
    ]
    return shapely.get_parts(shapely.line_merge(shapely.MultiLineString(lines)))


def plan_surface_rail(
    features, extent, height, *, blocked, tunnels=(), decks=(), routes=None
):
    lines = _surface_lines(features, extent, decks, routes)
    if not len(lines):
        return ()
    blocked = shapely.union_all(blocked)
    half = BALLAST_WIDTH / 2
    allowed = extent.buffer(-half).difference(blocked.buffer(half))
    candidates = [
        part
        for line in lines
        for part in shapely.get_parts(line.intersection(allowed))
        if part.geom_type == "LineString"
        and part.is_simple
        and part.length >= BALLAST_WIDTH
    ]
    found = []
    occupied = shapely.Polygon()
    for line in sorted(candidates, key=lambda part: (-part.length, part.wkb)):
        result = route_sections(line, BALLAST_WIDTH)
        if result is None:
            continue
        station, sections, cells, footprint = result
        if (
            footprint.intersection(occupied).area > 1e-6
            or footprint.intersection(blocked).area > 1e-6
        ):
            continue
        centre = sections.mean(axis=1)
        floor = profile_floor(cells, tunnels)
        mesh = fit_ballast(station, sections, height(centre) + BALLAST_DEPTH, floor)
        if mesh is None:
            continue
        hardware, _ = build_rail_hardware(centre, 0.0)
        if hardware is None:
            continue
        formation = RailFormation(
            footprint,
            centre,
            SurfaceSampler(mesh, xy_tolerance_m=3e-6, height_tolerance_m=4e-6),
        )
        products = []
        for material, product in (
            ("rail head", hardware.rails),
            ("rail sleeper", bake_rail_sleepers(hardware.sleeper_transforms)),
        ):
            points = product.vertices.copy()
            sampled = np.concatenate(
                [
                    formation.surface.sample(chunk[:, :2]).points_enu_m[:, 2]
                    for chunk in np.array_split(points, 1 + len(points) // 65536)
                ]
            )
            if not np.isfinite(sampled).all():
                break
            points[:, 2] += sampled
            products.append((material, points, product.triangles))
        if len(products) == 2:
            found.append((formation, products))
            occupied = occupied.union(footprint, grid_size=1e-6)
    return found
