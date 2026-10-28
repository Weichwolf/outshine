"""Full-profile three-arm geometry behind the explicit source-qualified recipe."""

import time

import numpy as np
import shapely

from ._ground_domain import mesh_parts
from ._ground_recipe import THREE_ARM_PARAMETERS as PARAMETERS
from ._ground_recipe import THREE_ARM_RECIPE


def profile_cuts(ends, half, *, method="linear", seconds=1.0, arm_count=3):
    """Keep each entire cut inside the other support planes.

    max dot(tj, di*ti +/- hi*ni) <= dj - 0.05 is linear in the cut distances.
    Lower cut bounds also retain the full profiles at the original source hub.
    This fits the explicit cut problem only; whole geometry is checked separately.
    """
    if not np.isfinite(seconds) or seconds <= 0:
        raise ValueError("three_arm_cut_time_budget")
    reserve = PARAMETERS["cut_and_remaining_arm_reserve_m"]
    tolerance = PARAMETERS["cut_residual_limit_m"]
    lengths = np.linalg.norm(ends, axis=1)
    if (
        not 2 <= arm_count <= 6
        or ends.shape != (arm_count, 2)
        or half.shape != (arm_count,)
        or not np.isfinite(ends).all()
        or not np.isfinite(half).all()
        or np.any(half <= PARAMETERS["width_domain_m"][0] / 2)
        or np.any(half > PARAMETERS["width_domain_m"][1] / 2)
        or np.any(lengths <= PARAMETERS["arm_length_domain_m"][0])
        or np.any(lengths > PARAMETERS["arm_length_domain_m"][1])
    ):
        raise ValueError("bounded positive three-arm profiles required")
    tangent = ends / lengths[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    rows, bounds = [], []
    for a in range(arm_count):
        for b in range(arm_count):
            if a == b:
                continue
            row = np.zeros(arm_count)
            row[a], row[b] = tangent[a] @ tangent[b], -1
            rows.append(row)
            bounds.append(-half[a] * abs(normal[a] @ tangent[b]) - reserve)
    matrix, bounds = np.array(rows), np.array(bounds)
    source_profiles = half[:, None] * np.abs(normal @ tangent.T)
    lower = np.maximum(
        np.maximum(reserve, half * PARAMETERS["minimum_cut_half_width_fraction"]),
        source_profiles.max(axis=0) + reserve,
    )
    upper = lengths - reserve
    began = time.perf_counter()
    if method == "linear":
        from scipy.optimize import linprog

        result = linprog(
            np.ones(arm_count),
            A_ub=matrix,
            b_ub=bounds,
            bounds=list(zip(lower, upper)),
            method="highs",
            options={"time_limit": min(1.0, seconds)},
        )
        if not result.success or result.status != 0:
            raise ValueError("three_arm_cut_fit_incomplete_or_infeasible")
        cuts = result.x
    elif method == "common":
        denominator = -matrix.sum(axis=1)
        if np.any(denominator <= 0):
            raise ValueError("three_arm_common_cut_direction")
        cuts = np.full(arm_count, max(np.max(-bounds / denominator), np.max(lower)))
    else:
        raise ValueError("known experimental cut method required")
    if (
        not np.isfinite(cuts).all()
        or np.any(cuts > upper + tolerance)
        or np.any(cuts < lower - tolerance)
        or np.max(matrix @ cuts - bounds) > tolerance
    ):
        raise ValueError("three_arm_cut_bounds")
    return (
        cuts,
        tangent,
        normal,
        {
            "method": method,
            "cuts_m": cuts.tolist(),
            "lengths_m": lengths.tolist(),
            "constraint_rows": len(rows),
            "constraint_nonzeros": int(np.count_nonzero(matrix)),
            "variable_lower_bounds_m": lower.tolist(),
            "variable_upper_bounds_m": upper.tolist(),
            "maximum_constraint_residual_m": float(np.max(matrix @ cuts - bounds)),
            "elapsed_ms": 1000 * (time.perf_counter() - began),
        },
    )


def arm_polygons(ends, half, grades, cuts, tangent, normal, proximal):
    profiles = cuts[:, None, None] * tangent[:, None]
    profiles = (
        profiles
        + np.array([-1, 1])[None, :, None] * half[:, None, None] * normal[:, None]
    )
    original_profiles = (
        np.array([-1, 1])[None, :, None] * half[:, None, None] * normal[:, None]
    )
    polygons = [
        shapely.MultiPoint(
            np.vstack((profiles.reshape(-1, 2), original_profiles.reshape(-1, 2)))
        ).convex_hull
    ]
    frames, roles, ports, links, caps, sources, stations = (
        [np.eye(2) / min(PARAMETERS["arm_cross_gradient"], float(grades.min()))],
        [1],
        [],
        [],
        [],
        [-1],
        [],
    )
    for arm in range(3):
        length = np.linalg.norm(ends[arm])
        terminal = length - min(
            PARAMETERS["maximum_terminal_band_m"],
            PARAMETERS["terminal_band_remaining_arm_fraction"] * (length - cuts[arm]),
        )
        station = [cuts[arm]]
        if proximal and length - cuts[arm] > PARAMETERS["proximal_station_trigger_m"]:
            station.extend(
                cuts[arm]
                + np.array(PARAMETERS["proximal_station_width_factors"])
                * (2 * half[arm])
            )
        station.extend([terminal, length])
        if np.any(np.diff(station) <= 0):
            raise ValueError("three_arm_station_order")
        sections = [
            s * tangent[arm] + np.array([-1, 1])[:, None] * half[arm] * normal[arm]
            for s in station
        ]
        sections[0] = profiles[arm]
        first = len(polygons)
        for j in range(len(station) - 1):
            polygons.append(
                shapely.Polygon(np.vstack([sections[j], sections[j + 1][::-1]]))
            )
            roles.append(j % 2)
            sources.append(arm)
            frames.append(
                np.stack(
                    [
                        tangent[arm] / grades[arm],
                        normal[arm] / PARAMETERS["arm_cross_gradient"],
                    ]
                )
            )
        ports.extend(sections[:-1])
        links.extend(
            [[0, first]] + [[first + j, first + j + 1] for j in range(len(station) - 2)]
        )
        caps.append(sections[-1])
        stations.append(list(map(float, station)))
    return polygons, frames, roles, ports, links, caps, sources, stations


def layout(ends, widths, grades, *, proximal=True, cut_method="linear", seconds=1.0):
    ends, half, grades = (
        np.asarray(ends, float),
        np.asarray(widths, float) / 2,
        np.asarray(grades, float),
    )
    if (
        grades.shape != (3,)
        or not np.isfinite(grades).all()
        or np.any(grades <= 0)
        or np.any(grades > PARAMETERS["maximum_source_gradient"])
    ):
        raise ValueError("three bounded source gradients required")
    cuts, tangent, normal, report = profile_cuts(
        ends, half, method=cut_method, seconds=seconds
    )
    polygons, frames, roles, ports, links, caps, sources, stations = arm_polygons(
        ends, half, grades, cuts, tangent, normal, proximal
    )
    raw, geometry = mesh_parts(
        polygons, frames, roles, ports, links, caps, prefix="three_arm"
    )
    report.update(
        stations_m=stations,
        **geometry,
        part_source_arms=sources,
        recipe=THREE_ARM_RECIPE if proximal and cut_method == "linear" else None,
        proximal_profile_bands=proximal,
    )
    return raw, report
