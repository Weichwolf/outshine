"""One exact-edge cut chain per observed native band, retaining all raw segments."""

import math
from itertools import pairwise

import numpy as np
from mvt_stair_native_crosscuts import cross_section


def canonical_profile(cut):
    if not 1 <= len(cut["triangles"]) <= 128:
        raise ValueError("bounded native cut face set required")
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
