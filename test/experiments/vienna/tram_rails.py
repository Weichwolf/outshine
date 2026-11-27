"""Embedded tram rail heads on the same fitted surfaces as their carriageways."""

from collections import defaultdict

import numpy as np
import shapely
from bridges import cap_mesh
from ground_surface import project_triangles
from outshine_ref.transport.rail_hardware import RailRecipe

RAIL_HEAD_WIDTH_M = 0.072
RAIL_HEAD_RISE_M = 0.007


def bank_route(port, layer, neighbors):
    route = []
    visited = {port}
    while True:
        candidates = [
            (line, level)
            for line, level in neighbors[port]
            if line.coords[-1] not in visited
        ]
        elevated = [line for line, level in candidates if level == layer and layer != 0]
        ground = [line for line, level in candidates if level == 0]
        if len(elevated) != 1:
            if elevated or len(ground) != 1:
                return None
            return [*route, ground[0]]
        line = elevated[0]
        route.append(line)
        port = line.coords[-1]
        visited.add(port)


def tram_routes(features, crossings):
    carried, neighbors = {}, defaultdict(list)
    seen = set()
    for name, shape, tags in features:
        if name != "transportation" or tags.get("subclass") != "tram":
            continue
        bridge = tags.get("brunnel")
        if bridge not in (None, "bridge"):
            continue
        layer = tags.get("layer", 1 if bridge else 0)
        for line in shapely.get_parts(shape):
            if line.geom_type != "LineString" or line.is_empty:
                continue
            key = shapely.normalize(line), layer, bridge
            if key in seen:
                continue
            seen.add(key)
            if bridge:
                carried.setdefault(layer, []).append(line)
            else:
                neighbors[line.coords[0]].append((line, layer))
                neighbors[line.coords[-1]].append((shapely.reverse(line), layer))
    result = []
    for crossing in crossings:
        for line in carried.get(crossing.layer, ()):
            if line.intersection(crossing.footprint).length <= max(
                crossing.span.length / 2, line.length / 2
            ):
                continue
            a = bank_route(line.coords[0], crossing.layer, neighbors)
            b = bank_route(line.coords[-1], crossing.layer, neighbors)
            if a is None or b is None:
                continue
            chain = [*(shapely.reverse(part) for part in reversed(a)), line, *b]
            coords = [
                *chain[0].coords,
                *(xy for part in chain[1:] for xy in part.coords[1:]),
            ]
            result.append((crossing, shapely.LineString(coords)))
    return result


def rail_head_footprints(routes):
    centre = (RailRecipe().gauge_m + RAIL_HEAD_WIDTH_M) / 2
    lines = np.array([line for _, line in routes], dtype=object)
    offsets = shapely.offset_curve(
        lines[:, None], [-centre, centre], join_style="round"
    )
    return shapely.get_parts(
        shapely.buffer(offsets.ravel(), RAIL_HEAD_WIDTH_M / 2, cap_style="flat")
    )


def project_rail_heads(meshes, junction, heads):
    mesh = junction.mesh
    parent = mesh.vertices[mesh.triangles]
    triangles = shapely.polygons(parent[:, :, :2])
    rail, target = shapely.STRtree(triangles).query(heads, predicate="intersects")
    cells = shapely.intersection(heads[rail], triangles[target], grid_size=1e-6)
    parts, owners = shapely.get_parts(cells, return_index=True)
    valid = (shapely.get_type_id(parts) == 3) & (shapely.area(parts) > 1e-10)
    if not valid.any():
        return
    xy, faces = project_triangles(parts[valid], parent[target[owners[valid]]])
    xy[:, 2] += RAIL_HEAD_RISE_M
    meshes.add("rail head", xy, faces)


def draw_tram_rails(meshes, routes, junctions):
    if not routes:
        return
    heads = rail_head_footprints(routes)
    index = shapely.STRtree(heads)
    for junction in junctions:
        selected = index.query(junction.footprint, predicate="intersects")
        project_rail_heads(meshes, junction, heads[selected])
    for crossing in {id(crossing.span): crossing for crossing, _ in routes}.values():
        selected = index.query(crossing.footprint, predicate="intersects")
        shapes = shapely.union_all(heads[selected]).intersection(
            crossing.footprint, grid_size=1e-6
        )
        vertices, faces = cap_mesh(
            shapes, lambda xy, span=crossing.span: span.height(xy) + RAIL_HEAD_RISE_M
        )
        meshes.add("rail head", vertices, faces)
