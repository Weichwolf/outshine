"""Compare complete source surfaces through bounded exact chart correspondences.

Material slots, constant original normal words, flat facade codes and winding
separate components. Bounds are numeric geometry/linear-varying correspondence,
not native signed-word, raster, derivative, PBR or World replacement admission.
"""

from collections import defaultdict
from fractions import Fraction as F

import numpy as np
from source_patch_candidates import integer_positions
from surface_patch_envelope import compare_envelope
from surface_patch_overlay import compare


def validate(corners, materials):
    if (
        corners.dtype != np.float32
        or corners.ndim != 3
        or corners.shape[1:] != (3, 14)
        or not 1 <= len(corners) <= 256
        or not np.isfinite(corners).all()
        or materials.ndim != 1
        or materials.shape != (len(corners),)
        or materials.dtype.kind not in "iu"
        or materials.min() < 0
        or materials.max() > 2
    ):
        raise ValueError(
            "Bounded whole original/candidate corners and native material slots required"
        )


def primitive_words(corners, materials):
    rows = np.c_[corners.reshape(-1, 42), materials.astype(np.float32)]
    return np.sort(np.ascontiguousarray(rows, dtype="<f4").view("V172").ravel())


def face_key(face, points, material):
    normals = face[:, 3:6].copy().view("u4")
    if not np.all(normals == normals[0]) or not np.any(face[0, 3:6]):
        raise ValueError("Varying/zero normal requires another surface chart")
    uv = face[:, 6]
    code = np.where(uv < 0, uv, np.floor(uv / np.float32(256))).astype("f4").view("u4")
    if not np.all(code == code[0]):
        raise ValueError(
            "Native flat facade-code boundary requires original primitives"
        )
    dropped = int(abs(face[0, 3:6]).argmax())
    axes = tuple(i for i in range(3) if i != dropped)
    a, b, c = [tuple(p[i] for i in axes) for p in points]
    orientation = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    if not orientation:
        raise ValueError("Degenerate chart projection retains original")
    return (
        int(material),
        tuple(map(int, normals[0])),
        int(code[0]),
        orientation > 0,
        axes,
    )


def components(corners, materials):
    points, _ = integer_positions(corners[..., :3].reshape(-1, 3))
    parents = list(range(len(corners)))
    edges, keys = {}, []

    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i

    for index, (face, material) in enumerate(zip(corners, materials, strict=True)):
        p = points[3 * index : 3 * index + 3]
        key = face_key(face, p, material)
        keys.append(key)
        for a, b in ((p[0], p[1]), (p[1], p[2]), (p[2], p[0])):
            edge = key, min(a, b), max(a, b)
            if edge in edges:
                parents[root(index)] = root(edges[edge])
            else:
                edges[edge] = index
    groups = defaultdict(list)
    for i in range(len(corners)):
        groups[root(i)].append(i)
    return [(indices, keys[indices[0]][-1]) for indices in groups.values()]


def compact_chart(corners):
    if not 1 <= len(corners) <= 32:
        raise ValueError("Exact source chart triangle capacity exceeded")
    rows = np.ascontiguousarray(corners.reshape(-1, 14))
    words, inverse = np.unique(rows.view("u4"), axis=0, return_inverse=True)
    if len(words) > 64:
        raise ValueError("Exact source chart vertex capacity exceeded")
    values = words.view("f4")
    return values[:, :3], values[:, 3:], inverse.reshape(-1, 3).astype(np.uint32)


def compare_source(
    original,
    replacement,
    original_material,
    replacement_material,
    *,
    boundary_envelope=False,
    max_envelope_work=32768,
):
    result = {
        "whole_source_proven": False,
        "proved_groups": 0,
        "original_triangles": len(original),
        "replacement_triangles": len(replacement),
    }
    try:
        validate(original, original_material)
        validate(replacement, replacement_material)
        if np.array_equal(
            primitive_words(original, original_material),
            primitive_words(replacement, replacement_material),
        ):
            return result | {
                "whole_source_proven": True,
                "mode": "same_primitive_word_multiset",
                "position_error_squared": F(0),
                "field_error_absolute": (F(0),) * 11,
                "triangle_pairs": 0,
                "overlay_vertex_evaluations": 0,
            }
        all_corners = np.concatenate((original, replacement))
        all_material = np.concatenate((original_material, replacement_material))
        groups = components(all_corners, all_material)
        maximum, fields = F(0), [F(0)] * 11
        pairs = vertices = 0
        clips = point_tests = 0
        remainder_area = [F(0), F(0)]
        for indices, axes in groups:
            before = [i for i in indices if i < len(original)]
            after = [i - len(original) for i in indices if i >= len(original)]
            if not before or not after:
                raise ValueError("Unmatched original/candidate surface component")
            old_chart, new_chart = (
                compact_chart(original[before]),
                compact_chart(replacement[after]),
            )
            bound = (
                compare_envelope(
                    old_chart, new_chart, axes=axes, max_work=max_envelope_work
                )
                if boundary_envelope
                else compare(old_chart, new_chart, axes=axes)
            )
            maximum = max(maximum, bound["position_error_squared"])
            fields = [
                max(a, b)
                for a, b in zip(fields, bound["field_error_absolute"], strict=True)
            ]
            pairs += len(before) * len(after)
            vertices += bound.get("overlay_vertex_evaluations", 0)
            if boundary_envelope:
                clips += bound["polygon_clips"]
                point_tests += bound["point_triangle_tests"]
                remainder_area[0] += bound["original_outside_candidate"]["chart_area"]
                remainder_area[1] += bound["candidate_outside_original"]["chart_area"]
            result["proved_groups"] += 1
        return result | {
            "whole_source_proven": True,
            "mode": "complete_boundary_envelopes"
            if boundary_envelope
            else "complete_common_charts",
            "position_error_squared": maximum,
            "field_error_absolute": tuple(fields),
            "triangle_pairs": pairs,
            "overlay_vertex_evaluations": vertices,
            "envelope_polygon_clips": clips,
            "point_triangle_tests": point_tests,
            "projected_remainder_area_sum": tuple(remainder_area),
        }
    except ValueError as error:
        return result | {"reason": str(error)}
