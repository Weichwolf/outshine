"""Exact rational audit of coverage by bisection trees and convex witnesses."""

from fractions import Fraction

import numpy as np
from convex_surface_bound import DENOMINATOR


def check_bound(source, target, result):
    if not result.complete or result.upper_m is None:
        raise ValueError("incomplete proof is not a distance bound")
    source, target = np.asarray(source), np.asarray(target)
    splits = {(int(i), int(path)): int(edge) for i, path, edge in result.splits}
    leaves = {
        (int(i), int(path)): (int(j), weights, upper)
        for i, path, j, weights, upper in result.leaves
    }
    if len(splits) != len(result.splits) or len(leaves) != len(result.leaves):
        raise AssertionError("duplicate proof node")
    if (
        result.exact_targets.shape != (len(source),)
        or not np.isfinite(result.upper_m)
        or result.upper_m < 0
    ):
        raise AssertionError("invalid complete result")
    used_splits, used_leaves = set(), set()
    tested = 0
    largest = Fraction(0)
    unit = tuple(tuple(Fraction(int(i == j)) for j in range(3)) for i in range(3))
    for index, original in enumerate(source):
        exact = int(result.exact_targets[index])
        if exact >= 0:
            if exact >= len(target) or sorted(map(tuple, original)) != sorted(
                map(tuple, target[exact])
            ):
                raise AssertionError("false exact triangle")
            continue
        queue = [(1, unit)]
        while queue:
            path, bary = queue.pop()
            key = (index, path)
            if key in splits:
                if key in leaves:
                    raise AssertionError("ambiguous node")
                edge = splits[key]
                if edge not in (0, 1, 2) or path.bit_length() > 25:
                    raise AssertionError("invalid bisection")
                a, b = ((0, 1), (1, 2), (2, 0))[edge]
                mid = tuple((x + y) / 2 for x, y in zip(bary[a], bary[b], strict=True))
                first, second = list(bary), list(bary)
                first[a] = mid
                second[b] = mid
                queue.extend(((2 * path, tuple(first)), (2 * path + 1, tuple(second))))
                used_splits.add(key)
                continue
            if key not in leaves:
                raise AssertionError("uncovered original triangle patch")
            which, weights, upper = leaves[key]
            weights = np.asarray(weights)
            if (
                not 0 <= which < len(target)
                or weights.shape != (3, 3)
                or weights.dtype.kind not in "iu"
                or (weights < 0).any()
                or (weights > DENOMINATOR).any()
                or not (weights.sum(axis=1) == DENOMINATOR).all()
                or not np.isfinite(upper)
                or not 0 <= upper <= result.upper_m
            ):
                raise AssertionError("non-convex or invalid target witness")
            local_bound = Fraction(float(upper)) ** 2
            for row in range(3):
                distance = Fraction(0)
                for axis in range(3):
                    p = sum(
                        bary[row][v] * Fraction(float(original[v, axis]))
                        for v in range(3)
                    )
                    q = sum(
                        Fraction(int(weights[row, v]), DENOMINATOR)
                        * Fraction(float(target[which, v, axis]))
                        for v in range(3)
                    )
                    distance += (p - q) ** 2
                if distance > local_bound:
                    raise AssertionError("false continuous patch bound")
                largest = max(largest, distance)
                tested += 1
            used_leaves.add(key)
    if used_splits != set(splits) or used_leaves != set(leaves):
        raise AssertionError("orphan proof node")
    return {
        "exact_triangles": int((result.exact_targets >= 0).sum()),
        "rational_corner_witnesses": tested,
        "covering_leaf_patches": len(used_leaves),
        "exact_max_squared_m": (largest.numerator, largest.denominator),
    }
