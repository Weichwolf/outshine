"""Share one geometric cell walk among inverse-CDF samples of the same ray."""

import numpy as np
from column_field_query import walk_cells
from urban_first_hit import ordered_events, sample_first_hits, sample_inputs
from urban_parent_conditional import validate_eye


def sample_grouped_first_hits(
    parent, product, origins, directions, uniform, stops=None
):
    uniform = np.asarray(uniform, float)
    if (
        uniform.ndim != 2
        or not 1 <= uniform.shape[1] <= 512
        or not np.isfinite(uniform).all()
        or np.any((uniform < 0) | (uniform >= 1))
    ):
        raise ValueError("one to512 finite CDF coordinates per ray required")
    o, d, _, stops = sample_inputs(origins, directions, uniform[:, 0], stops)
    if uniform.shape[1] == 1:
        return sample_first_hits(parent, product, o, d, uniform[:, 0], stops)
    validate_eye(parent, product, o, d)
    count = uniform.shape[1]
    size = len(o) * count
    depth, normal = np.full(size, np.inf), np.zeros((size, 3))
    cells, levels = np.full((size, 2), -1, np.int64), np.full(size, -1, np.int64)
    pending, target = np.ones(uniform.shape, bool), 1 - uniform
    transmission = np.ones(len(o))
    local = o - product["reference_origin_m"]
    work = {
        "steps": 0,
        "cell_ray_visits": 0,
        "height_hypothesis_tests": 0,
        "sample_threshold_comparisons": 0,
        "event_arrays_peak_bytes": 0,
        "grouped_rays": len(o),
        "sample_state_bytes": pending.nbytes + target.nbytes + transmission.nbytes,
    }
    for ids, cell, begin, leave, keep in walk_cells(
        product, local, d, work, maximum=stops
    ):
        entry, mass, normals, order = ordered_events(
            parent, product, local, d, ids, cell, begin, leave
        )
        remaining = transmission[ids, None] * (1 - np.cumsum(mass, axis=1))
        crossed = (remaining[:, None, :] < target[ids, :, None]) & pending[ids, :, None]
        chosen, hit = crossed.argmax(axis=2), crossed.any(axis=2)
        at, draw = np.nonzero(hit)
        sample, level = ids[at] * count + draw, chosen[at, draw]
        depth[sample], normal[sample] = entry[at, level], normals[at, level]
        cells[sample], levels[sample] = cell[at], order[at, level]
        pending[ids[at], draw] = False
        transmission[ids] *= 1 - mass.sum(axis=1)
        keep[:] = pending[ids].any(axis=1)
        work["height_hypothesis_tests"] += mass.size
        work["sample_threshold_comparisons"] += mass.size * count
        work["event_arrays_peak_bytes"] = max(
            work["event_arrays_peak_bytes"],
            sum(
                a.nbytes
                for a in [entry, mass, normals, order, remaining, crossed, chosen, hit]
            ),
        )
    work["sampled_hits"] = int(np.isfinite(depth).sum())
    return {
        "depth": depth,
        "normal": normal,
        "cell": cells,
        "height_index": levels,
    }, work
