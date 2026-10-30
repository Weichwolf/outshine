"""Bounded ordered ray/triangle batches; original geometry is borrowed only."""

import numpy as np

MAX_PAIRS = 16384
MAX_TRIANGLES = 131072


def pair_nearest(positions, triangles, faces, rows, origins, directions):
    """Ordered row groups, same front-face tests; reduce distance then ordinal."""
    origin, direction = origins[rows], directions[rows]
    a, b, c = positions[triangles[faces]].astype(float).transpose(1, 0, 2)
    starts = np.r_[0, np.flatnonzero(rows[1:] != rows[:-1]) + 1]
    ends = np.r_[starts[1:], len(rows)]
    with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
        e1, e2 = b - a, c - a
        p = np.cross(direction, e2)
        determinant = np.einsum("ij,ij->i", e1, p)
        front = determinant > 1e-12
        reciprocal = np.divide(
            1.0, determinant, out=np.zeros_like(determinant), where=front
        )
        relative = origin - a
        u = np.einsum("ij,ij->i", relative, p) * reciprocal
        q = np.cross(relative, e1)
        v = np.empty(len(rows))
        for low, high in zip(starts, ends, strict=True):
            v[low:high] = q[low:high] @ directions[rows[low]] * reciprocal[low:high]
        distance = np.einsum("ij,ij->i", e2, q) * reciprocal
    finite = (
        np.isfinite(determinant)
        & np.isfinite(u)
        & np.isfinite(v)
        & np.isfinite(distance)
    )
    valid = finite & front & (u >= 0) & (v >= 0) & (u + v <= 1) & (distance > 0)
    distances = np.where(valid, distance, np.inf)
    lengths = ends - starts
    minimum = np.minimum.reduceat(distances, starts)
    eligible = distances == np.repeat(minimum, lengths)
    owner = np.minimum.reduceat(
        np.where(eligible, faces, np.iinfo(np.uint32).max), starts
    ).astype(np.int64)
    owner[~np.isfinite(minimum)] = -1
    return rows[starts], minimum, owner, ~np.logical_and.reduceat(finite, starts)


class TriangleBatches:
    """Whole rays submitted once in order; fixed packet, bounded oversized ray."""

    def __init__(self, positions, triangles, origins, directions, capacity):
        if (
            type(capacity) is not int
            or not 1 <= capacity <= MAX_PAIRS
            or len(triangles) > MAX_TRIANGLES
        ):
            raise ValueError("Bounded positive pair capacity required")
        self.positions, self.triangles = positions, triangles
        self.origins, self.directions = origins, directions
        self.faces = np.empty(capacity, np.uint32)
        self.rows = np.empty(capacity, np.int64)
        self.distance = np.full(len(origins), np.inf)
        self.owner = np.full(len(origins), -1, np.int64)
        self.unknown = np.zeros(len(origins), bool)
        self.used = self.batches = self.tested = self.peak_pairs = 0
        self.single_ray_calls = self.single_ray_pairs = self.peak_single_ray_pairs = 0
        self.last_row = -1
        self.ray_groups = 0

    def add(self, row, candidates):
        if not self.last_row < row < len(self.origins):
            raise ValueError("Submit each complete ray once in increasing order")
        if len(candidates) > MAX_TRIANGLES:
            raise ValueError("Original single-ray triangle limit exceeded")
        self.last_row = row
        self.ray_groups += 1
        if len(candidates) > len(self.faces):
            self.flush()
            self.consume(candidates, np.full(len(candidates), row, np.int64))
            self.single_ray_calls += 1
            self.single_ray_pairs += len(candidates)
            self.peak_single_ray_pairs = max(
                self.peak_single_ray_pairs, len(candidates)
            )
            return
        if self.used + len(candidates) > len(self.faces):
            self.flush()
        end = self.used + len(candidates)
        self.faces[self.used : end] = candidates
        self.rows[self.used : end] = row
        self.used = end
        if self.used == len(self.faces):
            self.flush()

    def flush(self):
        if not self.used:
            return
        self.consume(self.faces[: self.used], self.rows[: self.used])
        self.batches += 1
        self.peak_pairs = max(self.peak_pairs, self.used)
        self.used = 0

    def consume(self, faces, source_rows):
        rows, distance, owner, unknown = pair_nearest(
            self.positions,
            self.triangles,
            faces,
            source_rows,
            self.origins,
            self.directions,
        )
        take = (distance < self.distance[rows]) | (
            (distance == self.distance[rows])
            & (owner >= 0)
            & ((self.owner[rows] < 0) | (owner < self.owner[rows]))
        )
        self.distance[rows[take]] = distance[take]
        self.owner[rows[take]] = owner[take]
        self.unknown[rows] |= unknown
        self.tested += len(faces)

    def work(self):
        return {
            "intersection_pair_capacity": len(self.faces),
            "intersection_batches": self.batches,
            "pair_kernel_calls": self.batches + self.single_ray_calls,
            "scalar_product_calls": self.ray_groups,
            "max_intersection_batch_pairs": self.peak_pairs,
            "single_ray_calls": self.single_ray_calls,
            "single_ray_triangle_tests": self.single_ray_pairs,
            "max_single_ray_pairs": self.peak_single_ray_pairs,
            "single_ray_pair_limit": MAX_TRIANGLES,
            "max_pair_kernel_rows": max(self.peak_pairs, self.peak_single_ray_pairs),
            "max_single_ray_id_payload_bytes": self.peak_single_ray_pairs * 8,
            "intersection_staging_payload_bytes": self.faces.nbytes + self.rows.nbytes,
            "intersection_result_payload_bytes": self.distance.nbytes
            + self.owner.nbytes
            + self.unknown.nbytes,
            "intersection_numeric_unknown_samples": int(self.unknown.sum()),
            "staged_triangle_index_copy_bytes": (self.tested - self.single_ray_pairs)
            * 4,
            "expanded_ray_gather_bytes": self.tested * 48,
            "ray_row_id_write_bytes": self.tested * 8,
            "known_triangle_index_corner_copy_bytes": self.tested * 120,
        }
