"""Visible faces of overlapping prismatic building parts."""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely.geometry import LineString, box

FACE_EPSILON_M = 1e-6


@dataclass(frozen=True)
class BuildingVolume:
    footprint: shapely.Polygon
    bottom: float
    top: float


def building_neighbours(footprints, volumes):
    if not footprints:
        return []
    owned = [(i, volume) for i, parts in enumerate(volumes) for volume in parts]
    shapes = [part.footprint for _, part in owned]
    index = shapely.STRtree(shapes)
    result = [[] for _ in footprints]
    rank = [(shape.area, i) for i, shape in enumerate(footprints)]
    for i, j in index.query(footprints, predicate="intersects").T:
        owner, volume = owned[j]
        if i != owner:
            result[i].append((volume, rank[owner] < rank[i]))
    return result


def obstructed_spans(line, footprint, origin, axis, outward, length, owns_face):
    if not footprint.intersects(line):
        return []
    intersections = shapely.get_coordinates(footprint.boundary.intersection(line))
    stations = np.unique(
        np.r_[0, np.clip((intersections - origin) @ axis, 0, length), length]
    )
    begin, end = stations[:-1], stations[1:]
    middle = origin + ((begin + end) * 0.5)[:, None] * axis
    probe = FACE_EPSILON_M * outward
    blocked = shapely.contains_xy(footprint, *(middle + probe).T)
    if owns_face:
        blocked |= shapely.contains_xy(
            footprint, *(middle - probe).T
        ) & shapely.dwithin(footprint.boundary, shapely.points(middle), FACE_EPSILON_M)
    return zip(begin[blocked], end[blocked], strict=True)


def exposed_wall(a, b, bottom, top, neighbours):
    """Return a clipped (along-wall metres, elevation) polygon or None for a full wall."""
    length = np.linalg.norm(b - a)
    if length <= FACE_EPSILON_M or not neighbours:
        return None
    axis = (b - a) / length
    outward = np.array([axis[1], -axis[0]])
    line = LineString([a, b])
    cuts = []
    for volume, owns_face in neighbours:
        low = max(bottom, volume.bottom)
        high = min(top, volume.top)
        if high - low <= FACE_EPSILON_M:
            continue
        cuts.extend(
            box(start, low, stop, high)
            for start, stop in obstructed_spans(
                line, volume.footprint, a, axis, outward, length, owns_face
            )
            if stop - start > FACE_EPSILON_M
        )
    if cuts:
        return box(0, bottom, length, top).difference(shapely.union_all(cuts))
    return None


def exposed_cap(footprint, height, neighbours, *, underside=False):
    hidden = []
    for volume, owns_face in neighbours:
        low = height - volume.bottom
        high = volume.top - height
        inward, outward = (high, low) if underside else (low, high)
        if inward >= -FACE_EPSILON_M and (
            outward > FACE_EPSILON_M
            or (owns_face and outward >= -FACE_EPSILON_M and inward > FACE_EPSILON_M)
        ):
            hidden.append(volume.footprint)
    return footprint.difference(shapely.union_all(hidden)) if hidden else footprint
