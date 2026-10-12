"""Batched oriented surface queries with local float rays and double hit refinement."""

import numpy as np
from embreex import mesh_construction, rtcore_scene


class SurfaceRays:
    def __init__(self, reference, fallback):
        vertices = reference["vertices"]
        self.centre = (vertices.min(axis=0) + vertices.max(axis=0)) * 0.5
        self.scale = float(np.linalg.norm(np.ptp(vertices, axis=0)))
        if not np.isfinite(self.scale) or self.scale <= 0:
            raise ValueError("finite nonempty ray reference required")
        self.scene = rtcore_scene.EmbreeScene()
        mesh_construction.TriangleMesh(
            self.scene,
            ((vertices - self.centre) / self.scale).astype(np.float32),
            reference["faces"].astype(np.int32),
        )
        self.triangles = vertices[reference["faces"]]
        self.normals = np.cross(
            self.triangles[:, 1] - self.triangles[:, 0],
            self.triangles[:, 2] - self.triangles[:, 0],
        )
        self.fallback = fallback
        self.bounds = (vertices.min(axis=0), vertices.max(axis=0))
        self.fallback_queries = 0

    def nearest(self, points, normals, length):
        if not np.isfinite(length) or length <= 0 or len(points) > 1_000_000:
            raise ValueError("positive bounded ray query required")
        best = np.full(len(points), np.inf)
        chosen = np.full(len(points), -1, dtype=np.int64)
        for sign in (-1, 1):
            self.half_rays(points, normals, length, sign, best, chosen)
        self.refine_ambiguous(points, normals, length, best, chosen)
        selected = np.flatnonzero(chosen >= 0)
        positions = points[selected] - best[selected, None] * normals[selected]
        return selected, positions, chosen[selected]

    def refine_ambiguous(self, points, normals, length, best, chosen):
        selected = np.flatnonzero(chosen >= 0)
        triangle = self.triangles[chosen[selected]]
        hit = points[selected] - best[selected, None] * normals[selected]
        a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
        n = self.normals[chosen[selected]]
        squared = np.sum(n * n, axis=1)
        relative = hit - triangle[:, 0]
        u = np.sum(np.cross(relative, b) * n, axis=1) / squared
        v = np.sum(np.cross(a, relative) * n, axis=1) / squared
        edge = np.minimum(np.minimum(u, v), 1 - u - v) < 1e-5
        grazing = abs(np.sum(n * normals[selected], axis=1)) < 1e-3 * np.sqrt(squared)
        ambiguous = chosen < 0
        ambiguous[selected] = edge | grazing
        beginning, ending = points + length * normals, points - length * normals
        low, high = self.bounds
        margin = (
            64
            * np.finfo(float).eps
            * max(np.max(abs(beginning)), np.max(abs(ending)), 1)
        )
        direction = ending - beginning
        parallel = direction == 0
        outside = parallel & ((beginning < low - margin) | (beginning > high + margin))
        safe = np.where(parallel, 1, direction)
        first, last = (
            (low - margin - beginning) / safe,
            (high + margin - beginning) / safe,
        )
        enter = np.where(parallel, -np.inf, np.minimum(first, last)).max(axis=1)
        leave = np.where(parallel, np.inf, np.maximum(first, last)).min(axis=1)
        possible = ~outside.any(axis=1) & (np.maximum(enter, 0) <= np.minimum(leave, 1))
        for index in np.flatnonzero(ambiguous & possible):
            self.fallback_queries += 1
            hits = [
                h
                for h in self.fallback.ray_cast(beginning[index], ending[index])
                if np.dot(h.normal, normals[index]) > 0
            ]
            if hits:
                hit = min(hits, key=lambda h: abs(h.distance - 0.5))
                best[index] = np.dot(points[index] - hit.position, normals[index])
                chosen[index] = hit.face_id
            else:
                best[index], chosen[index] = np.inf, -1

    def half_rays(self, points, normals, length, sign, best, chosen):
        pending = np.arange(len(points))
        offset = np.zeros(len(points))
        direction = normals * sign
        for _ in range(16):
            if not len(pending):
                return
            start = points[pending] + offset[pending, None] * direction[pending]
            ids = self.scene.run(
                ((start - self.centre) / self.scale).astype(np.float32),
                direction[pending].astype(np.float32),
                dists=((length - offset[pending]) / self.scale).astype(np.float32),
            )
            hit = ids >= 0
            pending, ids = pending[hit], ids[hit]
            if not len(pending):
                return
            n = self.normals[ids]
            denominator = np.sum(n * direction[pending], axis=1)
            if np.any(denominator == 0):
                raise ValueError("ray candidate parallel to double reference face")
            distance = (
                np.sum((self.triangles[ids, 0] - points[pending]) * n, axis=1)
                / denominator
            )
            tolerance = self.scale * 1e-10
            valid = (
                (np.sum(n * normals[pending], axis=1) > 0)
                & (distance >= -tolerance)
                & (distance <= length + tolerance)
            )
            selected, d = pending[valid], distance[valid]
            better = abs(d) < abs(best[selected])
            selected = selected[better]
            best[selected] = -sign * d[better]
            chosen[selected] = ids[valid][better]
            pending, distance = pending[~valid], distance[~valid]
            offset[pending] = np.maximum(offset[pending], distance) + self.scale * 2e-6
            pending = pending[offset[pending] < length]
        if len(pending):
            raise ValueError("oriented ray traversal exceeds sixteen intersections")
