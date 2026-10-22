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


def density(parent, product, cell):
    points = np.column_stack(
        ((cell + 0.5) * product["header"]["pitch_m"], np.zeros(len(cell)))
    )
    return centres_at(parent, points + product["reference_origin_m"], "midpoint") * (
        1 - parent.frame[6]
    )


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
            parent, density(parent, product, cell), height, cap
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


def render_packet(parent, product, origins, directions, profile, work):
    origin = origins - product["reference_origin_m"]
    transmission, colour = np.ones(len(origin)), np.zeros_like(origin)
    history = []
    shadow_work = {"steps": 0, "cell_ray_visits": 0, "history_cell_comparisons": 0}
    for ids, cell, begin, leave, keep in walk_cells(product, origin, directions, work):
        blocked = np.zeros(len(ids))
        for valid, entry, normal, probability in events(
            parent, product, origin, directions, ids, cell, begin, leave
        ):
            at = np.flatnonzero(valid & (probability > 0))
            work["height_hypothesis_tests"] += len(ids)
            if not len(at):
                continue
            rays = ids[at]
            points = origins[rays] + entry[at, None] * directions[rays]
            own = cell[at]
            light = light_at(points, profile)[0]
            lit = np.einsum("ij,ij->i", normal[at], light) > 0
            visibility = np.zeros(len(rays))
            h = [(c[rays][lit], z[rays][lit]) for c, z in history]
            visibility[lit] = expected_shadow(
                parent,
                product,
                points[lit] + normal[at][lit] * 0.001,
                light[lit],
                h,
                own[lit],
                shadow_work,
            )
            weight = transmission[rays] * probability[at]
            colour[rays] += weight[:, None] * shade(
                points, normal[at], -directions[rays], 0.6, visibility, profile
            )
            blocked += np.where(valid, probability, 0)
        transmission[ids] *= 1 - blocked
        history.append(
            record_miss(product, origin, directions, ids, cell, begin, leave)
        )
        keep[:] = transmission[ids] > 0
    times = -origins[:, 2] / directions[:, 2]
    points = origins + times[:, None] * directions
    light = light_at(points, profile)[0]
    visible = expected_shadow(
        parent,
        product,
        points + [0, 0, 0.001],
        light,
        history,
        np.full((len(points), 2), -1),
        shadow_work,
    )
    colour += transmission[:, None] * shade(
        points, np.array([0, 0, 1]), -directions, 0.2, visible, profile
    )
    work["history_peak_bytes"] = max(
        work["history_peak_bytes"], sum(c.nbytes + z.nbytes for c, z in history)
    )
    for key, value in shadow_work.items():
        work["shadow_" + key] += value
    return colour, 1 - transmission


def render_conditional(parent, origins, directions, profile, *, batch=2048):
    if np.any(directions[:, 2] >= 0) or np.any(origins[:, 2] <= parent.heights[-1]):
        raise ValueError("initial conditional study requires downward rays from above")
    if profile["kind"] == "point" and profile["position"][2] <= parent.heights[-1]:
        raise ValueError("initial conditional study requires the light above the field")
    product = grid(parent)
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
    colour, coverage = np.empty_like(origins), np.empty(len(origins))
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        colour[part], coverage[part] = render_packet(
            parent, product, origins[part], directions[part], profile, work
        )
    return colour, coverage, work
