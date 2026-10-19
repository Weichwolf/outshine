"""Orthographic depth/PCF reference; no native raster or Float32 admission."""

from dataclasses import dataclass

import numpy as np
from shadow_translation_cost import overlap


def raster(triangles, matrix, side, rectangles=None, counters=None):
    """Fresh reverse-Z raster, pixel centres, affine depth, two-sided opaque casters."""
    result = np.zeros((side, side), dtype=np.float32)
    clips = (
        np.c_[triangles.reshape(-1, 3), np.ones(triangles.size // 3)]
        @ np.asarray(matrix).reshape(4, 4, order="F").T
    )
    if not np.isfinite(clips).all() or not (clips[:, 3] == 1).all():
        raise ValueError("finite affine orthographic input required")
    vertices = clips[:, :3].reshape(-1, 3, 3).copy()
    vertices[:, :, :2] = (vertices[:, :, :2] * [0.5, -0.5] + 0.5) * side
    if rectangles is None:
        rectangles = [(0, 0, side, side)]
    for x0, y0, x1, y1 in rectangles:
        for a, b, c in vertices:
            lo = np.maximum(
                np.ceil(np.minimum.reduce([a[:2], b[:2], c[:2]]) - 0.5), [x0, y0]
            ).astype(int)
            hi = np.minimum(
                np.floor(np.maximum.reduce([a[:2], b[:2], c[:2]]) - 0.5) + 1, [x1, y1]
            ).astype(int)
            if (hi <= lo).any():
                continue
            e, f = b - a, c - a
            determinant = e[0] * f[1] - e[1] * f[0]
            if determinant == 0:
                continue
            y, x = np.mgrid[lo[1] : hi[1], lo[0] : hi[0]].astype(float)
            x, y = x + 0.5 - a[0], y + 0.5 - a[1]
            u, v = (
                (x * f[1] - y * f[0]) / determinant,
                (y * e[0] - x * e[1]) / determinant,
            )
            depth = a[2] + u * e[2] + v * f[2]
            covered = (u >= 0) & (v >= 0) & (u + v <= 1) & (depth >= 0) & (depth <= 1)
            view = result[lo[1] : hi[1], lo[0] : hi[0]]
            np.maximum(view, np.where(covered, depth, 0).astype(np.float32), out=view)
            if counters is not None:
                counters["triangle_pixel_candidates"] += x.size
                counters["covered_samples"] += int(covered.sum())
    return result


def exposed_rectangles(side, dx, dy):
    """Disjoint new logical strips; delta is new minus old position of a world point."""
    if abs(dx) >= side or abs(dy) >= side:
        return [(0, 0, side, side)]
    rectangles = []
    left, right = max(dx, 0), min(side, side + dx)
    if dx:
        rectangles.append((0 if dx > 0 else right, 0, left if dx > 0 else side, side))
    if dy:
        rectangles.append(
            (left, 0 if dy > 0 else side + dy, right, dy if dy > 0 else side)
        )
    return rectangles


@dataclass
class Tile:
    matrix: list
    content: int
    data: np.ndarray
    origin: tuple = (0, 0)


def update(previous, triangles, matrix, side, content=0, *, ring=True, accepted=True):
    """Transactional CPU candidate. Any content change conservatively redraws the full tile."""
    plan = None
    if (
        previous is not None
        and previous.data.shape == (side, side)
        and previous.content == content
    ):
        plan = overlap(previous.matrix, matrix, side)
        if plan is None:
            return previous, {
                "new_texels": 0,
                "staging_copy_bytes": 0,
                "retained_copy_bytes": 0,
            }
    reusable = plan is not None and plan["shift"] is not None and plan["retained"] > 0
    dx, dy = plan["shift"] if reusable else (side, side)
    rectangles = exposed_rectangles(side, dx, dy)
    work = {"triangle_pixel_candidates": 0, "covered_samples": 0}
    fresh = raster(triangles, matrix, side, rectangles, work)
    origin = (0, 0)
    staging, retained = 0, 0
    if reusable and ring:
        origin = tuple(
            (o - d) % side for o, d in zip(previous.origin, (dx, dy), strict=True)
        )
        data = previous.data.copy()
        staging = data.nbytes
        for x0, y0, x1, y1 in rectangles:
            ix, iy = (
                (np.arange(x0, x1) + origin[0]) % side,
                (np.arange(y0, y1) + origin[1]) % side,
            )
            data[np.ix_(iy, ix)] = fresh[y0:y1, x0:x1]
    elif reusable:
        data = fresh
        x0, x1, y0, y1 = (
            max(dx, 0),
            min(side, side + dx),
            max(dy, 0),
            min(side, side + dy),
        )
        data[y0:y1, x0:x1] = previous.data[y0 - dy : y1 - dy, x0 - dx : x1 - dx]
        retained = (x1 - x0) * (y1 - y0) * data.itemsize
    else:
        data = fresh
    candidate = Tile(list(matrix), content, data, origin)
    costs = {
        "new_texels": sum((x1 - x0) * (y1 - y0) for x0, y0, x1, y1 in rectangles),
        "staging_copy_bytes": staging,
        "retained_copy_bytes": retained,
        **work,
    }
    return (candidate if accepted else previous), costs


def logical(tile):
    side = len(tile.data)
    x, y = (
        (np.arange(side) + tile.origin[0]) % side,
        (np.arange(side) + tile.origin[1]) % side,
    )
    return tile.data[np.ix_(y, x)]


def pcf(tile, uv, receiver, slope, *, broken_physical_offsets=False):
    """Vector four-tap shader semantics; slopes already expressed per logical texel."""
    side = len(tile.data)
    texel = np.asarray(uv) * side - 0.5
    base = np.floor(texel).astype(int)
    low, high = np.clip(base, 0, side - 1), np.clip(base + 1, 0, side - 1)
    addresses = np.stack(
        [low, np.c_[high[:, 0], low[:, 1]], np.c_[low[:, 0], high[:, 1]], high], axis=1
    )
    physical = (addresses + tile.origin) % side
    offsets = (physical if broken_physical_offsets else addresses) - texel[:, None]
    depths = tile.data[physical[:, :, 1], physical[:, :, 0]]
    depths_at_receiver = np.asarray(receiver)[:, None] + (offsets * slope[:, None]).sum(
        axis=2
    )
    taps = (depths_at_receiver >= depths).astype(float)
    phase = texel - base
    along = taps[:, ::2] * (1 - phase[:, :1]) + taps[:, 1::2] * phase[:, :1]
    return along[:, 0] * (1 - phase[:, 1]) + along[:, 1] * phase[:, 1]


def pcf_scalar(depth, uv, receiver, slope):
    """Independent scalar logical reference: explicit weighted neighbour enumeration."""
    side = len(depth)
    t = [float(v) * side - 0.5 for v in uv]
    base = [int(np.floor(v)) for v in t]
    phase = [t[i] - base[i] for i in range(2)]
    value = 0.0
    for y in range(2):
        for x in range(2):
            i, j = (
                max(0, min(side - 1, base[0] + x)),
                max(0, min(side - 1, base[1] + y)),
            )
            at_receiver = receiver + slope[0] * (i - t[0]) + slope[1] * (j - t[1])
            weight = (phase[0] if x else 1 - phase[0]) * (
                phase[1] if y else 1 - phase[1]
            )
            value += weight * (at_receiver >= float(depth[j, i]))
    return value
