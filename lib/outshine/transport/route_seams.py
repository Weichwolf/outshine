"""Pair uniquely supported continuations across adjacent MVT tile cores."""

from bisect import bisect_left, bisect_right
from collections import defaultdict
from dataclasses import dataclass
from math import ceil

from .route_tiles import WORLD_CELLS

UNANCHORED_BASELINE_CELLS = 8


@dataclass(frozen=True, slots=True)
class SeamEnd:
    path: int
    index: int
    point: tuple[int, int]
    observed: bool
    cell: float
    support: tuple[tuple[int, int], tuple[int, int]] | None

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


def _wrapped_east(delta):
    return (delta + WORLD_CELLS // 2) % WORLD_CELLS - WORLD_CELLS // 2


def _support_in_frame(end, origin, axis):
    points = ((_wrapped_east(x - origin[0]), y - origin[1]) for x, y in end.support)
    return sorted((point[axis], point[1 - axis]) for point in points)


def _along_at(distance, first, last):
    return first[1] + (last[1] - first[1]) * (distance - first[0]) / (
        last[0] - first[0]
    )


def _overlapping_support(end, peer, axis, tolerance):
    """Resolve the tangent over a longer baseline without a shared source vertex."""
    left, right = _support_in_frame(end, end.point, axis)
    near, far = _support_in_frame(peer, end.point, axis)
    low, high = max(left[0], near[0]), min(right[0], far[0])
    anchored = any(point in peer.support for point in end.support)
    reach = tolerance if anchored else UNANCHORED_BASELINE_CELLS * tolerance
    return (
        low <= -reach
        and high >= reach
        and all(
            abs(_along_at(distance, left, right) - _along_at(distance, near, far))
            <= tolerance
            for distance in (low, high)
        )
    )


def _candidates(side, other, axis, one_way, buffered):
    along = 1 - axis
    other.sort(key=lambda end: end.point[along])
    positions = [peer.point[along] for peer in other]
    widest = max(peer.cell for peer in other)
    for end in side:
        position = end.point[along]
        window = ceil(max(end.cell, widest))
        start = bisect_left(positions, position - window)
        stop = bisect_right(positions, position + window)
        for j in range(start, stop):
            peer = other[j]
            if one_way and end.index == peer.index:
                continue
            tolerance = max(end.cell, peer.cell)
            maximum = (
                tolerance
                if buffered
                else 0
                if end.observed and peer.observed
                else ceil(tolerance)
            )
            if abs(position - positions[j]) <= maximum and (
                not buffered or _overlapping_support(end, peer, axis, tolerance)
            ):
                yield end, peer


def match_seams(fragments):
    source_ids = defaultdict(lambda: ([], []))
    common_segments = defaultdict(lambda: ([], []))
    buffered_segments = defaultdict(lambda: ([], []))
    boundary = {}
    for path, fragment in enumerate(fragments):
        for index in (0, -1):
            vertex = fragment.vertices[index]
            for seam, side, peer in _tile_sides(fragment.tile, vertex.point):
                inward = fragment.vertices[1 if index == 0 else -2].point
                if inward[seam[0]] == vertex.point[seam[0]]:
                    continue
                end = SeamEnd(
                    path,
                    index,
                    vertex.point,
                    vertex.observed,
                    fragment.cell,
                    vertex.support,
                )
                boundary.setdefault(end, set()).add(peer)
                if fragment.feature:
                    source_ids[seam, fragment.feature, fragment.tags][side > 0].append(
                        end
                    )
                if vertex.support is not None:
                    common_segments[seam, vertex.support, fragment.tags][
                        side > 0
                    ].append(end)
                    buffered_segments[seam, None, fragment.tags][side > 0].append(end)

    matches = {}
    reserved = _resolve(common_segments, matches)
    reserved.update(_resolve(source_ids, matches))
    _resolve(buffered_segments, matches, buffered=True, excluded=reserved)
    open_ends = {
        end.key: tuple(sorted(peers))
        for end, peers in boundary.items()
        if end.key not in matches
    }
    return matches, open_ends


def _resolve(groups, matches, *, buffered=False, excluded=()):
    proposals = defaultdict(set)
    for ((axis, _position, _line), _evidence, tags), (
        negative,
        positive,
    ) in groups.items():
        negative = [
            end
            for end in negative
            if end.key not in matches and end.key not in excluded
        ]
        positive = [
            end
            for end in positive
            if end.key not in matches and end.key not in excluded
        ]
        if not negative or not positive:
            continue
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
        for end, peer in _candidates(negative, positive, axis, oneway, buffered):
            proposals[end].add(peer)
            proposals[peer].add(end)

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
    return {end.key for end in proposals}
