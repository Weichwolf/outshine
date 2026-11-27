"""Shared grade-limited terrain surfaces for road and tram bridgeheads."""

from dataclasses import dataclass

import numpy as np
import shapely
from bridge_approach import APPROACH_REACH
from ground_surface import ROAD_WIDTH, partition_ground, path_material, route_area
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import HeightBudget, fit_surface_heights


@dataclass(frozen=True)
class BridgeJunction:
    footprint: shapely.Geometry
    mesh: Mesh


def junction_pavement(features, crossings, junctions, joined_spans, roads):
    if not junctions:
        return set()
    approach_area = shapely.union_all([j.footprint for j in junctions])
    served = [c for c in crossings if any(c.span is s for s in joined_spans)]
    carried = set()
    for geom, tags in features:
        width = ROAD_WIDTH.get(tags.get("class"))
        if (
            width is None
            or tags.get("brunnel") != "bridge"
            or not geom.geom_type.endswith("LineString")
        ):
            continue
        for crossing in served:
            if (
                tags.get("layer", 1) != crossing.layer
                or geom.intersection(crossing.footprint).length <= geom.length / 2
            ):
                continue
            carried.add(id(geom))
            material = path_material(tags) if tags["class"] == "path" else "road"
            surface = route_area(geom, width).intersection(approach_area)
            roads[material] = roads[material].union(surface, grid_size=1e-6)
            break
    return carried


def tram_bridge_crossings(features, crossings):
    routes = {}
    for name, geom, tags in features:
        if (
            name == "transportation"
            and tags.get("subclass") == "tram"
            and tags.get("brunnel") == "bridge"
        ):
            routes.setdefault(tags.get("layer", 1), []).append(geom)
    return [
        crossing
        for crossing in crossings
        if any(
            route.intersection(crossing.footprint).length > crossing.span.length / 2
            for route in routes.get(crossing.layer, ())
        )
    ]


def approach_core(crossing):
    span = crossing.span
    width = crossing.footprint.area / span.length
    trapezoid = np.array(
        [
            [-4, -0.8 * width],
            [APPROACH_REACH, -width],
            [APPROACH_REACH, width],
            [-4, 0.8 * width],
        ]
    )
    pieces = []
    for station, sign in ((0, -1), (span.length, 1)):
        end = span.origin + span.direction * station
        direction = sign * span.direction
        lateral = direction[[1, 0]] * [1, -1]
        pieces.append(shapely.Polygon(end + trapezoid @ [direction, lateral]))
    return shapely.union_all(pieces)


def patch_mesh(footprint, terrain, lattice, spacing):
    minimum, maximum = np.asarray(footprint.bounds).reshape(2, 2)
    first = np.floor((minimum - lattice) / spacing).astype(int)
    last = np.ceil((maximum - lattice) / spacing).astype(int)
    axes = [
        start + spacing * np.arange(lo, hi + 1)
        for start, lo, hi in zip(lattice, first, last, strict=True)
    ]
    xy = np.stack(np.meshgrid(*axes), axis=-1).reshape(-1, 2)
    width, height = map(len, axes)
    corners = np.arange(len(xy)).reshape(height, width)[:-1, :-1].ravel()
    quads = corners[:, None] + [0, 1, width + 1, width]
    parts = partition_ground(
        np.c_[xy, terrain(xy)],
        quads,
        np.zeros(len(quads), dtype=int),
        {0: "outside", 1: "head"},
        {1: footprint},
    )
    xy = np.concatenate([v[f, :2] for name, v, f in parts if name == "head"])
    points, indices = np.unique(
        np.round(xy.reshape(-1, 2), 6), axis=0, return_inverse=True
    )
    faces = indices.reshape(-1, 3)
    u, v = (points[faces[:, 1:]] - points[faces[:, :1]]).transpose(1, 0, 2)
    faces = faces[abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) > 1e-8]
    used, inverse = np.unique(faces, return_inverse=True)
    xy = points[used]
    return Mesh(np.c_[xy, terrain(xy)], inverse.reshape(-1, 3))


def graded_patch(mesh, footprint, exterior, core, crossings, road, track, shoulder):
    xy = mesh.vertices[:, :2]
    points = shapely.points(xy)
    border = shapely.dwithin(points, exterior, 2e-6)
    pins = {i: float(mesh.vertices[i, 2]) for i in np.flatnonzero(border)}
    for crossing in crossings:
        edge = crossing.footprint.boundary
        if not footprint.intersects(edge.buffer(2e-6)):
            continue
        contact = np.flatnonzero(shapely.dwithin(points, edge, 2e-6))
        pins.update(zip(contact, crossing.span.height(xy[contact]), strict=True))
    if len(pins) == np.count_nonzero(border):
        return None
    triangles = shapely.polygons(xy[mesh.triangles])
    limits = np.full(len(triangles), 0.55)
    limits[shapely.intersects(triangles, road)] = 0.085
    limits[shapely.intersects(triangles, track)] = 0.065
    blend = np.clip(
        shapely.distance(shapely.centroid(triangles), core) / shoulder, 0, 1
    )
    limits += (0.9 - limits) * blend
    frames = np.eye(2)[None] / limits[:, None, None]
    result = fit_surface_heights(mesh, frames, fixed=pins, budget=HeightBudget())
    return result.mesh


def bridge_junctions(features, crossings, domain, water, roads, vertices, terrain):
    served = tram_bridge_crossings(features, crossings)
    if not served:
        return [], []
    track = shapely.union_all(
        [
            geom.buffer(1.0, cap_style="flat").intersection(domain)
            for name, geom, tags in features
            if name == "transportation"
            and tags.get("subclass") == "tram"
            and not tags.get("brunnel")
        ]
    )
    core = shapely.union_all([approach_core(crossing) for crossing in served])
    shoulder = 20.0
    spacing = (vertices[1, 0] - vertices[0, 0]) / 2
    lattice = vertices[0, :2]
    outline = core.buffer(shoulder, quad_segs=2).intersection(domain.buffer(-1))
    barriers = shapely.union_all([water, *(c.footprint for c in crossings)])
    ground = outline.difference(barriers, grid_size=1e-6)
    products = []
    for piece in shapely.get_parts(ground):
        if piece.geom_type != "Polygon" or not piece.intersects(core):
            continue
        mesh = patch_mesh(piece, terrain, lattice, spacing)
        solved = graded_patch(
            mesh, piece, outline.boundary, core, served, roads, track, shoulder
        )
        if solved is None:
            return [], []
        products.append(BridgeJunction(piece, solved))
    return products, [crossing.span for crossing in served] if products else []
