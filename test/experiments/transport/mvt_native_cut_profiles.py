"""One exact-edge cut chain per observed native band, retaining all raw segments."""

import math
from itertools import pairwise

import numpy as np
from mvt_stair_native_crosscuts import cross_section


def validate_source_frame(cut):
    """Audit station/plane metadata; never move native points or relax contacts.

    The 1e-7 m agreement is the existing raw-section inspection precision. It
    applies only to derived source-frame metadata, not surface seams or welding.
    Positive tangent rescaling is allowed; direction follows the original axis.
    """
    axis = np.asarray(cut["original_source_xy_m"], float)
    center = np.asarray(cut["center_xy_m"], float)
    tangent = np.asarray(cut["tangent_xy"], float)
    station = cut["source_station_m"]
    if (
        axis.ndim != 2
        or axis.shape[1] != 2
        or not 2 <= len(axis) <= 256
        or center.shape != (2,)
        or tangent.shape != (2,)
        or not all(np.isfinite(a).all() for a in (axis, center, tangent))
        or not math.isfinite(station)
    ):
        raise ValueError("bounded finite original source frame required")
    lengths = [math.dist(a, b) for a, b in pairwise(axis)]
    if min(lengths) <= 0 or not all(math.isfinite(v) for v in lengths):
        raise ValueError("positive original source segments required")
    arc = np.r_[0, np.cumsum(lengths)]
    if not np.isfinite(arc).all() or not 0 <= station <= arc[-1]:
        raise ValueError("cut station must lie on its original source")
    index = min(len(axis) - 2, int(np.searchsorted(arc, station, side="right")) - 1)
    expected = (axis[index + 1] - axis[index]) / lengths[index]
    point = axis[index] + (station - arc[index]) * expected
    norm = math.hypot(*tangent)
    if (
        norm == 0
        or not math.isfinite(norm)
        or math.dist(point, center) > 1e-7
        or float(expected @ (tangent / norm)) <= 0
        or math.dist(expected, tangent / norm) * lengths[index] > 1e-7
    ):
        raise ValueError(
            "native cut plane must match its original source station on the original axis"
        )


def canonical_profile(cut):
    if not 1 <= len(cut["triangles"]) <= 128:
        raise ValueError("bounded native cut face set required")
    validate_source_frame(cut)
    raw = cross_section(
        cut["triangles"], cut["center_xy_m"], cut["tangent_xy"], cut["half_span_m"]
    )
    center = np.asarray(cut["center_xy_m"], float)
    tangent = np.array(cut["tangent_xy"], dtype=float, copy=True)
    tangent /= math.hypot(*tangent)
    normal = np.array([-tangent[1], tangent[0]])
    points, identities, edges = [], {}, []
    for triangle in cut["triangles"]:
        vertices = triangle["enu_m"]
        hit = []
        for first, second in zip(vertices, vertices[1:] + vertices[:1], strict=True):
            a, b = sorted((tuple(first), tuple(second)))
            da, db = ((np.asarray(p[:2]) - center) @ tangent for p in (a, b))
            if da == 0:
                identity, point = ("vertex", a), a
            elif db == 0:
                identity, point = ("vertex", b), b
            elif da * db < 0:
                identity = ("edge", a, b)
                point = np.asarray(a) + da / (da - db) * (np.asarray(b) - a)
            else:
                continue
            if identity not in identities:
                identities[identity] = len(points)
                points.append(np.asarray(point, float))
            if identities[identity] not in hit:
                hit.append(identities[identity])
        if len(hit) != 2:
            raise ValueError("each retained native face must cross in one segment")
        hit.sort(key=lambda v: (points[v][:2] - center) @ normal)
        edges.append((hit, triangle["page"], triangle["triangle"]))
    edges.sort(key=lambda e: (points[e[0][0]][:2] - center) @ normal)
    for a, b in pairwise(edges):
        if a[0][1] != b[0][0]:
            raise ValueError("native cut has no exact shared-edge chain")
    ordered = np.array([points[edges[0][0][0]], *[points[e[0][1]] for e in edges]])
    offsets = (ordered[:, :2] - center) @ normal
    if not offsets[0] < 0 < offsets[-1] or np.any(np.diff(offsets) <= 0):
        raise ValueError("native band must enclose the original-axis cut point")
    index = int(np.searchsorted(offsets, 0))
    if offsets[index] != 0:
        fraction = -offsets[index - 1] / (offsets[index] - offsets[index - 1])
        axis = ordered[index - 1] + fraction * (ordered[index] - ordered[index - 1])
        ordered = np.insert(ordered, index, axis, axis=0)
        offsets = np.insert(offsets, index, 0.0)
    return {
        "source_id": f"native-part:{cut['native_way']}",
        "native_way": cut["native_way"],
        "source_station_m": cut["source_station_m"],
        "original_source_xy_m": np.array(cut["original_source_xy_m"]).tolist(),
        "center_xy_m": center.tolist(),
        "tangent_xy": tangent.tolist(),
        "profile_xyz_m": ordered.tolist(),
        "offsets_m": offsets.tolist(),
        "axis_vertex": index,
        "raw_section": raw,
        "native_faces": [[e[1], e[2]] for e in edges],
        "work": {
            "triangles": len(cut["triangles"]),
            "cut_points": len(points),
            "profile_points": len(ordered),
            "profile_array_bytes": ordered.nbytes + offsets.nbytes,
        },
        "scope": "Canonical intersections of identical native geometric edges, not provider-node identity or replacement admission. Raw inspected segments remain unchanged.",
    }


def cut_profiles(case):
    if not 1 <= len(case["cuts"]) <= 64:
        raise ValueError("bounded nonempty native cut set required")
    result = [canonical_profile(c) for c in case["cuts"]]
    keys = [(r["source_id"], r["source_station_m"]) for r in result]
    if len(set(keys)) != len(keys):
        raise ValueError("unique source/cut station required")
    return result
