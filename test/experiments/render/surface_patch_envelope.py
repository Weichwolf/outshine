"""Exact two-sided surface upper bound when chart boundary domains differ.

Common cells use the same chart point. Each remaining convex cell maps its
vertices into one target triangle; convex combinations cover its entire interior.
This is a constructive upper bound, not the tight Hausdorff distance or a native
coverage/depth/derivative/PBR criterion. Original surfaces remain authoritative.
"""

from fractions import Fraction as F

from surface_patch_overlay import chart, clip, cross, twice_area, validate_axes, value


class Budget:
    def __init__(self, maximum=32768):
        if type(maximum) is not int or not 1 <= maximum <= 262144:
            raise ValueError("Bounded surface envelope work limit required")
        self.maximum = maximum
        self.clips = self.point_triangle_tests = 0

    def take(self, kind):
        setattr(self, kind, getattr(self, kind) + 1)
        if self.clips + self.point_triangle_tests > self.maximum:
            raise ValueError("Surface envelope work capacity exceeded; retain original")


def half(polygon, a, b, sign):
    result = []
    if not polygon:
        return result
    p, dp = polygon[-1], sign * cross(a, b, polygon[-1])
    for q in polygon:
        dq = sign * cross(a, b, q)
        if (dp < 0) != (dq < 0):
            t = dp / (dp - dq)
            result.append(tuple(x + t * (y - x) for x, y in zip(p, q, strict=True)))
        if dq >= 0:
            result.append(q)
        p, dp = q, dq
    return list(dict.fromkeys(result))


def subtract(polygon, triangle, budget):
    remaining, outside = polygon, []
    for a, b in zip(triangle, (*triangle[1:], triangle[0]), strict=True):
        budget.take("clips")
        piece = half(remaining, a, b, -1)
        if twice_area(piece):
            outside.append(piece)
        remaining = half(remaining, a, b, 1)
        if not twice_area(remaining):
            break
    return outside


def dot(a, b):
    return sum(x * y for x, y in zip(a, b, strict=True))


def closest_weights(point, rows, budget):
    budget.take("point_triangle_tests")
    a, b, c = [row[:3] for row in rows]
    u, v, w = (tuple(x - y for x, y in zip(p, a, strict=True)) for p in (b, c, point))
    aa, ab, bb, au, bv = dot(u, u), dot(u, v), dot(v, v), dot(u, w), dot(v, w)
    determinant = aa * bb - ab * ab
    if determinant <= 0:
        raise ValueError("Nondegenerate target surface triangle required")
    x, y = (au * bb - bv * ab) / determinant, (bv * aa - au * ab) / determinant
    choices = []
    if x >= 0 and y >= 0 and x + y <= 1:
        choices.append((1 - x - y, x, y))
    for i, j in ((0, 1), (1, 2), (2, 0)):
        direction = tuple(rows[j][k] - rows[i][k] for k in range(3))
        offset = tuple(point[k] - rows[i][k] for k in range(3))
        t = min(F(1), max(F(0), dot(direction, offset) / dot(direction, direction)))
        weights = [F(0)] * 3
        weights[i], weights[j] = 1 - t, t
        choices.append(tuple(weights))
    return min(
        (
            (
                sum(
                    (sum(weights[i] * rows[i][k] for i in range(3)) - point[k]) ** 2
                    for k in range(3)
                ),
                weights,
            )
            for weights in choices
        ),
        key=lambda row: row[0],
    )


def mapped_cell(points, target_values, budget):
    best = None
    for rows in target_values:
        candidates = [closest_weights(point[:3], rows, budget) for point in points]
        maximum = max(row[0] for row in candidates)
        if best is None or maximum < best[0]:
            best = maximum, rows, [row[1] for row in candidates]
    maximum, rows, weights = best
    fields = [F(0)] * (len(points[0]) - 3)
    for point, barycentric in zip(points, weights, strict=True):
        for k in range(len(fields)):
            target = sum(barycentric[i] * rows[i][k + 3] for i in range(3))
            fields[k] = max(fields[k], abs(point[k + 3] - target))
    return maximum, fields


def remainder_bounds(triangles, values, targets, target_values, budget):
    maximum, fields = F(0), [F(0)] * (len(values[0][0]) - 3)
    cells = 0
    area = F(0)
    for triangle, data in zip(triangles, values, strict=True):
        remaining = [list(triangle)]
        for target in targets:
            remaining = [
                piece
                for polygon in remaining
                for piece in subtract(polygon, target, budget)
            ]
            if len(remaining) > 256:
                raise ValueError(
                    "Surface envelope fragment capacity exceeded; retain original"
                )
            if not remaining:
                break
        for polygon in remaining:
            squared, errors = mapped_cell(
                [value(triangle, data, p) for p in polygon], target_values, budget
            )
            maximum = max(maximum, squared)
            fields = [max(a, b) for a, b in zip(fields, errors, strict=True)]
            cells += 1
            area += twice_area(polygon) / 2
    return maximum, fields, cells, area


def compare_envelope(original, replacement, *, axes, max_work=32768):
    validate_axes(axes)
    old, old_values, _areas, old_sign = chart(*original, axes)
    new, new_values, _areas, new_sign = chart(*replacement, axes)
    if old_sign != new_sign or len(old_values[0][0]) != len(new_values[0][0]):
        raise ValueError("Matching original winding and field inventory required")
    budget = Budget(max_work)
    maximum, fields = F(0), [F(0)] * (len(old_values[0][0]) - 3)
    common_cells = vertices = 0
    for a, values_a in zip(old, old_values, strict=True):
        for b, values_b in zip(new, new_values, strict=True):
            budget.take("clips")
            polygon = clip(a, b)
            common_cells += bool(twice_area(polygon))
            for point in polygon:
                vertices += 1
                x, y = value(a, values_a, point), value(b, values_b, point)
                difference = tuple(u - v for u, v in zip(x, y, strict=True))
                maximum = max(maximum, sum(d * d for d in difference[:3]))
                fields = [
                    max(f, abs(d)) for f, d in zip(fields, difference[3:], strict=True)
                ]
    missing = []
    for a, av, b, bv in (
        (old, old_values, new, new_values),
        (new, new_values, old, old_values),
    ):
        squared, errors, cells, area = remainder_bounds(a, av, b, bv, budget)
        maximum = max(maximum, squared)
        fields = [max(x, y) for x, y in zip(fields, errors, strict=True)]
        missing.append({"cells": cells, "chart_area": area})
    return {
        "position_error_squared": maximum,
        "field_error_absolute": tuple(fields),
        "positive_area_common_cells": common_cells,
        "overlay_vertex_evaluations": vertices,
        "original_outside_candidate": missing[0],
        "candidate_outside_original": missing[1],
        "polygon_clips": budget.clips,
        "point_triangle_tests": budget.point_triangle_tests,
        "work_scope": "Envelope clips/point-triangle probes; separate input chart validation is bounded by32triangles per side.",
    }
