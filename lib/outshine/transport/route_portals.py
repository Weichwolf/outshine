"""Qualify mapped outdoor approaches to bridges and tunnels."""

from __future__ import annotations

from collections import Counter, defaultdict
from dataclasses import dataclass
from typing import TYPE_CHECKING

from .route_tiles import WORLD_CELLS

if TYPE_CHECKING:
    from .route_graph import RouteNode


@dataclass(frozen=True, slots=True)
class RoutePortal:
    ground: RouteNode
    structure: RouteNode
    ground_next: tuple[int, int]
    structure_next: tuple[int, int]
    enter_modes: frozenset[str]
    exit_modes: frozenset[str]


def _structure_end(node):
    _, level, role, floor, indoor = node.domain
    if node.cut is not None or level is None or floor or indoor:
        return False
    return (role == "bridge" and level >= 0) or (role == "tunnel" and level <= 0)


def _spokes(paths, points):
    spokes = defaultdict(dict)
    for path in paths:
        forward = path.attributes.modes_in(1)
        backward = path.attributes.modes_in(-1)
        for a, b in zip(path.nodes[:-1], path.nodes[1:], strict=True):
            for node, peer, incoming, outgoing in (
                (a, b, backward, forward),
                (b, a, forward, backward),
            ):
                if node.cut is not None or node.point not in points:
                    continue
                if node.domain[1:] != (0, "", "", False) and not _structure_end(node):
                    continue
                arrival, departure = spokes[node].setdefault(peer.point, (set(), set()))
                arrival.update(incoming)
                departure.update(outgoing)
    return spokes


def _opposite(origin, a, b):
    half = WORLD_CELLS // 2
    dx = (a[0] - origin[0] + half) % WORLD_CELLS - half
    ex = (b[0] - origin[0] + half) % WORLD_CELLS - half
    return dx * ex + (a[1] - origin[1]) * (b[1] - origin[1]) < 0


def route_portals(paths):
    """Link the source topology at clear mouths. Grade and clearance belong to the surface fit."""
    ends = {
        node
        for path in paths
        for node in (path.nodes[0], path.nodes[-1])
        if _structure_end(node)
    }
    if not ends:
        return ()
    spokes = _spokes(paths, {node.point for node in ends})
    ground = {
        (node.point, node.domain[0]): node
        for node in spokes
        if node.domain[1:] == (0, "", "", False)
    }
    candidates = []
    for structure in sorted(ends, key=lambda node: (node.point, node.domain)):
        approaches = spokes[structure]
        land = ground.get((structure.point, structure.domain[0]))
        if land is None or len(approaches) != 1:
            continue
        ((structure_next, (arrive, depart)),) = approaches.items()
        choices = []
        for ground_next, (incoming, outgoing) in spokes[land].items():
            enter, leave = incoming & depart, arrive & outgoing
            if (enter or leave) and _opposite(land.point, ground_next, structure_next):
                choices.append(
                    RoutePortal(
                        land,
                        structure,
                        ground_next,
                        structure_next,
                        frozenset(enter),
                        frozenset(leave),
                    )
                )
        if len(choices) == 1:
            candidates.extend(choices)
    counts = Counter(portal.ground for portal in candidates)
    return tuple(portal for portal in candidates if counts[portal.ground] == 1)
