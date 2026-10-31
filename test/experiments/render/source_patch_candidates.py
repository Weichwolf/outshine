"""Exact coplanar/attribute-affine original cohorts; no raster or World admission."""

from collections import Counter, defaultdict
from fractions import Fraction as F
from functools import reduce
from math import gcd

import numpy as np


def integer_positions(values):
    if (
        values.dtype != np.float32
        or values.ndim != 2
        or values.shape[1] != 3
        or not np.isfinite(values).all()
    ):
        raise ValueError("Original finite Float32 positions required")
    bits = values.view(np.uint32).reshape(-1)
    exponent = ((bits >> 23) & 255).astype(int)
    mantissa = bits & 0x7FFFFF
    mantissa = mantissa | np.where(exponent > 0, 1 << 23, 0).astype(np.uint32)
    power = np.where(exponent > 0, exponent - 150, -149)
    grid = int(power[mantissa != 0].min()) if np.any(mantissa) else 0
    result = [
        0 if not m else (-1 if word >> 31 else 1) * int(m) * (1 << int(p - grid))
        for word, m, p in zip(bits, mantissa, power, strict=True)
    ]
    return tuple(tuple(result[i : i + 3]) for i in range(0, len(result), 3)), grid


def plane(points):
    a, b, c = points
    u = tuple(x - y for x, y in zip(b, a, strict=True))
    v = tuple(x - y for x, y in zip(c, a, strict=True))
    n = tuple(
        u[(i + 1) % 3] * v[(i + 2) % 3] - u[(i + 2) % 3] * v[(i + 1) % 3]
        for i in range(3)
    )
    if not any(n):
        return None
    row = (*n, -sum(x * y for x, y in zip(n, a, strict=True)))
    divisor = reduce(gcd, row)
    orientation = int(next(x for x in n if x) < 0)
    sign = -1 if orientation else 1
    return tuple(sign * x // divisor for x in row), orientation


def input_parts(source):
    n = len(source["positions"])
    faces = source["indices"]
    if (
        not 1 <= n <= 131072
        or faces.ndim != 2
        or faces.shape[1] != 3
        or faces.dtype.kind not in "iu"
        or not 1 <= len(faces) <= 131072
        or faces.min() < 0
        or faces.max() >= n
        or not 1 <= len(source["parts"]) <= 256
        or len(source["parts"]) != len(source["attributes"])
    ):
        raise ValueError("Bounded complete original source parts required")
    attributes = []
    normals = []
    owners = []
    vertex = first = 0
    for index, (part, data) in enumerate(
        zip(source["parts"], source["attributes"], strict=True)
    ):
        count, triangles = part["vertices"], part["triangles"]
        if (
            type(count) is not int
            or type(triangles) is not int
            or count < 0
            or triangles < 0
        ):
            raise ValueError("Original nonnegative part capacities required")
        for key, width in (("normals", 3), ("uv", 2), ("uv1", 2), ("colours", 4)):
            value = data[key]
            if (
                value.dtype != np.float32
                or value.shape != (count, width)
                or not np.isfinite(value).all()
            ):
                raise ValueError(
                    "Complete original finite Float32 attribute rows required"
                )
        local = faces[first : first + triangles]
        if local.size and (local.min() < vertex or local.max() >= vertex + count):
            raise ValueError("Original triangles cross their declared part")
        attributes.append(np.c_[data["uv"], data["uv1"], data["colours"]])
        normals.append(data["normals"])
        owners.extend([index] * triangles)
        vertex += count
        first += triangles
    if vertex != n or first != len(faces):
        raise ValueError("Whole original part coverage required")
    return np.concatenate(attributes), np.concatenate(normals), owners


def affine_group(triangles, faces, points, fields, positions):
    used = np.unique(faces[triangles])
    for values in (fields[used], positions[used]):
        for column in values.T:
            zeros = column[column == 0].view(np.uint32)
            if len(np.unique(zeros)) > 1:
                return None, "mixed_signed_zero"
    a, b, c = (points[int(v)] for v in faces[triangles[0]])
    normal = plane((a, b, c))[0][:3]
    axes = tuple(
        i for i in range(3) if i != max(range(3), key=lambda j: abs(normal[j]))
    )
    x, y = axes
    dx1, dy1 = b[x] - a[x], b[y] - a[y]
    dx2, dy2 = c[x] - a[x], c[y] - a[y]
    determinant = dx1 * dy2 - dy1 * dx2
    coefficients = []
    for column in range(fields.shape[1]):
        values = fields[used, column]
        if np.all(values.view(np.uint32) == values.view(np.uint32)[0]):
            coefficients.append((F(0), F(0), F(float(values[0]))))
            continue
        av, bv, cv = (F(float(fields[int(v), column])) for v in faces[triangles[0]])
        gx = ((bv - av) * dy2 - (cv - av) * dy1) / determinant
        gy = (dx1 * (cv - av) - dx2 * (bv - av)) / determinant
        intercept = av - gx * a[x] - gy * a[y]
        if any(
            gx * points[int(v)][x] + gy * points[int(v)][y] + intercept
            != F(float(fields[int(v), column]))
            for v in used
        ):
            return None, "nonaffine_original_attribute"
        coefficients.append((gx, gy, intercept))
    return {
        "triangles": tuple(map(int, triangles)),
        "vertices": tuple(map(int, used)),
        "axes": axes,
        "coefficients": tuple(coefficients),
    }, None


def prepare(source):
    fields, normals, owners = input_parts(source)
    points, grid = integer_positions(source["positions"])
    point_ids = {}
    ids = [point_ids.setdefault(p, len(point_ids)) for p in points]
    parent = list(range(len(source["indices"])))

    def root(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    edges = {}
    keys = {}
    eligible = []
    fallback = []
    for index, face in enumerate(source["indices"]):
        vertices = list(map(int, face))
        normal_words = normals[face].view(np.uint32)
        key = plane(tuple(points[v] for v in vertices))
        if key is None:
            fallback.append((index, "degenerate_original"))
            continue
        if not np.all(normal_words == normal_words[0]):
            fallback.append((index, "varying_original_vertex_normal"))
            continue
        uv = fields[face, 0]
        code = (
            np.where(uv < 0, uv, np.floor(uv / np.float32(256)))
            .astype(np.float32)
            .view(np.uint32)
        )
        if not np.all(code == code[0]):
            fallback.append((index, "native_flat_facade_code_boundary"))
            continue
        key = (owners[index], key, tuple(map(int, normal_words[0])), int(code[0]))
        key = keys.setdefault(key, key)
        eligible.append(index)
        for a, b in (
            (vertices[0], vertices[1]),
            (vertices[1], vertices[2]),
            (vertices[2], vertices[0]),
        ):
            edge = (key, min(ids[a], ids[b]), max(ids[a], ids[b]))
            if edge in edges:
                parent[root(index)] = root(edges[edge])
            else:
                edges[edge] = index
    groups = defaultdict(list)
    for index in eligible:
        groups[root(index)].append(index)
    patches = []
    for faces in groups.values():
        patch, reason = affine_group(
            faces, source["indices"], points, fields, source["positions"]
        )
        if reason:
            fallback.extend((index, reason) for index in faces)
        else:
            patch["part"] = owners[faces[0]]
            patches.append(patch)
    if sum(len(p["triangles"]) for p in patches) + len(fallback) != len(
        source["indices"]
    ):
        raise ValueError("Every original triangle must remain represented")
    return {
        "original_source": source,
        "patches": tuple(patches),
        "fallback": tuple(sorted(fallback)),
        "integer_points": points,
        "point_ids": tuple(ids),
        "position_grid_exponent": grid,
        "work": {
            "original_triangles": len(source["indices"]),
            "original_vertices": len(points),
            "coplanar_attribute_groups": len(patches),
            "candidate_triangles": sum(len(p["triangles"]) for p in patches),
            "fallback_reasons": dict(Counter(reason for _index, reason in fallback)),
        },
        "scope": "Structural exact encoded-geometry/attribute candidate only. Borrowed original source must remain unchanged; no persisted product or native raster/BRDF/World admission.",
    }
