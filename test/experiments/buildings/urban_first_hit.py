"""Discrete first-hit CDF sampling of the original categorical recipe.

Cells retain mutually exclusive heights. One uniform coordinate selects the first
event, including height labels at coincident roof/wall contacts; no per-cell random
world or fine geometry is created. Sampling follows PBRT4 section2.3.1.
"""

import numpy as np
from column_field_query import walk_cells
from urban_parent_conditional import events, validate_eye


def merge_work(target, source, prefix=""):
    for key, value in source.items():
        if isinstance(value, dict):
            merge_work(target, value, prefix + key + "_")
        else:
            name = prefix + key
            target[name] = (
                max(target.get(name, 0), value)
                if key.endswith("peak_bytes")
                else target.get(name, 0) + value
            )


def ordered_events(parent, product, origin, direction, ids, cells, begin, leave):
    entries, masses, normals = [], [], []
    for valid, entry, normal, mass in events(
        parent, product, origin, direction, ids, cells, begin, leave
    ):
        entries.append(np.where(valid, entry, np.inf))
        masses.append(np.where(valid, mass, 0))
        normals.append(normal)
    depth = np.stack(entries, axis=1)
    mass = np.stack(masses, axis=1)
    normal = np.stack(normals, axis=1)
    order = np.argsort(depth, axis=1, kind="stable")
    return (
        np.take_along_axis(depth, order, axis=1),
        np.take_along_axis(mass, order, axis=1),
        np.take_along_axis(normal, order[:, :, None], axis=1),
        order,
    )


def sample_inputs(origins, directions, uniform, stops):
    o, d, u = (
        np.asarray(origins, float),
        np.asarray(directions, float),
        np.asarray(uniform, float),
    )
    if (
        o.ndim != 2
        or o.shape[1] != 3
        or d.shape != o.shape
        or u.shape != (len(o),)
        or not np.isfinite(o).all()
        or not np.isfinite(d).all()
        or np.any(np.linalg.norm(d, axis=1) == 0)
        or np.any(o[:, 2] < 0)
        or not np.isfinite(u).all()
        or np.any((u < 0) | (u >= 1))
    ):
        raise ValueError(
            "finite above-base rays and uniform coordinates in [0,1) required"
        )
    if stops is not None:
        stops = np.asarray(stops, float)
        if stops.shape != (len(o),) or np.any(np.isnan(stops) | (stops < 0.1)):
            raise ValueError("valid ray-parameter stops required")
    return o, d, u, stops


def sample_first_hits(
    parent, product, origins, directions, uniform, stops=None, *, resume=None
):
    result, work, _ = sample_packets(
        parent, product, origins, directions, uniform, stops, resume=resume
    )
    return result, work


def sample_packets(
    parent,
    product,
    origins,
    directions,
    uniform,
    stops=None,
    *,
    resume=None,
    step_limit=None,
):
    o, d, u, stops = sample_inputs(origins, directions, uniform, stops)
    validate_eye(parent, product, o, d)
    if resume is not None:
        bounds, initial = resume
        if (
            bounds.shape != (len(o), 2)
            or initial.shape != (len(o),)
            or not np.isfinite(initial).all()
            or np.any((initial < 0) | (initial > 1))
            or np.any(np.isnan(bounds))
            or np.any(bounds[:, 0] < 0.1)
            or np.any(bounds[:, 1] < bounds[:, 0])
        ):
            raise ValueError("bounded resume interval and prefix probability required")
    local = o - product["reference_origin_m"]
    depth, normal = np.full(len(o), np.inf), np.zeros_like(o)
    cells, levels = np.full((len(o), 2), -1, np.int64), np.full(len(o), -1, np.int64)
    transmission = np.ones(len(o)) if resume is None else resume[1].copy()
    next_depth = np.full(len(o), 0.1)
    work = {
        "steps": 0,
        "cell_ray_visits": 0,
        "height_hypothesis_tests": 0,
        "event_arrays_peak_bytes": 0,
    }
    for ids, cell, begin, leave, keep in walk_cells(
        product,
        local,
        d,
        work,
        maximum=stops,
        ray_bounds=None if resume is None else resume[0],
    ):
        entry, mass, normals, order = ordered_events(
            parent, product, local, d, ids, cell, begin, leave
        )
        remaining = transmission[ids, None] * (1 - np.cumsum(mass, axis=1))
        crossed = remaining < (1 - u[ids, None])
        chosen = crossed.argmax(axis=1)
        hit = crossed.any(axis=1)
        at, sample = np.flatnonzero(hit), ids[hit]
        slot = chosen[hit]
        depth[sample], normal[sample] = entry[at, slot], normals[at, slot]
        cells[sample], levels[sample] = cell[hit], order[at, slot]
        transmission[ids] *= 1 - mass.sum(axis=1)
        next_depth[ids] = leave
        keep[hit] = False
        work["height_hypothesis_tests"] += mass.size
        work["event_arrays_peak_bytes"] = max(
            work["event_arrays_peak_bytes"],
            sum(a.nbytes for a in [entry, mass, normals, order, remaining, crossed]),
        )
        if step_limit is not None and work["steps"] >= step_limit:
            break
    work["sampled_hits"] = int(np.isfinite(depth).sum())
    return (
        {
            "depth": depth,
            "normal": normal,
            "cell": cells,
            "height_index": levels,
        },
        work,
        (next_depth, transmission),
    )
