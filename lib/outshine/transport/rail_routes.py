"""Trace ground rail chains through proven graph nodes and tile joins."""

from collections import defaultdict

import numpy as np
from shapely import LineString

from .route_tiles import geography


def _node_key(node):
    return node.point, node.domain, node.cut or ()


def surface_rail_chains(graph, *, kind="rail", mode=None):
    links = defaultdict(set)
    for path in graph.paths:
        if path.attributes.kind != kind or path.attributes.domain[1:] != (
            0,
            "",
            "",
            False,
        ):
            continue
        if mode is not None and path.attributes.domain[0] != mode:
            continue
        for a, b in zip(path.nodes[:-1], path.nodes[1:], strict=True):
            links[a].add(b)
            links[b].add(a)
    used = set()
    ordered = sorted(links, key=lambda node: (len(links[node]) == 2, _node_key(node)))
    for first in ordered:
        for second in sorted(links[first], key=_node_key):
            if (first, second) in used:
                continue
            chain = [first]
            previous, node = first, second
            while (previous, node) not in used:
                used.update(((previous, node), (node, previous)))
                chain.append(node)
                if len(links[node]) != 2:
                    break
                (following,) = links[node] - {previous}
                previous, node = node, following
            if chain[0] != chain[-1]:
                yield chain


def surface_rail_lines(graph, project, *, kind="rail", mode=None, curves=None):
    for chain in surface_rail_chains(graph, kind=kind, mode=mode):
        lonlat = np.asarray([geography(node.point) for node in chain])
        if curves is not None and len(chain) > 2:
            points = [lonlat[:1]]
            for index, (before, middle, after) in enumerate(
                zip(chain, chain[1:], chain[2:]), start=1
            ):
                turn = curves(before, middle, after)
                points.append(lonlat[index : index + 1] if turn is None else turn)
            lonlat = np.concatenate([*points, lonlat[-1:]])
        line = LineString(project(*lonlat.T))
        if not line.is_ring and line.is_simple:
            yield line
