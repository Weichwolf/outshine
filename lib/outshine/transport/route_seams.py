"""Pair uniquely supported continuations across adjacent MVT tile cores."""

from collections import defaultdict
from dataclasses import dataclass
from math import ceil

from .route_tiles import WORLD_CELLS


@dataclass(frozen=True, slots=True)
class SeamEnd:
    path: int
    index: int
    point: tuple[int, int]
    observed: bool
    cell: float

    @property
    def key(self):
        return self.path, self.index


def _tile_sides(tile, point):
    zoom, x, y = tile
    side = 1 << zoom
    stride = WORLD_CELLS >> zoom
    for axis, position, tile_index in zip((0, 1), point, (x, y), strict=True):
        if position == tile_index * stride:
            offset = -1
        elif position == (tile_index + 1) * stride:
            offset = 1
        else:
            continue
        tile_peer = tile_index + offset
        if axis == 0:
            yield (axis, position % WORLD_CELLS, y), offset, (zoom, tile_peer % side, y)
        elif 0 <= tile_peer < side:
            yield (axis, position, x), offset, (zoom, x, tile_peer)


def _match(side, other, axis, one_way):
    along = 1 - axis
    for end in side:
        candidates = [
            peer
            for peer in other
            if (not one_way or end.index != peer.index)
            and abs(end.point[along] - peer.point[along])
            <= (0 if end.observed and peer.observed else ceil(max(end.cell, peer.cell)))
        ]
        if len(candidates) == 1:
            yield end, candidates[0]


def match_seams(fragments):
    source_ids = defaultdict(lambda: ([], []))
    common_segments = defaultdict(lambda: ([], []))
    boundary = {}
    for path, fragment in enumerate(fragments):
        for index in (0, -1):
            vertex = fragment.vertices[index]
            for seam, side, peer in _tile_sides(fragment.tile, vertex.point):
                inward = fragment.vertices[1 if index == 0 else -2].point
                if inward[seam[0]] == vertex.point[seam[0]]:
                    continue
                end = SeamEnd(path, index, vertex.point, vertex.observed, fragment.cell)
                boundary.setdefault(end, set()).add(peer)
                if fragment.feature:
                    source_ids[seam, fragment.feature, fragment.tags][side > 0].append(
                        end
                    )
                if vertex.support is not None:
                    common_segments[seam, vertex.support, fragment.tags][
                        side > 0
                    ].append(end)

    matches = {}
    _resolve(common_segments, matches)
    _resolve(source_ids, matches)
    open_ends = {
        end.key: tuple(sorted(peers))
        for end, peers in boundary.items()
        if end.key not in matches
    }
    return matches, open_ends


def _resolve(groups, matches):
    proposals = defaultdict(set)
    for ((axis, _position, _line), _evidence, tags), (
        negative,
        positive,
    ) in groups.items():
        if not negative or not positive:
            continue
        negative = [end for end in negative if end.key not in matches]
        positive = [end for end in positive if end.key not in matches]
        attributes = dict(tags)
        oneway = attributes.get("oneway", attributes.get("class") == "motorway") in (
            1,
            -1,
            "1",
            "-1",
            "yes",
            "true",
            "reverse",
        )
        for end, peer in (
            *_match(negative, positive, axis, oneway),
            *_match(positive, negative, axis, oneway),
        ):
            proposals[end].add(peer)

    for end, peers in proposals.items():
        if len(peers) != 1:
            continue
        (peer,) = peers
        if proposals.get(peer) != {end}:
            continue
        position = (
            end.point
            if end.observed
            else peer.point
            if peer.observed
            else (
                (end.point[0] + peer.point[0]) // 2,
                (end.point[1] + peer.point[1]) // 2,
            )
        )
        if abs(end.point[0] - peer.point[0]) == WORLD_CELLS:
            position = 0, position[1]
        first = min(end.key, peer.key)
        matches[end.key] = (
            position,
            None if end.observed or peer.observed else first,
            peer.path,
        )
