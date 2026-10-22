"""Direct packet DDA over shared XY columns, consuming only a finished field."""

import json
from pathlib import Path

import numpy as np


def load_product(path):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if header["schema"] != "shared-column-geometry-1":
        raise ValueError("unsupported column field schema")
    with np.load(path / "field.npz", allow_pickle=False) as archive:
        arrays = {k: archive[k] for k in header["arrays"]}
    if any(arrays[k].nbytes != size for k, size in header["arrays"].items()):
        raise ValueError("stored array size differs")
    columns = arrays["columns"]
    low, high = columns.min(0), columns.max(0) + 1
    shape = high - low
    if np.prod(shape) > 4194304:
        raise ValueError("bounded common XY index required")
    lookup = np.full(tuple(shape[::-1]), -1, np.int32)
    xy = columns - low
    lookup[xy[:, 1], xy[:, 0]] = np.arange(len(columns), dtype=np.int32)
    product = arrays | {
        "header": header,
        "lookup": lookup,
        "low": low,
        "shape": shape,
        "derived_index_bytes": lookup.nbytes,
    }
    if "native_fallback_arrays" in header:
        from triangle_image_reference import prepare_source

        with np.load(path / "fallback.npz", allow_pickle=False) as archive:
            fallback = {k: archive[k] for k in header["native_fallback_arrays"]}
        if len(fallback["indices"]):
            product["fallback"] = fallback
            product["prepared_fallback"] = prepare_source(fallback)
    return product


def test_columns(
    product, origin, direction, ids, cell, begin, leave, depth, normal, owner
):
    """Update nearest surface hits for the current packet of XY cells."""
    columns = product["lookup"][cell[:, 1], cell[:, 0]]
    low, pitch = product["low"], product["header"]["pitch_m"]
    tests = 0
    occupied = np.flatnonzero(columns >= 0)
    if len(occupied):
        offsets = product["offsets"][columns[occupied]]
        counts = product["offsets"][columns[occupied] + 1] - offsets
        for layer in range(int(counts.max())):
            at = occupied[counts > layer]
            spans_at = product["offsets"][columns[at]] + layer
            spans = product["spans"][spans_at].astype(float)
            rays = ids[at]
            dz = direction[rays, 2]
            z = origin[rays, 2]
            zt = np.divide(
                spans - z[:, None],
                dz[:, None],
                out=np.zeros_like(spans),
                where=dz[:, None] != 0,
            )
            zn = np.where(dz == 0, -np.inf, zt.min(1))
            zf = np.where(dz == 0, np.inf, zt.max(1))
            entry = np.maximum(begin[rays], zn)
            exit = np.minimum(leave[at], zf)
            inside_z = (z + 0.1 * dz >= spans[:, 0]) & (z + 0.1 * dz <= spans[:, 1])
            if np.any((begin[rays] == 0.1) & inside_z):
                raise ValueError(
                    "eye inside occupied column requires an original/finer product"
                )
            hit = (
                (entry <= exit)
                & (entry < depth[rays])
                & ((dz != 0) | ((z >= spans[:, 0]) & (z <= spans[:, 1])))
            )
            tests += len(rays)
            rr = rays[hit]
            depth[rr], owner[rr] = entry[hit], spans_at[hit]
            roof_hit = zn[hit] >= begin[rr]
            n = np.zeros((len(rr), 3))
            n[roof_hit, 2] = -np.sign(direction[rr[roof_hit], 2])
            other = ~roof_hit
            # The XY entry axis is recomputed from the current cell's slab.
            ray_cell = cell[at[hit]] + low
            p = (ray_cell + (direction[rr, :2] < 0)) * pitch
            near_xy = np.divide(
                p - origin[rr, :2],
                direction[rr, :2],
                out=np.full((len(rr), 2), -np.inf),
                where=direction[rr, :2] != 0,
            )
            axis = near_xy.argmax(1)
            rows = np.flatnonzero(other)
            n[rows, axis[rows]] = -np.sign(direction[rr[rows], axis[rows]])
            normal[rr] = n
    return tests


def trace(product, origins, directions, *, intersect_columns=test_columns):
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
    basis = product["basis"]
    origin, direction = origins @ basis.T, directions @ basis.T
    pitch = product["header"]["pitch_m"]
    low, shape = product["low"], product["shape"]
    bounds = np.array([low, low + shape]) * pitch
    dxy = direction[:, :2]
    parallel = dxy == 0
    t = np.divide(
        bounds[None] - origin[:, None, :2],
        dxy[:, None],
        out=np.zeros((len(origin), 2, 2)),
        where=~parallel[:, None],
    )
    near = np.where(parallel, -np.inf, t.min(1))
    far = np.where(parallel, np.inf, t.max(1))
    begin, end = np.maximum(0.1, near.max(1)), far.min(1)
    missed = np.any(
        parallel & ((origin[:, :2] < bounds[0]) | (origin[:, :2] >= bounds[1])), axis=1
    )
    ids = np.flatnonzero(~missed & (begin <= end))
    point = origin[ids, :2] + begin[ids, None] * dxy[ids]
    # Select the cell entered in the ray direction, including exact grid ties.
    point = np.nextafter(point / pitch, point / pitch + np.sign(dxy[ids]))
    cell = np.clip(np.floor(point).astype(np.int64) - low, 0, shape - 1)
    step = np.sign(dxy[ids]).astype(int)
    boundary = (cell + low + (step > 0)) * pitch
    crossing = np.divide(
        boundary - origin[ids, :2],
        dxy[ids],
        out=np.full((len(ids), 2), np.inf),
        where=dxy[ids] != 0,
    )
    delta = np.divide(
        pitch, np.abs(dxy[ids]), out=np.full((len(ids), 2), np.inf), where=dxy[ids] != 0
    )
    depth, owner = np.full(len(origin), np.inf), np.full(len(origin), -1, np.int32)
    normal = np.zeros((len(origin), 3))
    work = {"cell_ray_visits": 0, "interval_ray_tests": 0, "steps": 0}
    for _ in range(int(shape.sum()) + 1):
        if not len(ids):
            break
        work["steps"] += 1
        work["cell_ray_visits"] += len(ids)
        leave = np.minimum(crossing.min(1), end[ids])
        work["interval_ray_tests"] += intersect_columns(
            product, origin, direction, ids, cell, begin, leave, depth, normal, owner
        )
        keep = (depth[ids] > leave) & (leave < end[ids])
        ids, cell, step, crossing, delta = (
            ids[keep],
            cell[keep],
            step[keep],
            crossing[keep],
            delta[keep],
        )
        if not len(ids):
            break
        next_t = crossing.min(1)
        advance = crossing == next_t[:, None]
        cell += advance * step
        crossing += np.where(advance, delta, 0)
        begin[ids] = next_t
        valid = np.all((cell >= 0) & (cell < shape), axis=1)
        ids, cell, step, crossing, delta = (
            ids[valid],
            cell[valid],
            step[valid],
            crossing[valid],
            delta[valid],
        )
    if len(ids):
        raise RuntimeError("DDA exceeded the finite grid traversal bound")
    normal = normal @ basis
    if "fallback" in product:
        from triangle_image_reference import intersect_source

        fallback = product["fallback"]
        d, f = intersect_source(
            fallback, origins, directions, prepared=product["prepared_fallback"]
        )
        take = np.isfinite(d) & (d < depth)
        depth[take] = d[take]
        normal[take] = fallback["vertex_normal"][fallback["indices"][f[take], 0]]
        owner[take] = -2 - f[take]
        work["fallback_query_rays"] = len(origins)
        work["fallback_original_triangles"] = len(fallback["indices"])
    return depth, normal, owner, work
