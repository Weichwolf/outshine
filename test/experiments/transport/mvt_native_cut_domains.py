"""Bounded variable-offset ribbons with unchanged native transverse vertices."""

import numpy as np
import shapely


def cut_ribbon(axis, first, last):
    axis = np.asarray(axis, float)
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(axis, axis=0), axis=1))]
    a, b = first["source_station_m"], last["source_station_m"]
    if not 0 <= a < b <= arc[-1] or len(axis) > 256:
        raise ValueError("bounded increasing cuts within the original source required")
    profile_a, profile_b = (np.array(p["profile_xyz_m"]) for p in (first, last))
    inside = (arc > a) & (arc < b)
    xy = np.vstack(
        [
            profile_a[first["axis_vertex"], :2],
            axis[inside],
            profile_b[last["axis_vertex"], :2],
        ]
    )
    stations = np.r_[a, arc[inside], b]
    tangent = np.diff(xy, axis=0)
    length = np.linalg.norm(tangent, axis=1)
    if np.any(length <= 0):
        raise ValueError("distinct constructed axis stations required")
    tangent /= length[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    miter = np.vstack([normal[0], (normal[:-1] + normal[1:]) / 2, normal[-1]])
    denominator = np.einsum("ij,ij->i", miter[1:-1], normal[:-1])
    if np.any(denominator <= 0.25):
        raise ValueError("bounded ribbon needs a separate sharp-turn recipe")
    miter[1:-1] /= denominator[:, None]
    sides = []
    for at in (0, -1):
        offsets = np.interp(
            stations, [a, b], [first["offsets_m"][at], last["offsets_m"][at]]
        )
        sides.append(xy + miter * offsets[:, None])
    ring = np.vstack(
        [profile_a[:, :2], sides[1][1:-1], profile_b[::-1, :2], sides[0][-2:0:-1]]
    )
    domain = shapely.Polygon(ring)
    if (
        not domain.is_valid
        or domain.area <= 0
        or not domain.covers(shapely.LineString(xy))
    ):
        raise ValueError(
            "native-cut ribbon must be valid and contain its constructed axis"
        )
    return (
        xy,
        domain,
        {
            "source_station_range_m": [a, b],
            "axis_points": len(xy),
            "boundary_points": len(ring),
            "area_m2": domain.area,
            "maximum_miter": float(np.linalg.norm(miter, axis=1).max()),
            "scope": "Constructed bounded ribbon; complete original source is retained separately. Native transverse vertices are not rounded.",
        },
    )
