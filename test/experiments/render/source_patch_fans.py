"""Discover original interior fans; propose boundary-preserving retriangulation.

Discovery is not admission: surface_patch_overlay checks the whole mapped chart.
Source ordinals, parts, original triangle/vertex bindings remain explicit.
"""

from collections import Counter, defaultdict
from fractions import Fraction as F

import numpy as np
from source_patch_candidates import input_parts, integer_positions
from surface_patch_overlay import cross


def discover(source):
    fields, normals, owners = input_parts(source)
    points, _ = integer_positions(source["positions"])
    point_map = {}
    ids = [point_map.setdefault(p, len(point_map)) for p in points]
    incident = defaultdict(list)
    for i, face in enumerate(source["indices"]):
        n = normals[face].view("u4")
        uv = fields[face, 0]
        code = np.where(uv < 0, uv, np.floor(uv / np.float32(256))).astype("f4")
        if not np.all(n == n[0]) or not np.all(code.view("u4") == code.view("u4")[0]):
            continue
        key = (owners[i], tuple(map(int, n[0])), int(code.view("u4")[0]))
        for v in face:
            incident[(key, ids[int(v)])].append(i)
    fans = []
    for (key, centre), faces in incident.items():
        if not 3 <= len(faces) <= 16:
            continue
        radial = Counter()
        boundary = []
        for face in source["indices"][faces]:
            row = [ids[int(v)] for v in face]
            for a, b in zip(row, row[1:] + row[:1], strict=True):
                if centre in (a, b):
                    radial[min(a, b), max(a, b)] += 1
                else:
                    boundary.append((a, b))
        if not all(count == 2 for count in radial.values()):
            continue
        next_point = dict(boundary)
        if len(next_point) != len(faces) or len(set(next_point.values())) != len(faces):
            continue
        loop = [boundary[0][0]]
        while len(loop) <= len(boundary):
            following = next_point.get(loop[-1])
            if following is None or following in loop:
                break
            loop.append(following)
        if len(loop) != len(boundary) or next_point[loop[-1]] != loop[0]:
            continue
        fans.append(
            {
                "part": key[0],
                "centre_point_ordinal": centre,
                "triangle_ordinals": tuple(faces),
                "boundary_point_ordinals": tuple(loop),
            }
        )
    return tuple(fans), fields, normals, ids


def retriangulate(source, fan, fields, normals, point_ids):
    used = np.unique(source["indices"][list(fan["triangle_ordinals"])])
    representatives = {}
    for v in used:
        identity = point_ids[int(v)]
        previous = representatives.setdefault(identity, int(v))
        if not np.array_equal(fields[v].view("u4"), fields[previous].view("u4")):
            raise ValueError("Original varying seam retains fan")
    vertex_map = {int(v): i for i, v in enumerate(used)}
    old = np.array(
        [
            [vertex_map[int(v)] for v in face]
            for face in source["indices"][list(fan["triangle_ordinals"])]
        ],
        np.uint32,
    )
    boundary = [vertex_map[representatives[p]] for p in fan["boundary_point_ordinals"]]
    axis = int(np.abs(normals[used[0]]).argmax())
    axes = tuple(i for i in range(3) if i != axis)
    points = [tuple(F(float(source["positions"][v, a])) for a in axes) for v in used]
    area = sum(
        points[a][0] * points[b][1] - points[a][1] * points[b][0]
        for a, b in zip(boundary, boundary[1:] + boundary[:1], strict=True)
    )
    if not area:
        raise ValueError("Degenerate fan chart retains original")
    sign = 1 if area > 0 else -1
    ring, triangles = boundary.copy(), []
    while len(ring) > 3:
        for i, b in enumerate(ring):
            a, c = ring[i - 1], ring[(i + 1) % len(ring)]
            triangle = points[a], points[b], points[c]
            if sign * cross(*triangle) <= 0:
                continue
            if any(
                all(
                    sign * cross(x, y, points[p]) >= 0
                    for x, y in zip(triangle, (*triangle[1:], triangle[0]), strict=True)
                )
                for p in ring
                if p not in (a, b, c)
            ):
                continue
            triangles.append((a, b, c))
            ring.pop(i)
            break
        else:
            raise ValueError("Unsupported polygon retains original fan")
    triangles.append(tuple(ring))
    positions, values = source["positions"][used], fields[used]
    return (
        (positions, values, old),
        (positions, values, np.array(triangles, np.uint32)),
        axes,
    )
