"""Build attributed route incidence from original vertices and resolved tile seams."""

from collections import defaultdict
from dataclasses import dataclass
from functools import cache

from .route_seams import match_seams
from .route_tiles import WORLD_CELLS, tile_fragments

ROADS = {
    "motorway",
    "trunk",
    "primary",
    "secondary",
    "tertiary",
    "minor",
    "service",
    "track",
    "busway",
    "bus_guideway",
    "raceway",
}
RAILS = {"rail", "transit"}
NON_PUBLIC = {
    "no",
    "private",
    "destination",
    "customers",
    "delivery",
    "agricultural",
    "forestry",
}


@dataclass(frozen=True, slots=True)
class RouteAttributes:
    kind: str
    domain: tuple[str, int | None, str, str, bool]
    directions: tuple[int, ...]
    public_modes: frozenset[str]
    tags: tuple


@dataclass(frozen=True, slots=True)
class RouteNode:
    point: tuple[int, int]
    domain: tuple[str, int | None, str, str, bool]
    cut: tuple | None = None


@dataclass(frozen=True, slots=True)
class RoutePath:
    source: tuple[tuple[int, int, int], int]
    attributes: RouteAttributes
    nodes: tuple[RouteNode, ...]


@dataclass(frozen=True, slots=True)
class RouteContact:
    road: RouteNode
    rail: RouteNode


@dataclass(frozen=True, slots=True)
class RouteFrontier:
    source: tuple[tuple[int, int, int], int]
    node: RouteNode
    adjacent_tiles: tuple[tuple[int, int, int], ...]


@dataclass(frozen=True, slots=True)
class RouteGraph:
    paths: tuple[RoutePath, ...]
    contacts: tuple[RouteContact, ...]
    frontiers: tuple[RouteFrontier, ...]
    seam_pairs: tuple[tuple[int, int], ...]

    def connections(self, mode):
        """Yield directed traversals. Mode changes need a separate site recipe."""
        emitted = set()
        for path in self.paths:
            if mode not in path.attributes.public_modes:
                continue
            directions = (
                (1, -1) if mode in {"walk", "horse"} else path.attributes.directions
            )
            for a, b in zip(path.nodes[:-1], path.nodes[1:], strict=True):
                for direction in directions:
                    edge = (a, b) if direction > 0 else (b, a)
                    if edge not in emitted:
                        emitted.add(edge)
                        yield edge


def _mode_defaults(kind, subclass):
    if kind in ROADS:
        if kind in {"motorway", "raceway"}:
            return {"motor"}
        if kind in {"busway", "bus_guideway"}:
            return {"bus"}
        return {"motor", "walk", "bicycle"}
    if kind == "path":
        if subclass == "cycleway":
            return {"bicycle"}
        if subclass == "bridleway":
            return {"horse"}
        return {"walk", "bicycle"} if subclass in {"path", ""} else {"walk"}
    if kind in RAILS:
        return {subclass or "rail"}
    return {"lift"} if kind == "aerialway" else {"ferry"} if kind == "ferry" else set()


def _attributes(tags):
    properties = dict(tags)
    raw_kind = str(properties.get("class", ""))
    kind = raw_kind.removesuffix("_construction")
    subclass = str(properties.get("subclass", ""))
    if kind in ROADS or kind == "path":
        mode = "land"
    elif kind in RAILS:
        mode = subclass or "rail"
    else:
        mode = raw_kind
    structure = properties.get("brunnel", "")
    brunnel = structure if structure in {"bridge", "tunnel"} else ""
    indoor = properties.get("indoor") in (1, "1", "yes", "true")
    floor = str(properties.get("level", ""))
    if floor in {"0", "0.0"} and not indoor:
        floor = ""
    raw_layer = str(properties.get("layer", 0))
    level = int(raw_layer) if raw_layer.removeprefix("-").isdecimal() else None
    domain = mode, level, brunnel, floor, indoor
    direction = properties.get("oneway")
    if direction is None and kind == "motorway":
        direction = 1
    if direction in (1, "1", "yes", "true"):
        directions = (1,)
    elif direction in (-1, "-1", "reverse"):
        directions = (-1,)
    else:
        directions = (1, -1)

    modes = _mode_defaults(kind, subclass)
    if properties.get("access") in NON_PUBLIC:
        modes.clear()
    for tag, mode in (("foot", "walk"), ("bicycle", "bicycle"), ("horse", "horse")):
        access = properties.get(tag)
        if access in NON_PUBLIC:
            modes.discard(mode)
        if (
            access in {"yes", "designated", "permissive", "official"}
            and domain[0] == "land"
        ):
            modes.add(mode)
    if level is None or raw_kind.endswith("_construction"):
        modes.clear()
    return RouteAttributes(raw_kind, domain, directions, frozenset(modes), tags)


def _endpoint_key(fragments, path, index):
    fragment = fragments[path]
    inside = fragment.vertices[1 if index == 0 else -2]
    return fragment.tile, fragment.feature, fragment.vertices[index].point, inside.point


def _node(fragments, path, index, domain, seams):
    vertices = fragments[path].vertices
    vertex = vertices[index]
    endpoint = -1 if index == len(vertices) - 1 else index
    point, paired, _ = seams.get((path, endpoint), (vertex.point, None, None))
    key = _endpoint_key(fragments, *paired) if paired is not None else None
    if not vertex.observed and (path, endpoint) not in seams:
        key = _endpoint_key(fragments, path, endpoint)
    return RouteNode((point[0] % WORLD_CELLS, point[1]), domain, key)


def _level_crossings(paths):
    roads, railway = defaultdict(set), defaultdict(set)
    for path in paths:
        if path.attributes.domain[1:] != (0, "", "", False):
            continue
        if path.attributes.kind in ROADS:
            incidence = roads
        elif path.attributes.kind in RAILS:
            incidence = railway
        else:
            continue
        for a, b in zip(path.nodes[:-1], path.nodes[1:], strict=True):
            if a.cut is None:
                incidence[a].add(b.point)
            if b.cut is None:
                incidence[b].add(a.point)
    road_vertices = {n.point: n for n, adjacent in roads.items() if len(adjacent) >= 2}
    return tuple(
        RouteContact(road_vertices[n.point], n)
        for n, adjacent in sorted(
            railway.items(), key=lambda item: (item[0].point, item[0].domain)
        )
        if len(adjacent) >= 2 and n.point in road_vertices
    )


def route_graph(tiles, *, bounds=None):
    """Resolve topology from one provider snapshot and zoom, independent of load order."""
    if len({key[0] for key in tiles}) > 1:
        raise ValueError("transport fragments must use one zoom")
    fragments = tuple(
        fragment
        for tile, layer in sorted(tiles.items())
        for fragment in sorted(
            tile_fragments(tile, layer, bounds),
            key=lambda f: (
                f.feature,
                tuple((v.point, v.support or ()) for v in f.vertices),
                repr(f.tags),
            ),
        )
    )
    seams, unresolved = match_seams(fragments)
    attributes_for = cache(_attributes)
    paths = []
    for index, fragment in enumerate(fragments):
        attributes = attributes_for(fragment.tags)
        nodes = tuple(
            _node(fragments, index, vertex, attributes.domain, seams)
            for vertex in range(len(fragment.vertices))
        )
        paths.append(RoutePath((fragment.tile, fragment.feature), attributes, nodes))
    frontiers = tuple(
        RouteFrontier(paths[path].source, paths[path].nodes[index], peers)
        for (path, index), peers in sorted(unresolved.items())
        if paths[path].nodes[index].cut is not None
        or any(peer not in tiles for peer in peers)
    )
    seam_pairs = tuple(
        sorted(
            {(path, peer) for (path, _), (_, _, peer) in seams.items() if path < peer}
        )
    )
    return RouteGraph(tuple(paths), _level_crossings(paths), frontiers, seam_pairs)
