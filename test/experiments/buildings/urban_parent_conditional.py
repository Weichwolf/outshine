"""Categorical height expectation with view-conditioned shadows, a cost control."""

import numpy as np
from column_field_query import walk_cells, xy_interval
from urban_parent_light import light_at, shade
from urban_parent_volume import centres_at


def grid(parent):
    scale, span, pitch, fill = parent.frame[[2, 3, 4, 5]]
    first = np.floor(parent.frame[:2] / pitch).astype(np.int64)
    shape = np.ceil((parent.frame[:2] + span) / pitch).astype(np.int64) - first
    return {
        "low": np.zeros(2, np.int64),
        "shape": shape,
        "height_bounds": np.array([0, parent.heights[-1]]),
        "reference_origin_m": np.r_[(first * pitch - parent.frame[:2]) * scale, 0],
        "header": {"pitch_m": pitch * scale, "footprint_fraction": fill},
    }


def covered_cells(cell, regions):
    covered = np.zeros(len(cell), bool)
    for low, high in regions:
        covered |= np.all((cell >= low) & (cell < high), axis=1)
    return covered


def covered_grid(product, regions):
    return any(
        np.all(low <= 0) and np.all(high >= product["shape"]) for low, high in regions
    )


def density(parent, product, cell, *, caster=False):
    points = np.column_stack(
        ((cell + 0.5) * product["header"]["pitch_m"], np.zeros(len(cell)))
    )
    result = centres_at(parent, points + product["reference_origin_m"], "midpoint") * (
        1 - parent.frame[6]
    )
    regions = product.get("caster_covered" if caster else "primary_covered", ())
    return np.where(covered_cells(cell, regions), 0, result)


def blocker_probability(parent, q, height, cap):
    """P(blocker at height | primary ray proved the cell lower than cap)."""
    heights, probabilities = parent.heights, parent.probabilities
    allowed = heights[None] < cap[:, None]
    denominator = 1 - q * ((~allowed) * probabilities).sum(axis=1)
    numerator = q * (allowed * (heights[None] >= height[:, None]) * probabilities).sum(
        axis=1
    )
    return numerator / np.maximum(denominator, 1e-15)


def expected_shadow(parent, product, points, light, history, own, work):
    if covered_grid(product, product.get("caster_covered", ())):
        return np.ones(len(points))
    if np.any(light[:, 2] <= 0):
        raise ValueError("initial conditional study requires lights above the field")
    origin = points - product["reference_origin_m"]
    transmission = np.ones(len(points))
    for ids, cell, begin, leave, keep in walk_cells(
        product, origin, light, work, minimum=0
    ):
        near, far, _ = xy_interval(product, origin, light, ids, cell)
        entry = np.maximum(begin[ids], near)
        cap = np.full(len(ids), np.inf)
        for previous_cell, previous_cap in history:
            same = np.all(previous_cell[ids] == cell, axis=1)
            cap = np.where(same, np.minimum(cap, previous_cap[ids]), cap)
            work["history_cell_comparisons"] += len(ids)
        height = origin[ids, 2] + entry * light[ids, 2]
        probability = blocker_probability(
            parent, density(parent, product, cell, caster=True), height, cap
        )
        valid = (entry < np.minimum(leave, far)) & np.any(cell != own[ids], axis=1)
        transmission[ids] *= 1 - np.where(valid, probability, 0)
        keep[:] = transmission[ids] > 0
    return transmission


def record_miss(product, origin, direction, ids, cell, begin, leave):
    near, far, _ = xy_interval(product, origin, direction, ids, cell)
    entry, exit = np.maximum(near, begin[ids]), np.minimum(far, leave)
    cells = np.full((len(origin), 2), -1, np.int64)
    caps = np.full(len(origin), np.inf)
    valid = entry < exit
    cells[ids[valid]] = cell[valid]
    caps[ids[valid]] = origin[ids[valid], 2] + exit[valid] * direction[ids[valid], 2]
    return cells, caps


def events(parent, product, origin, direction, ids, cell, begin, leave):
    near, far, axis = xy_interval(product, origin, direction, ids, cell)
    q = density(parent, product, cell)
    for height, probability in zip(parent.heights, parent.probabilities, strict=True):
        roof = (height - origin[ids, 2]) / direction[ids, 2]
        entry = np.maximum(np.maximum(near, roof), begin[ids])
        valid = entry < np.minimum(far, leave)
        normal = np.zeros((len(ids), 3))
        normal[:, 2] = 1
        wall = roof < near
        normal[wall] = 0
        rows = np.flatnonzero(wall)
        normal[rows, axis[rows]] = -np.sign(direction[ids[rows], axis[rows]])
        yield valid, entry, normal, q * probability


def surface_slots(height_index, normals):
    axis = np.abs(normals).argmax(axis=1)
    sign = normals[np.arange(len(normals)), axis] < 0
    code = np.where(axis == 2, 0, 1 + 2 * axis + sign)
    return height_index * 5 + code


def surface_visibility(
    parent,
    product,
    points,
    normals,
    history,
    own,
    slots,
    rays,
    profile,
    work,
    approximation,
):
    light = light_at(points, profile)[0]
    lit = np.flatnonzero(np.einsum("ij,ij->i", normals, light) > 0)
    visibility = np.zeros(len(points))
    if profile["kind"] == "point":
        above = points[lit, 2] > parent.heights[-1]
        visibility[lit[above]] = 1
        lit = lit[~above]
    if approximation is not None:
        estimate = approximation(rays[lit], slots[lit])
        work["lookup_samples"] = work.get("lookup_samples", 0) + len(lit)
        known = np.isfinite(estimate) & (estimate >= 0) & (estimate <= 1)
        visibility[lit[known]] = estimate[known]
        lit = lit[~known]
        work["fallback_rays"] = work.get("fallback_rays", 0) + len(lit)
    h = [(c[rays][lit], z[rays][lit]) for c, z in history]
    visibility[lit] = expected_shadow(
        parent,
        product,
        points[lit] + normals[lit] * 0.001,
        light[lit],
        h,
        own[lit],
        work,
    )
    return visibility


def integrate_packet(
    parent,
    product,
    origins,
    directions,
    profile,
    work,
    receive,
    approximation=None,
    stop=None,
    receiver_normals=None,
):
    origin = origins - product["reference_origin_m"]
    transmission = np.ones(len(origin))
    history = []
    shadow_work = {"steps": 0, "cell_ray_visits": 0, "history_cell_comparisons": 0}
    maximum = stop if stop is not None else None
    if covered_grid(product, product.get("primary_covered", ())):
        maximum = np.zeros(len(origin))
    for ids, cell, begin, leave, keep in walk_cells(
        product, origin, directions, work, maximum=maximum
    ):
        all_ids = ids
        active = ~covered_cells(cell, product.get("primary_covered", ()))
        ids, cell, leave = ids[active], cell[active], leave[active]
        if not len(ids):
            continue
        blocked = np.zeros(len(ids))
        for height_index, (valid, entry, normal, probability) in enumerate(
            events(parent, product, origin, directions, ids, cell, begin, leave)
        ):
            at = np.flatnonzero(valid & (probability > 0))
            work["height_hypothesis_tests"] += len(ids)
            if not len(at):
                continue
            rays = ids[at]
            points = origins[rays] + entry[at, None] * directions[rays]
            slots = surface_slots(height_index, normal[at])
            visibility = surface_visibility(
                parent,
                product,
                points,
                normal[at],
                history,
                cell[at],
                slots,
                rays,
                profile,
                shadow_work,
                approximation,
            )
            weight = transmission[rays] * probability[at]
            receive(rays, slots, points, normal[at], weight, visibility)
            blocked += np.where(valid, probability, 0)
        transmission[ids] *= 1 - blocked
        missed_cells, caps = record_miss(
            product, origin, directions, ids, cell, begin, leave
        )
        history.append((missed_cells, caps))
        keep[:] = transmission[all_ids] > 0
    times = -origins[:, 2] / directions[:, 2] if stop is None else stop
    rays = np.flatnonzero(np.isfinite(times))
    points = origins[rays] + times[rays, None] * directions[rays]
    slots = np.full(len(rays), len(parent.heights) * 5, int)
    normals = (
        np.tile([0.0, 0.0, 1.0], (len(rays), 1))
        if receiver_normals is None
        else receiver_normals[rays]
    )
    visibility = surface_visibility(
        parent,
        product,
        points,
        normals,
        [(c[rays], z[rays]) for c, z in history],
        np.full((len(points), 2), -1),
        slots,
        np.arange(len(rays)),
        profile,
        shadow_work,
        approximation if stop is None else None,
    )
    receive(rays, slots, points, normals, transmission[rays], visibility)
    work["history_peak_bytes"] = max(
        work["history_peak_bytes"], sum(c.nbytes + z.nbytes for c, z in history)
    )
    for key, value in shadow_work.items():
        work["shadow_" + key] = work.get("shadow_" + key, 0) + value
    return transmission


def trace_transport(
    parent,
    origins,
    directions,
    profile,
    receive,
    *,
    batch=2048,
    approximation=None,
    stop=None,
    receiver_normals=None,
    primary_covered=None,
    caster_covered=None,
):
    if np.any(directions[:, 2] >= 0) or np.any(origins[:, 2] <= parent.heights[-1]):
        raise ValueError("initial conditional study requires downward rays from above")
    if profile["kind"] == "point" and profile["position"][2] <= parent.heights[-1]:
        raise ValueError("initial conditional study requires the light above the field")
    product = grid(parent)
    for key, regions in [
        ("primary_covered", primary_covered),
        ("caster_covered", caster_covered),
    ]:
        regions = np.empty((0, 2, 2), int) if regions is None else np.asarray(regions)
        if (
            regions.ndim != 3
            or regions.shape[1:] != (2, 2)
            or not np.issubdtype(regions.dtype, np.integer)
            or np.any(regions[:, 0] >= regions[:, 1])
        ):
            raise ValueError("integer complete coverage rectangles required")
        product[key] = regions
    if stop is not None:
        stop = np.asarray(stop, float)
        if stop.shape != (len(origins),) or np.any(np.isnan(stop) | (stop < 0.1)):
            raise ValueError("opaque stop must match ray parameters and near minimum")
    if receiver_normals is not None:
        receiver_normals = np.asarray(receiver_normals, float)
        if (
            receiver_normals.shape != origins.shape
            or not np.isfinite(receiver_normals).all()
            or not np.allclose(
                np.linalg.norm(receiver_normals, axis=1), 1, rtol=0, atol=1e-12
            )
        ):
            raise ValueError("finite unit receiver normals required")
    work = dict.fromkeys(
        [
            "cell_ray_visits",
            "steps",
            "height_hypothesis_tests",
            "history_peak_bytes",
            "shadow_steps",
            "shadow_cell_ray_visits",
            "shadow_history_cell_comparisons",
        ],
        0,
    )
    transmission = np.empty(len(origins))
    for first in range(0, len(origins), batch):

        def sink(rays, *terms, first=first):
            receive(first + rays, *terms)

        def estimate(rays, slots, first=first):
            return approximation(first + rays, slots)

        part = slice(first, first + batch)
        transmission[part] = integrate_packet(
            parent,
            product,
            origins[part],
            directions[part],
            profile,
            work,
            sink,
            estimate if approximation is not None else None,
            None if stop is None else stop[part],
            None if receiver_normals is None else receiver_normals[part],
        )
    return 1 - transmission, work


def render_conditional(
    parent, origins, directions, profile, *, batch=2048, approximation=None
):
    colour = np.zeros_like(origins)

    def receive(rays, slots, points, normal, weight, visibility):
        ground = slots == len(parent.heights) * 5
        for selected, albedo in [(~ground, 0.6), (ground, 0.2)]:
            at = rays[selected]
            colour[at] += weight[selected, None] * shade(
                points[selected],
                normal[selected],
                -directions[at],
                albedo,
                visibility[selected],
                profile,
            )

    coverage, work = trace_transport(
        parent,
        origins,
        directions,
        profile,
        receive,
        batch=batch,
        approximation=approximation,
    )
    return colour, coverage, work
