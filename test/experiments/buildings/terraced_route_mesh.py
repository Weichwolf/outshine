"""Bounded triangle-channel fallback inside an already actor-eroded core space."""

from collections import deque
from dataclasses import dataclass

import numpy as np
import shapely
from shapely.affinity import affine_transform


@dataclass(frozen=True)
class RouteLimits:
    vertices: int = 768
    triangles: int = 1024
    path_points: int = 2048

    def __post_init__(self):
        if any(
            type(v) is not int or v <= 0
            for v in (self.vertices, self.triangles, self.path_points)
        ):
            raise ValueError("Route capacities require positive integers")


DEFAULT_LIMITS = RouteLimits()


class CoreRouteMesh:
    def __init__(self, space, root, core, *, limits=DEFAULT_LIMITS):
        if not isinstance(limits, RouteLimits):
            raise TypeError("Core route mesh requires explicit RouteLimits")
        self.across, self.along, self.origin = map(
            np.asarray, (core.across_xy, core.along_xy, core.origin_xy)
        )
        self.basis = np.stack((self.across, self.along))
        input_vertices = int(shapely.get_num_coordinates(space))
        self.initial_failure = (
            "route_vertex_budget" if input_vertices > limits.vertices else None
        )
        self.space = (
            None
            if self.initial_failure
            else affine_transform(
                space,
                [
                    *self.across,
                    *self.along,
                    -self.origin @ self.across,
                    -self.origin @ self.along,
                ],
            )
        )
        self.root = (np.asarray(root) - self.origin) @ self.basis.T
        self.limits, self.triangles, self.index, self.links = limits, None, None, None
        self.reason = None
        self.mesh_failure = None
        self.work = {
            "input_vertices": input_vertices,
            "triangles": 0,
            "shared_edges": 0,
            "mesh_builds": 0,
            "route_queries": 0,
            "expanded_triangles": 0,
            "containment_queries": 0,
        }

    def _contains(self, points):
        self.work["containment_queries"] += 1
        return bool(self.space.covers(shapely.LineString(points)))

    def _prepare(self):
        if self.work["input_vertices"] > self.limits.vertices:
            self.reason = "route_vertex_budget"
            return False
        self.work["mesh_builds"] += 1
        self.triangles = np.asarray(
            shapely.get_parts(shapely.constrained_delaunay_triangles(self.space)),
            dtype=object,
        )
        count = len(self.triangles)
        if not 0 < count <= self.limits.triangles:
            self.reason = "route_triangle_budget_or_empty_mesh"
            return False
        self.work["triangles"] = count
        union = shapely.union_all(self.triangles)
        if (
            union.symmetric_difference(self.space).area > 1e-8
            or abs(float(shapely.area(self.triangles).sum()) - union.area) > 1e-8
        ):
            self.reason = "route_mesh_coverage"
            return False
        self.links = [[] for _ in range(count)]
        edges = {}
        for i, triangle in enumerate(self.triangles):
            points = tuple(map(tuple, list(triangle.exterior.coords)[:3]))
            if triangle.area <= 0:
                self.reason = "route_null_triangle"
                return False
            for a, b in zip(points, points[1:] + points[:1]):
                key = tuple(sorted((a, b)))
                if key not in edges:
                    edges[key] = i
                    continue
                previous = edges[key]
                if previous is None:
                    self.reason = "route_nonmanifold_edge"
                    return False
                middle = (np.asarray(a) + b) / 2
                self.links[i].append((previous, middle))
                self.links[previous].append((i, middle))
                self.work["shared_edges"] += 1
                edges[key] = None
        self.index = shapely.STRtree(self.triangles)
        return True

    def _channel(self, target):
        indices = [
            self.index.query(shapely.Point(p), predicate="covered_by")
            for p in (self.root, target)
        ]
        if any(not len(v) for v in indices):
            self.reason = "route_anchor_outside_triangulation"
            return None
        start, goal = (int(v.min()) for v in indices)
        queue, previous = deque([start]), {start: None}
        while queue:
            node = queue.popleft()
            self.work["expanded_triangles"] += 1
            if node == goal:
                break
            for other, middle in self.links[node]:
                if other not in previous:
                    previous[other] = (node, middle)
                    queue.append(other)
        if goal not in previous:
            self.reason = "route_disconnected_triangle_channel"
            return None
        points, node = [target], goal
        while previous[node] is not None:
            node, middle = previous[node]
            points.append(middle)
        points.append(self.root)
        return list(reversed(points))

    def _shortcut(self, points):
        if len(points) > self.limits.path_points:
            self.reason = "route_path_point_budget"
            return None
        kept, previous = [points[0]], points[0]
        for current in points[1:]:
            if np.array_equal(current, previous):
                continue
            if not self._contains([kept[-1], current]):
                if np.array_equal(previous, kept[-1]):
                    self.reason = "route_channel_segment_not_contained"
                    return None
                kept.append(previous)
            previous = current
        if not np.array_equal(previous, kept[-1]):
            kept.append(previous)
        if len(kept) < 2 or not self._contains(kept):
            self.reason = "route_shortcut_not_contained"
            return None
        return shapely.LineString(kept)

    def route(self, target):
        self.work["route_queries"] += 1
        self.reason = None
        if self.initial_failure:
            self.reason = self.initial_failure
            return None
        target = (np.asarray(target) - self.origin) @ self.basis.T
        if np.array_equal(target, self.root):
            self.reason = "coincident_route_anchors"
            return None
        if not self.space.is_valid or not all(
            shapely.covers(self.space, shapely.points([self.root, target]))
        ):
            self.reason = "route_anchor_outside_valid_free_space"
            return None
        if self._contains([self.root, target]):
            path = shapely.LineString([self.root, target])
        else:
            if self.mesh_failure is not None:
                self.reason = self.mesh_failure
                return None
            if self.index is None and not self._prepare():
                self.mesh_failure = self.reason
                return None
            channel = self._channel(target)
            if channel is None:
                return None
            path = self._shortcut(channel)
            if path is None:
                return None
        forward = [
            self.across[0],
            self.along[0],
            self.across[1],
            self.along[1],
            *self.origin,
        ]
        return affine_transform(path, forward)
