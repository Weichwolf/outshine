"""Remove duplicate terrain facets only after an exact non-circular cover proof."""

from fractions import Fraction
from itertools import combinations, pairwise

import numpy as np
import shapely


def side(a, b, point):
    return (b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (point[0] - a[0])


def area(polygon):
    return sum(side(polygon[0], a, b) for a, b in pairwise(polygon[1:]))


def cut(polygon, a, b, sign, work):
    work["halfplane_clips"] += 1
    if work["halfplane_clips"] > work["clip_budget"]:
        raise ValueError("exact terrain overlap clip budget exhausted")
    result = []
    if not polygon:
        return result
    previous = polygon[-1]
    before = sign * side(a, b, previous)
    for current in polygon:
        after = sign * side(a, b, current)
        if (before >= 0) != (after >= 0):
            station = before / (before - after)
            result.append(
                tuple(u + station * (v - u) for u, v in zip(previous, current))
            )
        if after >= 0:
            result.append(current)
        previous, before = current, after
    return result


def subtract(polygon, triangle, work):
    outside = []
    inside = polygon
    for a, b in zip(triangle, (*triangle[1:], triangle[0])):
        part = cut(inside, a, b, -1, work)
        if area(part):
            outside.append(part)
        inside = cut(inside, a, b, 1, work)
        if not inside:
            break
    return outside


def covered(triangle, neighbours, work):
    if area(triangle) == 0:
        return False
    parts = [triangle]
    for other in neighbours:
        work["triangle_tests"] += 1
        value = area(other)
        if value == 0:
            continue
        other = other if value > 0 else other[::-1]
        parts = [q for p in parts for q in subtract(p, other, work)]
        work["maximum_parts"] = max(work["maximum_parts"], len(parts))
        if len(parts) > 256:
            raise ValueError("exact terrain overlap fragment budget exhausted")
        if not parts:
            return True
    return False


def convex_pair_cover(target, faces, candidates, triangle, work):
    target = set(map(int, target))
    members = {int(i): set(map(int, faces[i])) for i in candidates}
    for left, right in combinations(candidates, 2):
        work["pair_searches"] += 1
        a_ids, b_ids = members[int(left)], members[int(right)]
        shared, union = a_ids & b_ids, a_ids | b_ids
        if len(shared) != 2 or len(union) != 4 or not target <= union:
            continue
        work["convex_pair_tests"] += 1
        points = dict(zip(faces[left], triangle(left)))
        points.update(zip(faces[right], triangle(right)))
        a, b = [points[i] for i in sorted(shared)]
        c = points[next(iter(a_ids - shared))]
        d = points[next(iter(b_ids - shared))]
        if side(a, b, c) * side(a, b, d) >= 0:
            continue
        polygon = [c, a, d, b]
        turns = [
            side(polygon[i], polygon[(i + 1) % 4], polygon[(i + 2) % 4])
            for i in range(4)
        ]
        if all(v >= 0 for v in turns) or all(v <= 0 for v in turns):
            work["convex_pair_proofs"] += 1
            return True
    return False


def remove_covered_duplicates(
    xy, faces, *, max_groups=64, max_neighbours=64, max_clips=16384
):
    for value, maximum in ((max_groups, 64), (max_neighbours, 64), (max_clips, 16384)):
        if type(value) is not int or not 0 <= value <= maximum:
            raise ValueError("bounded exact terrain overlap proof capacities required")
    xy, faces = np.asarray(xy), np.asarray(faces)
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not 1 <= len(xy) <= 16384
        or xy.dtype.kind != "f"
        or xy.dtype.itemsize > 8
        or not np.isfinite(xy).all()
        or abs(xy).max() > 1e9
        or faces.ndim != 2
        or faces.shape[1] != 3
        or len(faces) > 32768
        or faces.dtype.kind not in "iu"
        or (len(faces) and (faces.min() < 0 or faces.max() >= len(xy)))
    ):
        raise ValueError("bounded finite terrain XY and triangle indices required")
    _, first, inverse, counts = np.unique(
        np.sort(faces, axis=1),
        axis=0,
        return_index=True,
        return_inverse=True,
        return_counts=True,
    )
    groups = np.flatnonzero(counts > 1)
    if not len(groups):
        return faces, {}
    if len(groups) > max_groups:
        raise ValueError("exact terrain overlap group budget exhausted")
    duplicate = counts[inverse] > 1
    polygons = shapely.polygons(xy[faces])
    tree = shapely.STRtree(polygons)
    keep, points = np.ones(len(faces), bool), {}
    work = {
        "duplicate_groups": len(groups),
        "removed_facets": 0,
        "triangle_tests": 0,
        "halfplane_clips": 0,
        "clip_budget": max_clips,
        "maximum_parts": 1,
        "pair_searches": 0,
        "convex_pair_tests": 0,
        "convex_pair_proofs": 0,
    }

    def triangle(index):
        result = []
        for vertex in faces[index]:
            key = int(vertex)
            if key not in points:
                points[key] = tuple(Fraction(float(v)) for v in xy[key])
            result.append(points[key])
        return result

    for group in groups:
        index = first[group]
        candidates = np.sort(tree.query(polygons[index]))
        candidates = candidates[~duplicate[candidates]]
        if len(candidates) > max_neighbours:
            raise ValueError("exact terrain overlap neighbour budget exhausted")
        original = triangle(index)
        if area(original) == 0:
            continue
        if convex_pair_cover(
            faces[index], faces, candidates, triangle, work
        ) or covered(original, (triangle(j) for j in candidates), work):
            keep[inverse == group] = False
            work["removed_facets"] += int(counts[group])
    if not work["removed_facets"]:
        return faces, work
    result = faces[keep]
    work.update(output_index_bytes=result.nbytes, rational_vertices=len(points))
    return result, work
