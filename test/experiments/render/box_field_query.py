"""Exact opaque flat-base box queries; no curved terrain, filtering or GPU admission."""

import numpy as np
from column_field_query import trace as trace_columns


def intervals(origins, directions, low, high):
    parallel = directions == 0
    a = np.divide(
        low - origins, directions, out=np.zeros_like(origins), where=~parallel
    )
    b = np.divide(
        high - origins, directions, out=np.zeros_like(origins), where=~parallel
    )
    near = np.where(parallel, -np.inf, np.minimum(a, b))
    far = np.where(parallel, np.inf, np.maximum(a, b))
    entry, leave = near.max(1), far.min(1)
    outside = np.any(parallel & ((origins < low) | (origins > high)), axis=1)
    entry[outside], leave[outside] = np.inf, -np.inf
    return entry, leave, near.argmax(1), far.argmin(1)


def validate_rays(origins, directions):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or len(origins) > 4194304
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("bounded finite nonzero ray packet required")
    return origins, directions


class BoxField:
    def __init__(self, heights, pitch, fill):
        heights = np.asarray(heights, np.float32)
        if (
            heights.ndim != 2
            or not heights.size
            or heights.size > 1048576
            or max(heights.shape) > 16384
            or not np.isfinite(heights).all()
            or np.any(heights < 0)
            or not np.isfinite(pitch)
            or pitch <= 0
            or not 0 < fill <= 1
        ):
            raise ValueError("finite bounded nonnegative height grid required")
        self.rows, self.columns = heights.shape
        self.pitch, self.fill = pitch, fill
        palette, indices = np.unique(heights, return_inverse=True)
        if palette[0] != 0:
            palette = np.concatenate((np.zeros(1, np.float32), palette))
            indices += 1
        self.palette = palette if len(palette) <= 256 else None
        self.maximum_height = float(heights.max())
        first = (
            indices.reshape(heights.shape).astype(np.uint8)
            if self.palette is not None
            else heights
        )
        self.mips = []
        while True:
            self.mips.append(np.pad(first, ((0, 1), (0, 1))))
            rows, columns = first.shape
            if rows == columns == 1:
                break
            padded = np.pad(first, ((0, rows % 2), (0, columns % 2)))
            first = padded.reshape((rows + 1) // 2, 2, (columns + 1) // 2, 2).max(
                (1, 3)
            )
        self.top = len(self.mips) - 1

    @property
    def retained_bytes(self):
        return sum(m.nbytes for m in self.mips) + (
            0 if self.palette is None else self.palette.nbytes
        )

    def trace_hierarchy(self, origins, directions, *, packet_rays=16384):
        origins, directions = validate_rays(origins, directions)
        if not isinstance(packet_rays, int) or not 0 < packet_rays <= 65536:
            raise ValueError("packet ray limit must be between one and 65536")
        depth, normal = np.full(len(origins), np.inf), np.zeros_like(origins)
        work = {
            "node_ray_visits": 0,
            "box_ray_tests": 0,
            "leaf_ray_tests": 0,
            "steps": 0,
            "max_stack": 0,
            "retained_bytes": self.retained_bytes,
            "stack_bytes": 0,
        }
        for first in range(0, len(origins), packet_rays):
            part = slice(first, first + packet_rays)
            d, n, w = self._trace_packet(origins[part], directions[part])
            depth[part], normal[part] = d, n
            for key in ("node_ray_visits", "box_ray_tests", "leaf_ray_tests", "steps"):
                work[key] += w[key]
            for key in ("max_stack", "stack_bytes"):
                work[key] = max(work[key], w[key])
        return depth, normal, work

    def boxes(self, level, x, y):
        scale = 1 << level
        low = np.column_stack(
            (x * scale * self.pitch, y * scale * self.pitch, np.zeros(len(x)))
        )
        high = np.minimum(
            low + [scale * self.pitch, scale * self.pitch, 0],
            [self.columns * self.pitch, self.rows * self.pitch, np.inf],
        )
        values = self.mips[level][y, x]
        high[:, 2] = self.palette[values] if self.palette is not None else values
        inset = (1 - self.fill) * self.pitch / 2
        low[:, :2] += inset
        high[:, :2] -= inset
        return low, high

    def root_hits(self, origins, directions):
        inset = (1 - self.fill) * self.pitch / 2
        low = np.array([inset, inset, 0.0])
        high = [
            self.columns * self.pitch - inset,
            self.rows * self.pitch - inset,
            self.maximum_height,
        ]
        entry, leave, _, _ = intervals(origins, directions, low, high)
        return (entry <= leave) & (leave > 0.1) & (high[2] > 0)

    def leaf_hits(self, origins, directions, x, y):
        low, high = self.boxes(0, x, y)
        entry, leave, near_axis, far_axis = intervals(origins, directions, low, high)
        entering = entry > 0.1
        depth = np.where(entering, entry, leave)
        hit = (entry <= leave) & (depth > 0.1) & (high[:, 2] > 0)
        normal = np.zeros_like(origins)
        axis = np.where(entering, near_axis, far_axis)
        at = np.arange(len(x))
        normal[at, axis] = np.where(entering, -1, 1) * np.sign(directions[at, axis])
        return np.where(hit, depth, np.inf), normal

    def trace_direct(self, origins, directions):
        origins, directions = validate_rays(origins, directions)
        keep = np.flatnonzero(self.root_hits(origins, directions))
        depth, normal = np.full(len(origins), np.inf), np.zeros_like(origins)

        def intersect(product, origin, direction, ids, cell, begin, leave, d, n, owner):
            t, normals = self.leaf_hits(
                origin[ids], direction[ids], cell[:, 0], cell[:, 1]
            )
            hit = t < d[ids]
            rays = ids[hit]
            d[rays], n[rays] = t[hit], normals[hit]
            owner[rays] = cell[hit, 1] * self.columns + cell[hit, 0]
            return len(ids)

        product = {
            "basis": np.eye(3),
            "low": np.zeros(2, int),
            "shape": np.array([self.columns, self.rows]),
            "header": {"pitch_m": self.pitch},
        }
        d, n, _, work = trace_columns(
            product, origins[keep], directions[keep], intersect_columns=intersect
        )
        depth[keep], normal[keep] = d, n
        work["box_ray_tests"] = len(origins) + work["interval_ray_tests"]
        work["grid_xy_ray_tests"] = len(keep)
        return depth, normal, work

    def _trace_packet(self, origins, directions):
        depth, normal = np.full(len(origins), np.inf), np.zeros_like(origins)
        size = 3 * self.top + 1
        stack = np.zeros((len(origins), size), np.uint32)
        pending = np.zeros(len(origins), np.int32)
        keep = self.root_hits(origins, directions)
        stack[keep, 0], pending[keep] = self.top << 28, 1
        work = {
            "node_ray_visits": 0,
            "box_ray_tests": len(origins),
            "leaf_ray_tests": 0,
            "steps": 0,
            "max_stack": int(np.any(keep)),
            "retained_bytes": sum(m.nbytes for m in self.mips)
            + (0 if self.palette is None else self.palette.nbytes),
            "stack_bytes": stack.nbytes + pending.nbytes,
        }
        limit = sum(m.size for m in self.mips) + 1
        for _ in range(limit):
            rays = np.flatnonzero(pending)
            if not len(rays):
                break
            work["steps"] += 1
            work["node_ray_visits"] += len(rays)
            pending[rays] -= 1
            nodes = stack[rays, pending[rays]]
            levels, x, y = nodes >> 28, nodes & 16383, (nodes >> 14) & 16383
            for level in np.unique(levels):
                take = levels == level
                ids, xx, yy = rays[take], x[take], y[take]
                work["box_ray_tests"] += len(ids)
                if level == 0:
                    work["leaf_ray_tests"] += len(ids)
                    t, n = self.leaf_hits(origins[ids], directions[ids], xx, yy)
                    hit = t < depth[ids]
                    depth[ids[hit]], normal[ids[hit]] = t[hit], n[hit]
                    continue
                low, high = self.boxes(int(level), xx, yy)
                entry, leave, _, _ = intervals(origins[ids], directions[ids], low, high)
                valid = (
                    (entry <= leave)
                    & (leave > 0.1)
                    & (entry < depth[ids])
                    & (high[:, 2] > 0)
                )
                ids, xx, yy = ids[valid], xx[valid], yy[valid]
                work["box_ray_tests"] += 4 * len(ids)
                entries, children = [], []
                for dx, dy in ((0, 0), (1, 0), (0, 1), (1, 1)):
                    cx, cy = xx * 2 + dx, yy * 2 + dy
                    lo, hi = self.boxes(int(level) - 1, cx, cy)
                    a, b, _, _ = intervals(origins[ids], directions[ids], lo, hi)
                    ok = (a <= b) & (b > 0.1) & (a < depth[ids]) & (hi[:, 2] > 0)
                    entries.append(np.where(ok, a, np.inf))
                    children.append(((level - 1) << 28) | (cy << 14) | cx)
                entries, children = np.array(entries).T, np.array(children).T
                order = np.argsort(-entries, axis=1, kind="stable")
                for i in range(4):
                    chosen = order[:, i]
                    at = np.arange(len(ids))
                    good = np.isfinite(entries[at, chosen])
                    rr = ids[good]
                    if np.any(pending[rr] >= size):
                        raise RuntimeError("bounded traversal stack overflow")
                    stack[rr, pending[rr]] = children[at[good], chosen[good]]
                    pending[rr] += 1
                work["max_stack"] = max(work["max_stack"], int(pending.max(initial=0)))
        else:
            raise RuntimeError("finite hierarchy traversal bound exceeded")
        return depth, normal, work
