"""Bounded ray queries on unchanged fine grid triangles, with optional outer faces."""

from dataclasses import dataclass

import numpy as np
from outshine_ref.contracts import Mesh
from terrain_grid_lod import triangles
from terrain_occluders import skirts


@dataclass(frozen=True, slots=True)
class TerrainCells:
    mesh: Mesh
    side: int
    strategy: str
    height_bounds: tuple
    xy_margin: np.ndarray
    skirt_bottoms: object


def _edges(rows, columns, cells):
    return (
        (0, rows == 0, columns),
        (1, rows == cells - 1, columns),
        (2, columns == 0, rows),
        (3, columns == cells - 1, rows),
    )


def _augment(low, high, rows, columns, bottom, cells):
    if bottom is None:
        return
    for edge, selected, along in _edges(rows, columns, cells):
        ids = np.flatnonzero(selected)
        z = bottom[edge, along[ids, None] + [0, 1]]
        low[ids, 2] = np.minimum(low[ids, 2], z.min(axis=1))
        high[ids, 2] = np.maximum(high[ids, 2], z.max(axis=1))


def prepare(vertices, faces, side, *, strategy="tree"):
    if type(side) is not int or not 2 <= side <= 33 or (side - 1) & (side - 2):
        raise ValueError("power-of-two grid with at most32cells required")
    if strategy not in ("scan", "cells", "tree"):
        raise ValueError("known triangle/cell/tree method required")
    if len(vertices) > 4096 or len(faces) > 4096:
        raise ValueError("bounded existing terrain mesh required")
    mesh = Mesh(vertices, faces)
    expected = triangles(side, 1)
    if len(mesh.vertices) < side * side or not np.array_equal(
        mesh.triangles[: len(expected)], expected
    ):
        raise ValueError("unchanged native fine grid must precede extra outer faces")
    bottom = None
    if len(mesh.triangles) > len(expected):
        source, extra, _ = skirts(mesh.vertices[: side * side], 0, side=side)
        if (
            len(mesh.vertices) != len(source)
            or not np.array_equal(mesh.triangles[len(expected) :], extra)
            or not np.array_equal(
                mesh.vertices[side * side :, :2], source[side * side :, :2]
            )
        ):
            raise ValueError("complete actual indexed outer-skirt layout required")
        bottom = mesh.vertices[side * side :, 2].reshape(4, side)
    grid = mesh.vertices[: side * side].reshape(side, side, 3)
    margin = np.zeros(2)
    bounds = [None]
    if strategy == "tree":
        corners = np.stack((grid[:-1, :-1], grid[:-1, 1:], grid[1:, :-1], grid[1:, 1:]))
        low, high = corners.min(axis=0), corners.max(axis=0)
        cells = side - 1
        rows, columns = np.indices((cells, cells))
        _augment(
            low.reshape(-1, 3),
            high.reshape(-1, 3),
            rows.ravel(),
            columns.ravel(),
            bottom,
            cells,
        )
        for level in range(1, cells.bit_length()):
            n = low.shape[0] // 2
            low = low.reshape(n, 2, n, 2, 3).min(axis=(1, 3))
            high = high.reshape(n, 2, n, 2, 3).max(axis=(1, 3))
            span = 1 << level
            corners = np.stack(
                (
                    grid[:-span:span, :-span:span],
                    grid[:-span:span, span::span],
                    grid[span::span, :-span:span],
                    grid[span::span, span::span],
                )
            )
            margin = np.maximum(
                margin,
                np.maximum(
                    corners[..., :2].min(axis=0) - low[..., :2],
                    high[..., :2] - corners[..., :2].max(axis=0),
                ).max(axis=(0, 1)),
            )
            packed = np.stack(
                (
                    np.nextafter(low[..., 2].astype(np.float32), np.float32(-np.inf)),
                    np.nextafter(high[..., 2].astype(np.float32), np.float32(np.inf)),
                )
            )
            if not np.isfinite(packed).all():
                raise ValueError("finite outward Float32 height bounds required")
            packed.setflags(write=False)
            bounds.append(packed)
        margin = np.nextafter(margin, np.inf)
    margin.setflags(write=False)
    return TerrainCells(mesh, side, strategy, tuple(bounds), margin, bottom)


def node_bounds(model, level, rows, columns):
    span = 1 << level
    first = rows * span * model.side + columns * span
    indices = first[:, None] + np.array(
        [0, span, span * model.side, span * (model.side + 1)]
    )
    corners = model.mesh.vertices[indices]
    low, high = corners.min(axis=1), corners.max(axis=1)
    if level:
        low[:, 2], high[:, 2] = model.height_bounds[level][:, rows, columns]
        low[:, :2] -= model.xy_margin
        high[:, :2] += model.xy_margin
    else:
        _augment(low, high, rows, columns, model.skirt_bottoms, model.side - 1)
    return np.nextafter(low, -np.inf), np.nextafter(high, np.inf)


def slab(low, high, origins, directions, t_min, t_max):
    moving = directions != 0
    safe = np.where(moving, directions, 1)
    a = np.nextafter(low - origins, -np.inf)
    b = np.nextafter(high - origins, np.inf)
    near = np.nextafter(np.where(safe > 0, a, b) / safe, -np.inf)
    far = np.nextafter(np.where(safe > 0, b, a) / safe, np.inf)
    near, far = np.where(moving, near, -np.inf), np.where(moving, far, np.inf)
    enters = np.maximum(near.max(axis=1), t_min)
    leaves = np.minimum(far.min(axis=1), t_max)
    valid = np.all(moving | ((origins >= low) & (origins <= high)), axis=1)
    return enters, valid & (enters <= leaves)


def _charge(work, name, count, max_work, max_pairs):
    if (
        count > max_pairs
        or work["ray_node_tests"] + work["triangle_tests"] + count > max_work
    ):
        raise ValueError(
            "terrain ray work budget exhausted; no partial result published"
        )
    work[name] += count
    work["maximum_pair_batch"] = max(work["maximum_pair_batch"], count)


def _triangles(model, origins, directions, rays, faces, output, work, budgets, t_min):
    _charge(work, "triangle_tests", len(rays), *budgets)
    work["skirt_triangle_tests"] += int(
        np.count_nonzero(faces >= 2 * (model.side - 1) ** 2)
    )
    p = model.mesh.vertices[model.mesh.triangles[faces]]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    cross = np.cross(directions[rays], b)
    determinant = np.einsum("ij,ij->i", a, cross)
    safe = np.where(determinant > 0, determinant, 1)
    delta = origins[rays] - p[:, 0]
    u = np.einsum("ij,ij->i", delta, cross) / safe
    q = np.cross(delta, a)
    v = np.einsum("ij,ij->i", directions[rays], q) / safe
    t = np.einsum("ij,ij->i", b, q) / safe
    valid = (
        (determinant > 0)
        & (u >= 0)
        & (v >= 0)
        & (u + v <= 1)
        & (t > t_min)
        & (t <= output["ray_t"][rays])
    )
    rays, faces, t, u, v = (value[valid] for value in (rays, faces, t, u, v))
    touched = np.unique(rays)
    before = output["ray_t"][touched].copy()
    np.minimum.at(output["ray_t"], rays, t)
    closest = t == output["ray_t"][rays]
    old_faces = output["face_ids"]
    old_faces[touched[output["ray_t"][touched] < before]] = np.iinfo(np.int32).max
    np.minimum.at(old_faces, rays[closest], faces[closest])
    chosen = closest & (faces == old_faces[rays])
    output["barycentric"][rays[chosen]] = np.column_stack(
        (1 - u[chosen] - v[chosen], u[chosen], v[chosen])
    )


def _pairs(rays, faces, max_pairs):
    if len(rays) * len(faces) > max_pairs:
        raise ValueError("triangle pair batch exceeds terrain ray capacity")
    return np.repeat(rays, len(faces)), np.tile(faces, len(rays))


def _cell_faces(model, rays, rows, columns, max_pairs):
    cells = model.side - 1
    edges = () if model.skirt_bottoms is None else _edges(rows, columns, cells)
    if (
        2 * (len(rays) + sum(np.count_nonzero(selected) for _, selected, _ in edges))
        > max_pairs
    ):
        raise ValueError("leaf face pairs exceed terrain ray capacity")
    ray_parts = [np.repeat(rays, 2)]
    face_parts = [(2 * (rows * cells + columns)[:, None] + [0, 1]).ravel()]
    for edge, selected, along in edges:
        ray_parts.append(np.repeat(rays[selected], 2))
        first = 2 * cells * cells + 2 * (edge * cells + along[selected])
        face_parts.append((first[:, None] + [0, 1]).ravel())
    return np.concatenate(ray_parts), np.concatenate(face_parts)


def _batch(model, origins, directions, rays, output, work, budgets, t_min, cancelled):
    cells = model.side - 1
    if model.strategy == "scan":
        _triangles(
            model,
            origins,
            directions,
            *_pairs(rays, np.arange(len(model.mesh.triangles)), budgets[1]),
            output,
            work,
            budgets,
            t_min,
        )
        return
    if model.strategy == "cells":
        if len(rays) * cells * cells > budgets[1]:
            raise ValueError("cell pair batch exceeds terrain ray capacity")
        rr, cc = np.indices((cells, cells))
        rows, columns = np.tile(rr.ravel(), len(rays)), np.tile(cc.ravel(), len(rays))
        rays = np.repeat(rays, cells * cells)
        level = 0
    else:
        level = cells.bit_length() - 1
        rows, columns = np.zeros(len(rays), dtype=int), np.zeros(len(rays), dtype=int)
    while len(rays):
        if cancelled is not None and cancelled():
            raise ValueError("terrain ray query cancelled")
        _charge(work, "ray_node_tests", len(rays), *budgets)
        if level == 0 and model.skirt_bottoms is not None:
            work["selected_boundary_height_gather_bytes"] += 16 * int(
                sum(
                    np.count_nonzero(selected)
                    for _, selected, _ in _edges(rows, columns, cells)
                )
            )
        low, high = node_bounds(model, level, rows, columns)
        _near, keep = slab(
            low, high, origins[rays], directions[rays], t_min, output["ray_t"][rays]
        )
        rays, rows, columns = (value[keep] for value in (rays, rows, columns))
        if level == 0:
            pair_rays, pair_faces = _cell_faces(model, rays, rows, columns, budgets[1])
            _triangles(
                model,
                origins,
                directions,
                pair_rays,
                pair_faces,
                output,
                work,
                budgets,
                t_min,
            )
            return
        if 4 * len(rays) > budgets[1]:
            raise ValueError("child pair batch exceeds terrain ray capacity")
        rays = np.repeat(rays, 4)
        rows = (2 * rows[:, None] + [0, 0, 1, 1]).ravel()
        columns = (2 * columns[:, None] + [0, 1, 0, 1]).ravel()
        level -= 1


def sample(
    model,
    origins,
    directions,
    *,
    max_rays=16384,
    max_pairs=262144,
    max_work=67108864,
    batch_size=64,
    t_min=0.0,
    t_max=np.inf,
    cancelled=None,
):
    if cancelled is not None and cancelled():
        raise ValueError("terrain ray query cancelled")
    if (
        any(
            type(v) is not int or v < 0
            for v in (max_rays, max_pairs, max_work, batch_size)
        )
        or not 1 <= batch_size <= 64
    ):
        raise ValueError("bounded nonnegative ray budgets and batch size1..64 required")
    if len(origins) > max_rays:
        raise ValueError("ray input count exceeds capacity before conversion")
    origins, directions = (
        np.asarray(origins, dtype=float),
        np.asarray(directions, dtype=float),
    )
    if (
        origins.ndim != 2
        or origins.shape[1:] != (3,)
        or directions.shape != origins.shape
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.all(directions == 0, axis=1))
    ):
        raise ValueError("finite Nx3 origins and nonzero directions required")
    if not np.isfinite(t_min) or not 0 <= t_min < t_max:
        raise ValueError("ordered nonnegative ray interval required")
    output = {
        "ray_t": np.full(len(origins), t_max),
        "face_ids": np.full(len(origins), np.iinfo(np.int32).max, dtype=np.int32),
        "barycentric": np.full((len(origins), 3), np.nan),
    }
    work = {
        "ray_node_tests": 0,
        "triangle_tests": 0,
        "skirt_triangle_tests": 0,
        "selected_boundary_height_gather_bytes": 0,
        "maximum_pair_batch": 0,
    }
    for begin in range(0, len(origins), batch_size):
        if cancelled is not None and cancelled():
            raise ValueError("terrain ray query cancelled")
        _batch(
            model,
            origins,
            directions,
            np.arange(begin, min(begin + batch_size, len(origins))),
            output,
            work,
            (max_work, max_pairs),
            t_min,
            cancelled,
        )
    output["valid"] = output["face_ids"] != np.iinfo(np.int32).max
    output["face_ids"][~output["valid"]] = -1
    output["ray_t"][~output["valid"]] = np.inf
    work["selected_triangle_vertex_gather_bytes"] = work["triangle_tests"] * 9 * 8
    work["selected_node_corner_gather_bytes"] = work["ray_node_tests"] * 12 * 8
    output["work"] = work
    return output
