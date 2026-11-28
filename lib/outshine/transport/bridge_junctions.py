"""Shared grade-limited terrain surfaces at bridgeheads."""

from dataclasses import dataclass, replace

import numpy as np
import shapely

from outshine.contracts import Mesh
from outshine.terrain.ground_surface import (
    ROAD_WIDTH,
    at_grade_width,
    carriageway_footprint,
    partition_ground,
    path_material,
    route_area,
)
from outshine.transport.bridge_approach import APPROACH_REACH
from outshine.transport.heights import HeightBudget, fit_surface_heights


@dataclass(frozen=True)
class BridgeJunction:
    footprint: shapely.Geometry
    mesh: Mesh


def junction_pavement(features, crossings, junctions, joined_spans, roads):
    if not junctions:
        return {}
    approach_area = shapely.union_all([j.footprint for j in junctions])
    served = [c for c in crossings if any(c.span is s for s in joined_spans)]
    endpoints, _ = bridge_routes(("transportation", g, tags) for g, tags in features)
    carried = {}
    for geom, tags in features:
        kind = tags.get("class")
        width = ROAD_WIDTH.get(kind)
        if (
            width is None
            or tags.get("brunnel") != "bridge"
            or not geom.geom_type.endswith("LineString")
        ):
            continue
        material = path_material(tags) if kind == "path" else "road"
        neighbors = endpoints["pedestrian" if kind == "path" else "road"]
        parts = []
        for route in shapely.get_parts(geom):
            if not any(
                tags.get("layer", 1) == crossing.layer
                and route.intersection(crossing.footprint).length > route.length / 2
                for crossing in served
            ):
                continue
            parts.append(route)
            surface = route_area(route_with_banks(route, neighbors), width)
            surface = surface.intersection(route.buffer(width / 2))
            fitted = shapely.get_parts(surface.intersection(approach_area))
            paved = fitted[shapely.get_dimensions(fitted) == 2]
            roads[material] = shapely.union_all(
                [roads[material], *paved], grid_size=1e-6
            )
        if parts:
            carried[id(geom)] = shapely.union_all(parts)
    return carried


def route_with_banks(route, endpoints):
    first = endpoints.get(route.coords[0], ())
    last = endpoints.get(route.coords[-1], ())
    return shapely.line_merge(shapely.union_all([route, *first, *last]))


def bridge_routes(features):
    ends, routes = {"road": {}, "pedestrian": {}}, {}
    for name, geom, tags in features:
        if name != "transportation" or not geom.geom_type.endswith("LineString"):
            continue
        kind = tags.get("class")
        if tags.get("subclass") == "tram":
            mode = "tram"
        elif kind == "path" and tags.get("subclass") != "steps":
            mode = "pedestrian"
        elif kind in ROAD_WIDTH and kind != "path":
            mode = "road"
        else:
            continue
        for line in shapely.get_parts(geom):
            if line.is_empty:
                continue
            if tags.get("brunnel") == "bridge":
                width = ROAD_WIDTH.get(kind)
                routes.setdefault(tags.get("layer", 1), []).append((line, mode, width))
            elif mode != "tram" and at_grade_width(tags) is not None:
                for end in (line.coords[0], line.coords[-1]):
                    ends[mode].setdefault(end, []).append(line)
    return ends, routes


def crossing_routes(crossing, endpoints, routes):
    for route, mode, width in routes.get(crossing.layer, ()):
        if route.intersection(crossing.footprint).length <= crossing.span.length / 2:
            continue
        if mode == "tram" or (
            (mode == "road" or crossing.pavement.is_empty)
            and route.coords[0] in endpoints[mode]
            and route.coords[-1] in endpoints[mode]
        ):
            yield route, mode, width


def connected_bridge_crossings(features, crossings):
    endpoints, routes = bridge_routes(features)
    return [
        crossing
        for crossing in crossings
        if any(crossing_routes(crossing, endpoints, routes))
    ]


def deck_bank_contacts(features, crossings, water):
    if water.is_empty or not crossings:
        return crossings
    endpoints, routes = bridge_routes(features)
    planned = []
    for crossing in crossings:
        deck, pavement = crossing.footprint, crossing.pavement
        extensions = []
        for route, mode, width in crossing_routes(crossing, endpoints, routes):
            if mode == "tram":
                continue
            connected = route_with_banks(route, endpoints[mode])
            nearby = crossing.footprint.buffer(width / 2, quad_segs=2)
            contact = route_area(connected, width).intersection(nearby)
            for extension in shapely.get_parts(
                contact.difference(deck, grid_size=1e-6)
            ):
                if not extension.intersects(water):
                    continue
                joined = deck.union(extension, grid_size=1e-6)
                if (
                    extension.intersection(water).area > 0
                    and extension.difference(water).area > 0
                    and joined.geom_type == "Polygon"
                ):
                    deck = joined
                    extensions.append(extension)
                    if mode == "road":
                        pavement = pavement.union(extension, grid_size=1e-6)
        if extensions:
            crossing = replace(
                crossing,
                footprint=shapely.orient_polygons(deck),
                pavement=pavement,
                bank_contact=shapely.union_all(extensions),
            )
        planned.append(crossing)
    return planned


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


def patch_mesh(footprint, terrain, lattice, spacing, *, breaks=()):
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
        breaks=breaks,
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


def graded_patch(
    mesh, footprint, exterior, core, crossings, road, track, shoulder, walkways
):
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
    limits[shapely.intersects(triangles, walkways)] = 0.08
    limits[shapely.intersects(triangles, track)] = 0.065
    blend = np.clip(
        shapely.distance(shapely.centroid(triangles), core) / shoulder, 0, 1
    )
    limits += (0.9 - limits) * blend
    frames = np.eye(2)[None] / limits[:, None, None]
    result = fit_surface_heights(mesh, frames, fixed=pins, budget=HeightBudget())
    return result.mesh


def bridge_junctions(features, crossings, domain, water, roads, vertices, terrain):
    served = connected_bridge_crossings(features, crossings)
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
    cores = [(crossing, approach_core(crossing)) for crossing in served]
    core = shapely.union_all([shape for _, shape in cores])
    pedestrian_core = shapely.union_all(
        [shape for crossing, shape in cores if crossing.pavement.is_empty]
    )
    walkways = shapely.union_all(
        [shape for name, shape in roads.items() if name != "road"]
    )
    pedestrian = walkways.intersection(pedestrian_core)
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
            mesh,
            piece,
            outline.boundary,
            core,
            served,
            carriageway_footprint(roads),
            track,
            shoulder,
            pedestrian,
        )
        if solved is None:
            return [], []
        products.append(BridgeJunction(piece, solved))
    return products, [crossing.span for crossing in served] if products else []
