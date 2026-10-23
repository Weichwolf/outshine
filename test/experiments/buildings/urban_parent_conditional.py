"""Categorical height expectation with view-conditioned shadows, a cost control."""

from itertools import pairwise

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
    """Test declared rectangle union using only its boundary coordinates."""
    if any(
        np.all(low <= 0) and np.all(high >= product["shape"]) for low, high in regions
    ):
        return True
    if len(regions) < 2:
        return False
    regions = np.asarray(regions, np.int64)
    low = np.maximum(regions[:, 0], 0)
    high = np.minimum(regions[:, 1], product["shape"])
    valid = np.all(low < high, axis=1)
    low, high = low[valid], high[valid]
    cuts = np.unique(np.r_[0, product["shape"][0], low[:, 0], high[:, 0]])
    for left, right in pairwise(cuts):
        active = (low[:, 0] <= left) & (high[:, 0] >= right)
        intervals = np.c_[low[active, 1], high[active, 1]]
        intervals = intervals[np.argsort(intervals[:, 0], kind="stable")]
        covered = 0
        for first, last in intervals:
            if first > covered:
                return False
            covered = max(covered, last)
        if covered < product["shape"][1]:
            return False
    return True


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


def new_shadow_work(*, history=False):
    work = dict.fromkeys(
        [
            "steps",
            "cell_ray_visits",
            "primary_condition_tests",
            "conditioning_peak_bytes",
            "shared_wall_visibilities",
        ],
        0,
    )
    if history:
        work["history_cell_comparisons"] = 0
    return work


def primary_cap(product, primary, ids, cell, work):
    """Intersect one caster footprint with the known-missed primary prefix."""
    origin, direction, limit = primary
    near, far, _ = xy_interval(product, origin, direction, ids, cell)
    entry, exit = np.maximum(near, 0.1), np.minimum(far, limit[ids])
    valid = entry < exit
    covered = covered_cells(cell, product.get("primary_covered", ()))
    if product.get("camera_miss_witness", False):
        work["camera_miss_conditioned_cells"] = work.get(
            "camera_miss_conditioned_cells", 0
        ) + int(np.count_nonzero(valid & covered))
    else:
        valid &= ~covered
    cap = np.full(len(ids), np.inf)
    rays = ids[valid]
    first_z = origin[rays, 2] + entry[valid] * direction[rays, 2]
    last_z = origin[rays, 2] + exit[valid] * direction[rays, 2]
    cap[valid] = np.minimum(first_z, last_z)
    # An opaque endpoint owns an exact tie; the primary prefix is open there.
    endpoint = valid & (exit == limit[ids]) & (direction[ids, 2] < 0)
    cap[endpoint] = np.nextafter(cap[endpoint], np.inf)
    work["primary_condition_tests"] += len(ids)
    return cap


def expected_shadow(parent, product, points, light, primary, own, work):
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
        cap = primary_cap(product, primary, ids, cell, work)
        height = origin[ids, 2] + entry * light[ids, 2]
        probability = blocker_probability(
            parent, density(parent, product, cell, caster=True), height, cap
        )
        valid = (entry < np.minimum(leave, far)) & np.any(cell != own[ids], axis=1)
        transmission[ids] *= 1 - np.where(valid, probability, 0)
        keep[:] = transmission[ids] > 0
    return transmission


def events(parent, product, origin, direction, ids, cell, begin, leave):
    near, far, axis = xy_interval(product, origin, direction, ids, cell)
    q = density(parent, product, cell)
    dz, z = direction[ids, 2], origin[ids, 2]
    for height, probability in zip(parent.heights, parent.probabilities, strict=True):
        vertical = np.divide(
            np.column_stack((-z, height - z)),
            dz[:, None],
            out=np.zeros((len(ids), 2)),
            where=dz[:, None] != 0,
        )
        zn = np.where(dz == 0, -np.inf, vertical.min(axis=1))
        zf = np.where(dz == 0, np.inf, vertical.max(axis=1))
        entry = np.maximum(np.maximum(near, zn), begin[ids])
        valid = (
            (entry < leave)
            & (entry <= np.minimum(far, zf))
            & ((dz != 0) | ((z >= 0) & (z <= height)))
        )
        normal = np.zeros((len(ids), 3))
        normal[:, 2] = -np.sign(dz)
        wall = zn < near
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
    primary,
    own,
    slots,
    rays,
    profile,
    work,
    approximation,
    wall_visibility=None,
    shadow_query=None,
):
    light = light_at(points, profile)[0]
    visibility = np.zeros(len(points))
    lit_mask = np.einsum("ij,ij->i", normals, light) > 0
    if wall_visibility is not None and approximation is None:
        shared = (normals[:, 2] == 0) & np.isfinite(wall_visibility[rays])
        visibility[shared] = wall_visibility[rays[shared]]
        work["shared_wall_visibilities"] = work.get(
            "shared_wall_visibilities", 0
        ) + int(shared.sum())
        lit_mask &= ~shared
    lit = np.flatnonzero(lit_mask)
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
    conditioned = tuple(a[lit] for a in primary)
    work["conditioning_peak_bytes"] = max(
        work["conditioning_peak_bytes"],
        sum(a.nbytes for a in primary) + sum(a.nbytes for a in conditioned),
    )
    query = expected_shadow if shadow_query is None else shadow_query
    visibility[lit] = query(
        parent,
        product,
        points[lit] + normals[lit] * 0.001,
        light[lit],
        conditioned,
        own[lit],
        work,
    )
    if wall_visibility is not None and approximation is None:
        wall = normals[:, 2] == 0
        wall_visibility[rays[wall]] = visibility[wall]
    return visibility


def ground_stops(origins, directions):
    times = np.divide(
        -origins[:, 2],
        directions[:, 2],
        out=np.full(len(origins), np.inf),
        where=directions[:, 2] < 0,
    )
    return np.where(times >= 0.1, times, np.inf)


def validate_eye(parent, product, origins, directions):
    near_z = origins[:, 2] + 0.1 * directions[:, 2]
    at = np.flatnonzero((near_z >= 0) & (near_z <= parent.heights[-1]))
    point = (
        origins[at, :2] + 0.1 * directions[at, :2] - product["reference_origin_m"][:2]
    ) / product["header"]["pitch_m"]
    point = point[np.all((point >= 0) & (point < product["shape"]), axis=1)]
    cell = np.floor(point).astype(np.int64)
    fraction = point - cell
    edge = (1 - parent.frame[5]) / 2
    inside = np.all((fraction >= edge) & (fraction <= 1 - edge), axis=1)
    if np.any(density(parent, product, cell[inside]) > 0):
        raise ValueError(
            "eye inside possible unresolved mass requires a resolved product"
        )


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
    transmission_tolerance=0.0,
    shadow_query=None,
    receiver_cells=None,
):
    origin = origins - product["reference_origin_m"]
    transmission = np.ones(len(origin))
    traversed = np.full(len(origin), 0.1)
    shadow_work = new_shadow_work(history=True)
    maximum = stop if stop is not None else None
    if covered_grid(product, product.get("primary_covered", ())):
        maximum = np.zeros(len(origin))
    for ids, cell, begin, leave, keep in walk_cells(
        product, origin, directions, work, maximum=maximum
    ):
        all_ids = ids
        traversed[ids] = leave
        active = ~covered_cells(cell, product.get("primary_covered", ()))
        ids, cell, leave = ids[active], cell[active], leave[active]
        if not len(ids):
            continue
        blocked = np.zeros(len(ids))
        wall_visibility = None
        for height_index, (valid, entry, normal, probability) in enumerate(
            events(parent, product, origin, directions, ids, cell, begin, leave)
        ):
            at = np.flatnonzero(valid & (probability > 0))
            work["height_hypothesis_tests"] += len(ids)
            if not len(at):
                continue
            if (
                wall_visibility is None
                and approximation is None
                and height_index + 1 < len(parent.heights)
                and np.any(normal[at, 2] == 0)
            ):
                wall_visibility = np.full(len(origin), np.nan)
                work["wall_visibility_peak_bytes"] = max(
                    work["wall_visibility_peak_bytes"], wall_visibility.nbytes
                )
            rays = ids[at]
            points = origins[rays] + entry[at, None] * directions[rays]
            slots = surface_slots(height_index, normal[at])
            visibility = surface_visibility(
                parent,
                product,
                points,
                normal[at],
                (origin[rays], directions[rays], entry[at]),
                cell[at],
                slots,
                rays,
                profile,
                shadow_work,
                approximation,
                wall_visibility,
                shadow_query,
            )
            weight = transmission[rays] * probability[at]
            receive(rays, slots, points, normal[at], weight, visibility)
            blocked += np.where(valid, probability, 0)
        transmission[ids] *= 1 - blocked
        terminated = (transmission[all_ids] > 0) & (
            transmission[all_ids] <= transmission_tolerance
        )
        work["early_terminated_rays"] += int(terminated.sum())
        work["truncated_probability_sum"] += float(
            transmission[all_ids[terminated]].sum()
        )
        keep[:] = transmission[all_ids] > transmission_tolerance
    times = ground_stops(origins, directions) if stop is None else stop
    primary_limit = times if product.get("camera_miss_witness", False) else traversed
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
        (origin[rays], directions[rays], primary_limit[rays]),
        np.full((len(points), 2), -1)
        if receiver_cells is None
        else receiver_cells[rays],
        slots,
        np.arange(len(rays)),
        profile,
        shadow_work,
        approximation if stop is None else None,
        shadow_query=shadow_query,
    )
    receive(rays, slots, points, normals, transmission[rays], visibility)
    for key, value in shadow_work.items():
        work["shadow_" + key] = (
            max(work.get("shadow_" + key, 0), value)
            if key.endswith("peak_bytes")
            else work.get("shadow_" + key, 0) + value
        )
    return transmission


def transport_inputs(
    parent,
    origins,
    directions,
    profile,
    stop=None,
    receiver_normals=None,
    primary_covered=None,
    caster_covered=None,
    transmission_tolerance=0.0,
    camera_miss_witness=False,
):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.shape != directions.shape
        or origins.ndim != 2
        or origins.shape[1] != 3
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
        or np.any(origins[:, 2] < 0)
    ):
        raise ValueError("finite nonzero rays from above-ground origins required")
    if not np.isfinite(transmission_tolerance) or not 0 <= transmission_tolerance < 1:
        raise ValueError("finite transmission tolerance in [0, 1) required")
    if transmission_tolerance and not np.allclose(
        np.linalg.norm(directions, axis=1), 1, rtol=0, atol=1e-12
    ):
        raise ValueError("bounded radiance termination requires unit view directions")
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
    validate_eye(parent, product, origins, directions)
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
    if type(camera_miss_witness) is not bool:
        raise ValueError("camera miss witness must be an explicit boolean")
    if camera_miss_witness:
        if stop is None or not len(product["primary_covered"]):
            raise ValueError(
                "camera miss witness requires queried stops and complete camera regions"
            )
        product["camera_miss_witness"] = True
    return origins, directions, product, stop, receiver_normals


def validate_receiver_cells(cells, product, origins, stop, normals):
    """Explicit owner proof requires a finite stop in a complete camera cell.

    The caller certifies an outward face of the sole matching convex cell body;
    arbitrary opaque/foreground stops carry no such proof. No Caster-Ready domain
    is inferred. Only that receiver body's own shadow hypothesis is excluded.
    """
    if cells is None:
        return None
    cells = np.asarray(cells)
    if cells.shape != (len(origins), 2) or not np.issubdtype(cells.dtype, np.integer):
        raise ValueError("one integer receiver-owner cell per ray required")
    known = np.all(cells >= 0, axis=1)
    unknown = np.all(cells == -1, axis=1)
    if (
        np.any(~(known | unknown))
        or np.any(cells[known] >= product["shape"])
        or (np.any(known) and (stop is None or normals is None))
    ):
        raise ValueError(
            "bounded owner cells and explicit opaque stops/normals required"
        )
    if np.any(known) and (
        not np.isfinite(stop[known]).all()
        or not covered_cells(cells[known], product["primary_covered"]).all()
    ):
        raise ValueError("known receiver must have a finite stop in camera-ready cells")
    return cells


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
    transmission_tolerance=0.0,
    shadow_query=None,
    receiver_cells=None,
    camera_miss_witness=False,
):
    origins, directions, product, stop, receiver_normals = transport_inputs(
        parent,
        origins,
        directions,
        profile,
        stop,
        receiver_normals,
        primary_covered,
        caster_covered,
        transmission_tolerance,
        camera_miss_witness,
    )
    receiver_cells = validate_receiver_cells(
        receiver_cells, product, origins, stop, receiver_normals
    )
    work = dict.fromkeys(
        [
            "cell_ray_visits",
            "steps",
            "height_hypothesis_tests",
            "history_peak_bytes",
            "shadow_steps",
            "shadow_cell_ray_visits",
            "shadow_history_cell_comparisons",
            "early_terminated_rays",
            "truncated_probability_sum",
            "wall_visibility_peak_bytes",
        ],
        0,
    )
    if receiver_cells is not None:
        work["receiver_owner_rays"] = int(np.all(receiver_cells >= 0, axis=1).sum())
        work["receiver_owner_input_peak_bytes"] = receiver_cells.nbytes
    if camera_miss_witness:
        work["camera_miss_witness_rays"] = len(origins)
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
            transmission_tolerance,
            shadow_query,
            None if receiver_cells is None else receiver_cells[part],
        )
    return 1 - transmission, work


def render_conditional(
    parent,
    origins,
    directions,
    profile,
    *,
    batch=2048,
    approximation=None,
    transmission_tolerance=0.0,
    shadow_query=None,
):
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
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
        transmission_tolerance=transmission_tolerance,
        shadow_query=shadow_query,
    )
    return colour, coverage, work
