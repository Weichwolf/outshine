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


def xy_interval(product, origin, direction, rays, cell):
    """Optional centred lots retain XY gaps while sharing the packet traversal."""
    pitch = product["header"]["pitch_m"]
    fraction = product["header"].get("footprint_fraction", 1.0)
    if not 0 < fraction <= 1:
        raise ValueError("bounded column footprint fraction required")
    low = (cell + product["low"] + (1 - fraction) / 2) * pitch
    high = low + pitch * fraction
    dxy = direction[rays, :2]
    parallel = dxy == 0
    times = np.divide(
        np.stack((low, high), axis=1) - origin[rays, None, :2],
        dxy[:, None],
        out=np.zeros((len(rays), 2, 2)),
        where=~parallel[:, None],
    )
    near = np.where(parallel, -np.inf, times.min(axis=1))
    far = np.where(parallel, np.inf, times.max(axis=1)).min(axis=1)
    missed = np.any(
        parallel & ((origin[rays, :2] < low) | (origin[rays, :2] > high)), axis=1
    )
    return near.max(axis=1), np.where(missed, -np.inf, far), near.argmax(axis=1)


def clip_height(product, origin, direction, begin, end):
    if "height_bounds" not in product:
        return begin, end
    bounds = np.asarray(product["height_bounds"], float)
    dz = direction[:, 2]
    t = np.divide(
        bounds[None] - origin[:, 2, None],
        dz[:, None],
        out=np.zeros((len(origin), 2)),
        where=dz[:, None] != 0,
    )
    near = np.where(dz == 0, -np.inf, t.min(axis=1))
    far = np.where(dz == 0, np.inf, t.max(axis=1))
    missed = (dz == 0) & ((origin[:, 2] < bounds[0]) | (origin[:, 2] > bounds[1]))
    return np.maximum(begin, near), np.where(missed, -np.inf, np.minimum(end, far))


def test_columns(
    product, origin, direction, ids, cell, begin, leave, depth, normal, owner, minimum
):
    """Update nearest surface hits for the current packet of XY cells."""
    columns = product["lookup"][cell[:, 1], cell[:, 0]]
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
            xy_near, xy_far, xy_axis = begin[rays], leave[at], None
            if product["header"].get("footprint_fraction", 1.0) != 1:
                xy_near, xy_far, xy_axis = xy_interval(
                    product, origin, direction, rays, cell[at]
                )
            entry = np.maximum(np.maximum(begin[rays], zn), xy_near)
            exit = np.minimum(np.minimum(leave[at], zf), xy_far)
            inside_z = (z + minimum * dz >= spans[:, 0]) & (
                z + minimum * dz <= spans[:, 1]
            )
            if np.any(
                (begin[rays] == minimum)
                & inside_z
                & (xy_near <= minimum)
                & (xy_far >= minimum)
            ):
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
            roof_hit = zn[hit] >= np.maximum(begin[rr], xy_near[hit])
            n = np.zeros((len(rr), 3))
            n[roof_hit, 2] = -np.sign(direction[rr[roof_hit], 2])
            other = ~roof_hit
            axis = (
                xy_interval(product, origin, direction, rr, cell[at[hit]])[2]
                if xy_axis is None
                else xy_axis[hit]
            )
            rows = np.flatnonzero(other)
            n[rows, axis[rows]] = -np.sign(direction[rr[rows], axis[rows]])
            normal[rr] = n
    return tests


def walk_cells(product, origin, direction, work, *, minimum=0.1, maximum=None):
    """Yield ordered XY packets; consumers can clear keep to stop a ray."""
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
    begin, end = np.maximum(minimum, near.max(1)), far.min(1)
    begin, end = clip_height(product, origin, direction, begin, end)
    if maximum is not None:
        end = np.minimum(end, maximum)
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
    for _ in range(int(shape.sum()) + 1):
        if not len(ids):
            break
        work["steps"] += 1
        work["cell_ray_visits"] += len(ids)
        leave = np.minimum(crossing.min(1), end[ids])
        keep = np.ones(len(ids), bool)
        yield ids, cell, begin, leave, keep
        keep &= leave < end[ids]
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


def trace(product, origins, directions, *, minimum=0.1):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or len(origins) > 4194304
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
        or not np.isfinite(minimum)
        or minimum < 0
    ):
        raise ValueError("bounded finite nonzero ray packet required")
    basis = product["basis"]
    origin, direction = origins @ basis.T, directions @ basis.T
    depth, owner = np.full(len(origin), np.inf), np.full(len(origin), -1, np.int32)
    normal = np.zeros((len(origin), 3))
    work = {"cell_ray_visits": 0, "interval_ray_tests": 0, "steps": 0}
    for ids, cell, begin, leave, keep in walk_cells(
        product, origin, direction, work, minimum=minimum
    ):
        work["interval_ray_tests"] += test_columns(
            product,
            origin,
            direction,
            ids,
            cell,
            begin,
            leave,
            depth,
            normal,
            owner,
            minimum,
        )
        keep[:] = depth[ids] > leave
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
