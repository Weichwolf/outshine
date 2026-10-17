"""Prepared explicit XY-height surfaces; local absence never becomes height zero."""

import time
from collections.abc import Mapping
from dataclasses import dataclass, field
from math import isfinite
from types import MappingProxyType

import numpy as np
import shapely

from ..contracts import Mesh


def owned(array, dtype):
    result = np.array(array, dtype=dtype, copy=True)
    result.setflags(write=False)
    return result


@dataclass(frozen=True, slots=True)
class SurfaceSamples:
    """NaN height/normal and face -1 with a reason denote unresolved contacts."""

    points_enu_m: np.ndarray
    normals_enu: np.ndarray
    face_indices: np.ndarray
    reasons: tuple[str, ...]
    report: Mapping[str, object]

    def __post_init__(self):
        object.__setattr__(self, "points_enu_m", owned(self.points_enu_m, float))
        object.__setattr__(self, "normals_enu", owned(self.normals_enu, float))
        object.__setattr__(self, "face_indices", owned(self.face_indices, np.int64))
        object.__setattr__(self, "reasons", tuple(self.reasons))
        object.__setattr__(self, "report", MappingProxyType(dict(self.report)))


@dataclass(frozen=True, slots=True)
class SurfaceSampler:
    """One selected height surface, not a highest-hit ground/deck/roof selector.

    XY tolerance is metric distance to a face; heights extrapolate its affine
    plane only within that distance. Consistent shared-edge/vertex contacts pick
    the lowest face index and its upward facet normal. Other multiple projected
    hits are unresolved, even if their heights match. The caller keeps frame,
    product kind and original source IDs associated with the selected mesh/faces.
    """

    mesh: Mesh
    xy_tolerance_m: float = 1e-8
    height_tolerance_m: float = 1e-7
    max_faces: int = 65536
    _polygons: object = field(init=False, repr=False)
    _tree: object = field(init=False, repr=False)
    _gradient: np.ndarray = field(init=False, repr=False)
    _normal: np.ndarray = field(init=False, repr=False)

    def __post_init__(self):
        if (
            not isinstance(self.mesh, Mesh)
            or type(self.max_faces) is not int
            or self.max_faces <= 0
        ):
            raise ValueError("owned Mesh and positive integer face budget required")
        if not all(
            isfinite(t) and t >= 0
            for t in (self.xy_tolerance_m, self.height_tolerance_m)
        ):
            raise ValueError("finite nonnegative metric XY/height tolerances required")
        if len(self.mesh.triangles) > self.max_faces:
            raise ValueError("surface exceeds preparation face budget")
        p = self.mesh.vertices[self.mesh.triangles]
        u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        with np.errstate(over="ignore", invalid="ignore"):
            normal = np.cross(u, v)
        if not np.isfinite(normal).all():
            raise ValueError("surface plane cannot be represented")
        if np.any(normal[:, 2] == 0):
            raise ValueError(
                "height surfaces cannot have degenerate or vertical XY projections"
            )
        if len(np.unique(np.sort(self.mesh.triangles, axis=1), axis=0)) != len(p):
            raise ValueError("duplicate projected faces are ambiguous")
        with np.errstate(over="ignore", invalid="ignore", divide="ignore"):
            gradient = -normal[:, :2] / normal[:, 2:3]
            normal = np.c_[-gradient, np.ones(len(p))]
            normal /= np.hypot(np.hypot(gradient[:, 0], gradient[:, 1]), 1.0)[:, None]
        if not np.isfinite(gradient).all() or not np.isfinite(normal).all():
            raise ValueError("surface gradient cannot be represented")
        polygons = shapely.polygons(p[:, :, :2])
        polygons.setflags(write=False)
        object.__setattr__(self, "_polygons", polygons)
        object.__setattr__(self, "_tree", shapely.STRtree(polygons))
        object.__setattr__(self, "_gradient", owned(gradient, float))
        object.__setattr__(self, "_normal", owned(normal, float))

    def _pairs(self, xy):
        return self._tree.query(
            shapely.points(xy), predicate="dwithin", distance=self.xy_tolerance_m
        )

    def _features(self, xy, pairs):
        corners = self.mesh.triangles[pairs[1]]
        p = self.mesh.vertices[corners, :2]
        q = xy[pairs[0]]
        delta = p - q[:, None, :]
        vertex_distance = np.hypot(delta[:, :, 0], delta[:, :, 1])
        edges = np.roll(p, -1, axis=1) - p
        relative = q[:, None, :] - p
        length = np.hypot(edges[:, :, 0], edges[:, :, 1])
        unit = edges / length[:, :, None]
        edge_distance = abs(
            unit[:, :, 0] * relative[:, :, 1] - unit[:, :, 1] * relative[:, :, 0]
        )
        along = np.einsum("ijk,ijk->ij", relative, unit)
        outside = (along < 0) | (along > length)
        endpoint_distance = np.minimum(
            vertex_distance, np.roll(vertex_distance, -1, axis=1)
        )
        edge_distance = np.where(outside, endpoint_distance, edge_distance)
        row = np.arange(len(pairs[0]))
        key = np.repeat(-(pairs[1] + 1)[:, None], 2, axis=1)
        near_edge = edge_distance.min(axis=1) <= self.xy_tolerance_m
        i = np.argmin(edge_distance, axis=1)
        key[near_edge] = np.sort(
            np.c_[corners[row, i], corners[row, (i + 1) % 3]][near_edge], axis=1
        )
        near_vertex = vertex_distance.min(axis=1) <= self.xy_tolerance_m
        ids = corners[row, np.argmin(vertex_distance, axis=1)]
        key[near_vertex] = ids[near_vertex, None]
        return key

    def _shared_boundary(self, xy, faces):
        common = set(map(int, self.mesh.triangles[faces[0]]))
        for face in faces[1:]:
            common.intersection_update(map(int, self.mesh.triangles[face]))
            if not common:
                return False
        vertices = self.mesh.vertices[sorted(common), :2]
        delta = vertices - xy
        if np.any(np.hypot(delta[:, 0], delta[:, 1]) <= self.xy_tolerance_m):
            return True
        for i in range(len(vertices)):
            for j in range(i + 1, len(vertices)):
                edge = vertices[j] - vertices[i]
                length = np.hypot(*edge)
                unit = edge / length
                station = np.clip(np.dot(xy - vertices[i], unit), 0, length)
                delta = xy - (vertices[i] + station * unit)
                if np.hypot(*delta) <= self.xy_tolerance_m:
                    return True
        return False

    def _contacts(self, xy, pairs):
        pairs = pairs[:, np.lexsort((pairs[1], pairs[0]))]
        starts = np.r_[0, np.flatnonzero(np.diff(pairs[0])) + 1]
        chosen = pairs[:, starts]
        p0 = self.mesh.vertices[self.mesh.triangles[pairs[1], 0]]
        heights = p0[:, 2] + np.einsum(
            "ij,ij->i", xy[pairs[0]] - p0[:, :2], self._gradient[pairs[1]]
        )
        if len(starts) == pairs.shape[1]:
            return chosen, heights[starts], np.isfinite(heights[starts])
        features = self._features(xy, pairs)
        shared = np.all(
            np.minimum.reduceat(features, starts, axis=0)
            == np.maximum.reduceat(features, starts, axis=0),
            axis=1,
        )
        consistent = (
            np.maximum.reduceat(heights, starts) - np.minimum.reduceat(heights, starts)
            <= self.height_tolerance_m
        )
        ends = np.r_[starts[1:], pairs.shape[1]]
        for group in np.flatnonzero(~shared & consistent):
            first, last = starts[group], ends[group]
            shared[group] = self._shared_boundary(
                xy[pairs[0, first]], pairs[1, first:last]
            )
        return chosen, heights[starts], shared & consistent

    def sample(
        self, xy: np.ndarray, *, max_queries: int = 65536, max_candidates: int = 1000000
    ) -> SurfaceSamples:
        """Bound GEOS candidate emission before allocation; omit only remaining queries.

        Conservative batches account for every face as a possible hit. The last
        remaining candidate budget may therefore be insufficient for one query,
        even if its actual hit count would have been smaller. Inputs beyond the
        explicit query cap are rejected for the caller to split, not allocated.
        """
        began = time.perf_counter()
        if (
            type(max_queries) is not int
            or max_queries <= 0
            or type(max_candidates) is not int
            or max_candidates < 0
        ):
            raise ValueError(
                "positive query cap and nonnegative integer candidate budget required"
            )
        try:
            count = len(xy)
        except TypeError:
            raise ValueError(
                "finite Nx2 points within the explicit query cap required"
            ) from None
        if count > max_queries:
            raise ValueError("query input exceeds cap before conversion")
        if isinstance(xy, np.ndarray):
            if xy.ndim != 2 or xy.shape[1] != 2:
                raise ValueError("Nx2 query array required")
        elif not isinstance(xy, (list, tuple)) or any(
            not isinstance(row, (list, tuple, np.ndarray))
            or len(row) != 2
            or not all(np.isscalar(v) for v in row)
            for row in xy
        ):
            raise ValueError("bounded rows with two scalar coordinates required")
        xy = np.asarray(xy, dtype=float).reshape(-1, 2)
        if (
            xy.ndim != 2
            or xy.shape[1] != 2
            or len(xy) > max_queries
            or not np.isfinite(xy).all()
        ):
            raise ValueError("finite Nx2 points within the explicit query cap required")
        points = np.c_[xy, np.full(len(xy), np.nan)]
        normals, faces = (
            np.full((len(xy), 3), np.nan),
            np.full(len(xy), -1, dtype=np.int64),
        )
        reasons = np.full(len(xy), "outside_surface", dtype="U32")
        used, begin, batches = 0, 0, 0
        while begin < len(xy) and len(self.mesh.triangles):
            count = min(
                256,
                (max_candidates - used) // len(self.mesh.triangles),
                len(xy) - begin,
            )
            if not count:
                reasons[begin:] = "candidate_budget_exhausted"
                break
            pairs = self._pairs(xy[begin : begin + count])
            used += pairs.shape[1]
            batches += 1
            if pairs.shape[1]:
                chosen, heights, valid = self._contacts(
                    xy[begin : begin + count], pairs
                )
                rows, owners = chosen[0] + begin, chosen[1]
                reasons[rows] = "ambiguous_projected_contact"
                rows, owners, heights = rows[valid], owners[valid], heights[valid]
                points[rows, 2], normals[rows], faces[rows], reasons[rows] = (
                    heights,
                    self._normal[owners],
                    owners,
                    "",
                )
            begin += count
        report = {
            "queries": len(xy),
            "candidates": used,
            "batches": batches,
            "maximum_candidates": max_candidates,
            "valid": int(np.count_nonzero(faces >= 0)),
            "query_ms": (time.perf_counter() - began) * 1000,
        }
        return SurfaceSamples(points, normals, faces, tuple(map(str, reasons)), report)
