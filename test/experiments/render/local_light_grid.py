"""Conservative finite-light candidates in real arithmetic; no opaque depth pruning."""

from dataclasses import dataclass

import numpy as np


@dataclass
class Grid:
    frame: object
    tile: int
    edges: np.ndarray
    headers: np.ndarray
    indices: np.ndarray
    globals: np.ndarray
    stats: dict

    def keys(self, points):
        view = self.frame.view(points)
        xy = view[:, :2] / self.frame.half
        if self.frame.perspective:
            xy /= view[:, 2, None]
        pixels = (xy * [0.5, -0.5] + 0.5) * [self.frame.width, self.frame.height]
        tiles = np.floor(pixels / self.tile).astype(int)
        nx, ny = self.extent()
        x = np.clip(tiles[:, 0], 0, nx - 1)
        y = np.clip(tiles[:, 1], 0, ny - 1)
        z = np.clip(
            np.searchsorted(self.edges, view[:, 2], side="right") - 1,
            0,
            len(self.edges) - 2,
        )
        return (z * ny + y) * nx + x

    def extent(self):
        return (
            (self.frame.width + self.tile - 1) // self.tile,
            (self.frame.height + self.tile - 1) // self.tile,
        )


def cell_boxes(frame, tile, nx, ny, edges, xyz, light_far):
    z, y, x = xyz.T
    near = edges[z]
    far = np.minimum(edges[z + 1], light_far)
    px = np.c_[x * tile, np.minimum((x + 1) * tile, frame.width)]
    py = np.c_[y * tile, np.minimum((y + 1) * tile, frame.height)]
    horizontal = (px / frame.width * 2 - 1) * frame.half[0]
    vertical = (1 - py / frame.height * 2) * frame.half[1]
    if frame.perspective:
        horizontal = (horizontal[:, :, None] * np.c_[near, far][:, None, :]).reshape(
            -1, 4
        )
        vertical = (vertical[:, :, None] * np.c_[near, far][:, None, :]).reshape(-1, 4)
    low = np.c_[horizontal.min(axis=1), vertical.min(axis=1), near]
    high = np.c_[horizontal.max(axis=1), vertical.max(axis=1), far]
    return low, high


def candidate_cells(frame, tile, nx, ny, edges, centre, radius, refine):
    zlo, zhi = centre[2] - radius, centre[2] + radius
    if zhi < frame.near:
        return np.empty(0, dtype=np.int64), 0
    if frame.perspective and zlo <= frame.near:
        lo, hi = np.array([-1, -1]), np.array([1, 1])
    elif frame.perspective:
        ratios = (
            centre[:2, None, None] + np.array([-radius, radius])[None, :, None]
        ) / np.array([zlo, zhi])[None, None, :]
        lo, hi = (
            ratios.min(axis=(1, 2)) / frame.half,
            ratios.max(axis=(1, 2)) / frame.half,
        )
    else:
        lo, hi = (centre[:2] - radius) / frame.half, (centre[:2] + radius) / frame.half
    if np.any(hi < -1) or np.any(lo > 1):
        return np.empty(0, dtype=np.int64), 0
    first = np.floor(
        (np.array([lo[0], -hi[1]]) * 0.5 + 0.5) * [frame.width, frame.height] / tile
    ).astype(int)
    last = np.floor(
        (np.array([hi[0], -lo[1]]) * 0.5 + 0.5) * [frame.width, frame.height] / tile
    ).astype(int)
    first, last = (
        np.clip(first, 0, [nx - 1, ny - 1]),
        np.clip(last, 0, [nx - 1, ny - 1]),
    )
    zfirst = max(0, np.searchsorted(edges, zlo, side="right") - 1)
    zlast = min(len(edges) - 2, np.searchsorted(edges, zhi, side="right") - 1)
    zz, yy, xx = np.meshgrid(
        np.arange(zfirst, zlast + 1),
        np.arange(first[1], last[1] + 1),
        np.arange(first[0], last[0] + 1),
        indexing="ij",
    )
    xyz = np.c_[zz.ravel(), yy.ravel(), xx.ravel()]
    tested = len(xyz)
    if refine and tested:
        low, high = cell_boxes(frame, tile, nx, ny, edges, xyz, zhi)
        distance = np.maximum(np.maximum(low - centre, centre - high), 0)
        xyz = xyz[np.einsum("ij,ij->i", distance, distance) <= radius * radius]
    return (xyz[:, 0] * ny + xyz[:, 1]) * nx + xyz[:, 2], tested


def build_grid(
    lights,
    frame,
    mode="log",
    tile=64,
    slices=24,
    index_budget=16 * 1024**2,
    refine=True,
):
    nx, ny = (frame.width + tile - 1) // tile, (frame.height + tile - 1) // tile
    if mode == "tiles":
        edges = np.array([frame.near, np.inf])
    else:
        edges = (
            np.geomspace(frame.near, frame.depth_extent, slices)
            if mode == "log"
            else np.linspace(frame.near, frame.depth_extent, slices)
        )
        edges = np.r_[edges, np.inf]
    count = nx * ny * (len(edges) - 1)
    globals_ = np.flatnonzero(lights.ranges == 0).astype("<u4")
    positions = frame.view(lights.positions)
    chunks, total, tests = [], 0, 0
    for light in np.flatnonzero(lights.ranges > 0):
        cells, tested = candidate_cells(
            frame, tile, nx, ny, edges, positions[light], lights.ranges[light], refine
        )
        total += len(cells)
        tests += tested
        if total * 4 > index_budget:
            return Grid(
                frame,
                tile,
                edges,
                np.zeros((count, 2), dtype="<u4"),
                np.empty(0, dtype="<u4"),
                np.arange(len(positions), dtype="<u4"),
                {
                    "mode": mode,
                    "overflow_full_list": True,
                    "candidate_tests": tests,
                    "accepted_before_overflow": total,
                    "index_budget_bytes": index_budget,
                },
            )
        chunks.append((cells, int(light)))
    cell_ids = (
        np.concatenate([p[0] for p in chunks]) if chunks else np.empty(0, dtype=int)
    )
    light_ids = (
        np.concatenate([np.full(len(p[0]), p[1], dtype="<u4") for p in chunks])
        if chunks
        else np.empty(0, dtype="<u4")
    )
    order = np.argsort(cell_ids, kind="stable")
    indices = light_ids[order]
    counts = np.bincount(cell_ids, minlength=count).astype("<u4")
    headers = np.c_[np.r_[0, np.cumsum(counts[:-1], dtype=np.uint64)], counts].astype(
        "<u4"
    )
    return Grid(
        frame,
        tile,
        edges,
        headers,
        indices,
        globals_,
        {
            "mode": mode,
            "overflow_full_list": False,
            "candidate_tests": tests,
            "assignments": total,
            "index_budget_bytes": index_budget,
            "major_array_bytes_at_sort": cell_ids.nbytes
            + light_ids.nbytes
            + order.nbytes
            + indices.nbytes
            + counts.nbytes
            + headers.nbytes
            + positions.nbytes
            + globals_.nbytes
            + sum(chunk[0].nbytes for chunk in chunks),
        },
    )
