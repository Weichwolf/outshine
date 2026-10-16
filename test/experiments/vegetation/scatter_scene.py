"""Finite local nearest-hit packets with Float64 reconstruction and edge fallback."""

import numpy as np
from grass_shadow import GrassShadowIndex
from scatter_reference import Float64Nearest


class ScatterScene:
    def __init__(self, vertices, faces):
        self.index = GrassShadowIndex(vertices, faces)
        self.reference = None
        self.shadow_work = {}
        triangles = vertices[faces]
        if np.any(
            np.linalg.norm(
                np.cross(
                    triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
                ),
                axis=1,
            )
            <= 0
        ):
            raise ValueError("nondegenerate finite scattering triangles required")
        self.work = {
            "rays": 0,
            "packets": 0,
            "hits": 0,
            "fallback_rays": 0,
            "fallback_triangle_tests": 0,
            "input_copy_bytes": 0,
        }

    def nearest(self, origins, directions, length):
        origins, directions = np.asarray(origins, float), np.asarray(directions, float)
        if (
            origins.shape != directions.shape
            or origins.ndim != 2
            or origins.shape[1] != 3
            or len(origins) > 65536
            or not np.isfinite(origins).all()
            or np.max(abs(origins), initial=0) > 100
            or not np.isfinite(directions).all()
            or np.any(abs(np.linalg.norm(directions, axis=1) - 1) > 1e-10)
            or not np.isfinite(length)
            or not 0 < length <= 100
        ):
            raise ValueError(
                "bounded finite local unit-ray packet and metric length required"
            )
        index = self.index
        encoded = ((origins - index.centre) / index.scale).astype(np.float32)
        dirs = directions.astype(np.float32)
        limits = np.full(len(origins), length / index.scale, np.float32)
        ids = (
            index.scene.run(encoded, dirs, dists=limits)
            if len(index.faces)
            else np.full(len(origins), -1)
        )
        points, normals, distance, bad = self.reconstruct(
            origins, directions, ids, length
        )
        if len(bad):
            if self.reference is None:
                self.reference = Float64Nearest(index.vertices, index.faces)
            fixed, _, tested = self.reference.query(
                origins[bad], directions[bad], length
            )
            ids[bad] = fixed
            points, normals, distance, _ = self.reconstruct(
                origins, directions, ids, length
            )
            self.work["fallback_rays"] += len(bad)
            self.work["fallback_triangle_tests"] += tested
        self.work["rays"] += len(origins)
        self.work["packets"] += 1
        self.work["hits"] += int(np.count_nonzero(ids >= 0))
        self.work["input_copy_bytes"] += encoded.nbytes + dirs.nbytes + limits.nbytes
        return ids, points, normals, distance

    def reconstruct(self, origins, directions, ids, length):
        selected = np.flatnonzero(ids >= 0)
        points, normals = np.zeros_like(origins), np.zeros_like(origins)
        distance = np.zeros(len(origins))
        if not len(selected):
            return points, normals, distance, np.empty(0, int)
        triangles = self.index.vertices[self.index.faces[ids[selected]]]
        a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        n = np.cross(a, b)
        squared = np.einsum("ij,ij->i", n, n)
        denominator = np.einsum("ij,ij->i", n, directions[selected])
        t = np.divide(
            np.einsum("ij,ij->i", triangles[:, 0] - origins[selected], n),
            denominator,
            out=np.full(len(selected), -1.0),
            where=denominator != 0,
        )
        points[selected] = origins[selected] + t[:, None] * directions[selected]
        normals[selected] = n / np.sqrt(squared)[:, None]
        distance[selected] = t
        relative = points[selected] - triangles[:, 0]
        u = np.einsum("ij,ij->i", np.cross(relative, b), n) / squared
        v = np.einsum("ij,ij->i", np.cross(a, relative), n) / squared
        bad = selected[
            (t <= 0)
            | (t > length)
            | (np.minimum(np.minimum(u, v), 1 - u - v) < 1e-5)
            | (abs(denominator) < 1e-7 * np.sqrt(squared))
        ]
        return points, normals, distance, bad
