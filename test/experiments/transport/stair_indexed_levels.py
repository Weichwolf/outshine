"""Level clips share original indexed edges across faces and adjacent bands."""

import numpy as np
from stair_junction_treads import stair_recipes, unique_caps


class LevelCuts:
    def __init__(self, points, levels):
        self.points = points.tolist()
        self.levels = levels
        self.roots = [{i} for i in range(len(points))]
        self.cache = {}
        self.edge_queries = 0

    def crossing(self, first, second, level):
        self.edge_queries += 1
        for vertex in (first, second):
            if self.points[vertex][2] == self.levels[level]:
                return vertex
        origin = self.roots[first] | self.roots[second]
        if len(origin) != 2:
            raise ValueError("parallel level clips must retain one original edge")
        a, b = sorted(origin)
        key = (a, b, level)
        if key not in self.cache:
            if len(self.points) >= 65536:
                raise ValueError("indexed tread vertex budget")
            pa, pb = np.array(self.points[a]), np.array(self.points[b])
            if pa[2] == pb[2]:
                raise ValueError("constant original edge cannot cross a level")
            ratio = (self.levels[level] - pa[2]) / (pb[2] - pa[2])
            if not 0 < ratio < 1:
                raise ValueError("level crossing leaves its original edge")
            point = pa + ratio * (pb - pa)
            point[2] = self.levels[level]
            self.cache[key] = len(self.points)
            self.points.append(point.tolist())
            self.roots.append(origin)
        return self.cache[key]

    def clip(self, polygon, level, above):
        result = []
        for a, b in zip(polygon, polygon[1:] + polygon[:1], strict=True):
            za, zb = self.points[a][2], self.points[b][2]
            cut = self.levels[level]
            inside_a, inside_b = (
                (za >= cut, zb >= cut) if above else (za <= cut, zb <= cut)
            )
            if inside_a:
                result.append(a)
            if inside_a != inside_b:
                result.append(self.crossing(a, b, level))
        return list(dict.fromkeys(result))


def _emit(cuts, polygon, owner, step, output, masks, steps):
    zero_faces = 0
    for j in range(1, len(polygon) - 1):
        face = [polygon[0], polygon[j], polygon[j + 1]]
        xy = np.array([cuts.points[v][:2] for v in face])
        a, b = xy[1] - xy[0], xy[2] - xy[0]
        signed = a[0] * b[1] - a[1] * b[0]
        if signed == 0:
            zero_faces += 1
            continue
        if signed < 0:
            raise ValueError("indexed tread clipping reversed a projected face")
        if len(output) >= 8192:
            raise ValueError("indexed tread face budget")
        output.append(face)
        masks.append(owner)
        steps.append(step)
    return zero_faces


def partition(plan, heights):
    faces, membership = unique_caps(plan)
    if len(faces) > 8192 or len(plan["xy"]) > 4096:
        raise ValueError("bounded prepared stair cap required")
    if heights.shape != (len(plan["xy"]),) or not np.isfinite(heights).all():
        raise ValueError("one finite height per prepared stair vertex required")
    recipes, levels = stair_recipes(plan, heights)
    cuts = LevelCuts(np.c_[plan["xy"], heights], levels)
    output, masks, steps, zero = [], [], [], 0
    for face, owner in zip(faces, membership, strict=True):
        chosen = [r for r in recipes if owner & (1 << r["owner"])]
        if not chosen:
            zero += _emit(cuts, face.tolist(), owner, -1, output, masks, steps)
            continue
        if len(chosen) != 1:
            raise ValueError("shared cap between multiple stair recipes is unplanned")
        recipe = chosen[0]
        offset, count = recipe["level_offset"], recipe["risers"]
        z = heights[face]
        if z.min() < recipe["low"] - 1e-7 or z.max() > recipe["high"] + 1e-7:
            raise ValueError("stair envelope leaves its prepared landing interval")
        if np.ptp(z) < 1e-10:
            k = int(
                np.clip(
                    np.ceil((z.mean() - recipe["low"]) / recipe["rise_m"] - 1e-8),
                    0,
                    count,
                )
            )
            zero += _emit(cuts, face.tolist(), owner, offset + k, output, masks, steps)
            continue
        if owner != 1 << recipe["owner"]:
            raise ValueError("a common landing is not flat")
        for k in range(1, count + 1):
            polygon = cuts.clip(
                cuts.clip(face.tolist(), offset + k - 1, True), offset + k, False
            )
            zero += _emit(cuts, polygon, owner, offset + k, output, masks, steps)
    return (
        np.array(cuts.points),
        np.array(output, dtype=int),
        np.array(masks, dtype=int),
        np.array(steps, dtype=int),
        levels,
        {
            "risers": sum(r["risers"] for r in recipes),
            "rise_m": max(r["rise_m"] for r in recipes),
            "stairs": recipes,
            "level_weld_m": 0.0,
            "edge_intersections": len(cuts.cache),
            "edge_queries": cuts.edge_queries,
            "exact_zero_projected_faces_removed": zero,
        },
    )
