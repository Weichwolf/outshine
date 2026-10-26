"""Level-set tread bands on an already bounded monotone stair grade envelope."""

import math

import numpy as np
from transport_vertex_pool import welded_vertices


def clip(points, level, above):
    result = []
    for a, b in zip(points, np.roll(points, -1, axis=0), strict=True):
        inside_a = (a[2] >= level) if above else (a[2] <= level)
        inside_b = (b[2] >= level) if above else (b[2] <= level)
        if inside_a:
            result.append(a)
        if inside_a != inside_b:
            point = a + (b - a) * ((level - a[2]) / (b[2] - a[2]))
            point[2] = level
            result.append(point)
    return np.array(result).reshape(-1, 3)


def unique_caps(plan):
    records = {}
    for owner, faces in enumerate(plan["parts"]):
        for face in faces:
            key = tuple(face)
            records[key] = records.get(key, 0) | (1 << owner)
    return np.array(list(records)), np.array(list(records.values()))


def stair_recipes(plan, heights):
    xy = plan["xy"]
    result, levels = [], []
    for owner, low_node, high_node in plan.get("stairs", [(0, 0, 1)]):
        indices = np.unique(plan["parts"][owner])
        a, b = [
            indices[np.argmin(np.linalg.norm(xy[indices] - plan["nodes"][n], axis=1))]
            for n in (low_node, high_node)
        ]
        low, high = float(heights[a]), float(heights[b])
        count = math.ceil((high - low) / 0.179)
        if not 1 <= count <= 64:
            raise ValueError("bounded positive stair rise required")
        result.append(
            {
                "owner": owner,
                "low": low,
                "high": high,
                "risers": count,
                "rise_m": (high - low) / count,
                "level_offset": len(levels),
            }
        )
        levels.extend(np.linspace(low, high, count + 1))
    return result, np.array(levels)


def slices(points, owner, recipes, levels):
    for recipe in recipes:
        for level in (recipe["low"], recipe["high"]):
            points[abs(points[:, 2] - level) < 1e-8, 2] = level
    chosen = [r for r in recipes if owner & (1 << r["owner"])]
    if not chosen:
        return [(points, -1)]
    if len(chosen) != 1:
        raise ValueError("shared cap between multiple stair recipes is unplanned")
    recipe = chosen[0]
    low, high = recipe["low"], recipe["high"]
    offset, count, rise = recipe["level_offset"], recipe["risers"], recipe["rise_m"]
    if np.ptp(points[:, 2]) < 1e-10:
        k = int(np.clip(np.ceil((points[:, 2].mean() - low) / rise - 1e-8), 0, count))
        return [(points, offset + k)]
    if points[:, 2].min() < low - 1e-7 or points[:, 2].max() > high + 1e-7:
        raise ValueError("stair envelope leaves flat landing height interval")
    if owner != 1 << recipe["owner"]:
        raise ValueError("a common landing is not flat")
    result = []
    for k in range(1, count + 1):
        polygon = clip(
            clip(points, levels[offset + k - 1], True), levels[offset + k], False
        )
        if len(polygon) >= 3:
            result.append((polygon, offset + k))
    return result


def partition(plan, heights):
    faces, source_masks = unique_caps(plan)
    recipes, levels = stair_recipes(plan, heights)
    xy = plan["xy"]
    raw, masks, chosen = [], [], []
    for face, owner in zip(faces, source_masks, strict=True):
        points = np.c_[xy[face], heights[face]]
        for polygon, k in slices(points, owner, recipes, levels):
            for j in range(1, len(polygon) - 1):
                tri = polygon[[0, j, j + 1]]
                if np.cross(tri[1] - tri[0], tri[2] - tri[0])[2] > 1e-12:
                    raw.append(tri)
                    masks.append(owner)
                    chosen.append(k)
    if len(raw) > 8192:
        raise ValueError("stair tread face budget")
    points = np.array(raw).reshape(-1, 3)
    points, inverse = np.unique(points, axis=0, return_inverse=True)
    points, labels, drift = welded_vertices(points, 1e-8)
    faces = labels[inverse].reshape(-1, 3)
    note = {
        "risers": sum(r["risers"] for r in recipes),
        "rise_m": max(r["rise_m"] for r in recipes),
        "level_weld_m": drift,
    }
    if len(recipes) > 1:
        note["stairs"] = recipes
    return (
        points,
        faces,
        np.array(masks),
        np.array(chosen),
        levels,
        note,
    )
