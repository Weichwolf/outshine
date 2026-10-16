"""Bounded Embree direct shadow packets; open opaque blades, not canopy transport."""

import time

import numpy as np
from embreex import mesh_construction, rtcore_scene
from grass_shadow_reference import direct_visibility


def query_origins(points, normals, direction, bias):
    if (
        points.shape != normals.shape
        or points.ndim != 2
        or points.shape[1] != 3
        or len(points) > 1_048_576
        or not np.isfinite(points).all()
        or np.max(abs(points), initial=0) > 100
        or not np.isfinite(normals).all()
        or np.any(abs(np.linalg.norm(normals, axis=1) - 1) > 1e-10)
        or direction.shape != (3,)
        or not np.isfinite(direction).all()
        or abs(np.linalg.norm(direction) - 1) > 1e-12
        or direction[2] < 0.25
        or not np.isfinite(bias)
        or not 1e-7 <= bias <= 1e-3
    ):
        raise ValueError(
            "bounded local points, unit normals/upper light and explicit metric bias required"
        )
    facing = np.where(normals @ direction >= 0, 1, -1)
    return points + bias * facing[:, None] * normals


class GrassShadowIndex:
    def __init__(self, vertices, faces):
        began = time.perf_counter()
        if (
            vertices.ndim != 2
            or vertices.shape[1] != 3
            or len(vertices) > 1_000_000
            or not np.isfinite(vertices).all()
            or np.max(abs(vertices), initial=0) > 100
            or faces.ndim != 2
            or faces.shape[1] != 3
            or faces.dtype.kind not in "iu"
            or len(faces) > 500_000
            or faces.min(initial=0) < 0
            or faces.max(initial=-1) >= len(vertices)
        ):
            raise ValueError("bounded finite blade triangle reference required")
        self.vertices, self.faces = vertices, faces
        self.maximum_height = float(vertices[:, 2].max(initial=0))
        self.centre = (
            (vertices.min(0) + vertices.max(0)) / 2 if len(vertices) else np.zeros(3)
        )
        self.scale = (
            max(float(np.linalg.norm(np.ptp(vertices, axis=0))), 1e-3)
            if len(vertices)
            else 1.0
        )
        self.scene = rtcore_scene.EmbreeScene()
        encoded = ((vertices - self.centre) / self.scale).astype(np.float32)
        indices = faces.astype(np.int32)
        self.geometry = (
            mesh_construction.TriangleMesh(self.scene, encoded, indices)
            if len(faces)
            else None
        )
        if len(faces):
            # Force the lazy BVH build into setup, separately from measured queries.
            self.scene.run(
                np.array([[2, 2, 2]], np.float32), np.array([[0, 0, 1]], np.float32)
            )
        self.setup = {
            "build_wall_s": time.perf_counter() - began,
            "vertices": len(vertices),
            "triangles": len(faces),
            "encoded_geometry_copy_bytes": encoded.nbytes + indices.nbytes,
            "known_encoded_arrays": 2,
            "kernel_internal_memory": "unmeasured",
        }

    def validate_hits(self, origins, direction, lengths, ids):
        selected = np.flatnonzero(ids >= 0)
        if not len(selected):
            return np.empty(0, int)
        triangles = self.vertices[self.faces[ids[selected]]]
        a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        n = np.cross(a, b)
        denominator = n @ direction
        distance = np.divide(
            np.einsum("ij,ij->i", triangles[:, 0] - origins[selected], n),
            denominator,
            out=np.full(len(selected), -1.0),
            where=denominator != 0,
        )
        hit = origins[selected] + distance[:, None] * direction
        relative = hit - triangles[:, 0]
        squared = np.einsum("ij,ij->i", n, n)
        u = np.divide(
            np.einsum("ij,ij->i", np.cross(relative, b), n),
            squared,
            out=np.full(len(selected), -1.0),
            where=squared > 0,
        )
        v = np.divide(
            np.einsum("ij,ij->i", np.cross(a, relative), n),
            squared,
            out=np.full(len(selected), -1.0),
            where=squared > 0,
        )
        ambiguous = (
            (distance <= 0)
            | (distance > lengths[selected])
            | (np.minimum(np.minimum(u, v), 1 - u - v) < 1e-5)
            | (abs(denominator) < 1e-7 * np.sqrt(squared))
        )
        return selected[ambiguous]

    def visibility(self, points, normals, direction, bias=2e-5, batch=8192):
        direction = np.asarray(direction, float)
        origins = query_origins(points, normals, direction, bias)
        if type(batch) is not int or not 1 <= batch <= 65536:
            raise ValueError("bounded positive ray packet required")
        lengths = np.maximum(
            (self.maximum_height + 2 * bias - origins[:, 2]) / direction[2], 0
        )
        visible = np.ones(len(points), bool)
        work = {
            "rays": len(points),
            "packets": 0,
            "fallback_rays": 0,
            "fallback_triangle_tests": 0,
            "ray_input_copy_bytes": 0,
        }
        if not len(self.faces):
            return visible, work
        for start in range(0, len(points), batch):
            stop = min(start + batch, len(points))
            origin = origins[start:stop]
            encoded = ((origin - self.centre) / self.scale).astype(np.float32)
            dirs = np.broadcast_to(direction, origin.shape).astype(np.float32)
            limits = (lengths[start:stop] / self.scale).astype(np.float32)
            ids = self.scene.run(encoded, dirs, dists=limits)
            visible[start:stop] = ids < 0
            ambiguous = self.validate_hits(origin, direction, lengths[start:stop], ids)
            if len(ambiguous):
                exact, counters = direct_visibility(
                    self.vertices,
                    self.faces,
                    origin[ambiguous],
                    direction,
                    lengths[start:stop][ambiguous],
                )
                visible[start + ambiguous] = exact
                work["fallback_triangle_tests"] += counters["reference_triangle_tests"]
            work["fallback_rays"] += len(ambiguous)
            work["packets"] += 1
            work["ray_input_copy_bytes"] += encoded.nbytes + dirs.nbytes + limits.nbytes
        return visible, work
