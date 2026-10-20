"""Opaque two-triangle disks with convex projection over the whole observer box."""

import itertools

import numpy as np
from beam_intervals import cross, dot, exact, sub


class Patches:
    def __init__(self, source, beams):
        _, welded = np.unique(
            source["positions"].view(np.uint32), axis=0, return_inverse=True
        )
        triangles = welded[source["indices"]]
        positions = source["positions"].astype(float) @ beams.basis.T
        _, first = np.unique(welded, return_index=True)
        positions = positions[first]
        groups = np.column_stack(
            (
                source["binding"][:, [0, 1, 2, 4]],
                source["normal"][source["indices"][:, 0]],
            )
        )
        edges = {}
        for face, tri in enumerate(triangles):
            for a, b in zip(tri, np.roll(tri, -1), strict=True):
                edges.setdefault((min(a, b), max(a, b), *groups[face]), []).append(
                    (face, int(a), int(b))
                )
        corners = np.array(list(itertools.product(*zip(*beams.observer, strict=True))))
        eyes = exact(corners[:, None])
        pairs, polygons = [], []
        for members in edges.values():
            if len(members) != 2:
                continue
            (a, u, v), (b, p, q) = members
            if u != q or v != p or beams.numerator[1][[a, b]].max() >= 0:
                continue
            boundary = [
                (int(x), int(y))
                for face in (a, b)
                for x, y in zip(
                    triangles[face], np.roll(triangles[face], -1), strict=True
                )
                if {int(x), int(y)} != {u, v}
            ]
            successor = dict(boundary)
            if len(successor) != 4:
                continue
            ring = [boundary[0][0]]
            for _ in range(3):
                ring.append(successor[ring[-1]])
            if successor[ring[-1]] != ring[0] or len(set(ring)) != 4:
                continue
            polygon = positions[ring]
            valid = True
            for i in range(4):
                start, end = polygon[i], polygon[(i + 1) % 4]
                normal = cross(sub(exact(end), exact(start)), sub(eyes, exact(start)))
                for j in ((i + 2) % 4, (i + 3) % 4):
                    value = dot(normal, sub(exact(polygon[j]), exact(start)))
                    if value[1].max() >= 0:
                        valid = False
            if valid:
                pairs.append([a, b])
                polygons.append(polygon)
        self.pairs = np.array(pairs, np.uint32).reshape(-1, 2)
        if not len(pairs):
            self.edges = (np.empty((8, 0, 4, 3)),) * 2
            self.high = np.empty(0)
            return
        polygons = np.array(polygons)
        a = exact(polygons)
        b = exact(np.roll(polygons, -1, axis=1))
        self.edges = cross(sub(b, a), sub(exact(corners[:, None, None]), a))
        self.high = np.nextafter(
            polygons[:, :, 2].max(1) - beams.observer[0, 2], np.inf
        )

    def classify(self, ids, box):
        low = np.r_[np.nextafter(np.nextafter(box[0], -np.inf), -np.inf), 1.0]
        high = np.r_[np.nextafter(np.nextafter(box[1], np.inf), np.inf), 1.0]
        value = dot(tuple(a[:, ids] for a in self.edges), (low, high))
        return (value[0].min(0) <= 0).all(1), (value[1].max(0) < 0).all(1)
