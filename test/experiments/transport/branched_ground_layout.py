"""Experimental full-profile three-arm geometry; source admission stays caller-owned."""

import time

import numpy as np
import shapely
from scipy.optimize import linprog


def profile_cuts(ends, half, *, method="linear"):
    """Keep each entire cut inside the other support planes.

    max dot(tj, di*ti +/- hi*ni) <= dj - 0.05 is linear in the cut distances.
    This fits the explicit cut problem only; whole geometry is checked separately.
    """
    lengths = np.linalg.norm(ends, axis=1)
    if (
        ends.shape != (3, 2)
        or half.shape != (3,)
        or not np.isfinite(ends).all()
        or not np.isfinite(half).all()
        or np.any(half <= 0)
        or np.any(half > 15)
        or np.any(lengths <= 0.1)
        or np.any(lengths > 512)
    ):
        raise ValueError("bounded positive three-arm profiles required")
    tangent = ends / lengths[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    rows, bounds = [], []
    for a in range(3):
        for b in range(3):
            if a == b:
                continue
            row = np.zeros(3)
            row[a], row[b] = tangent[a] @ tangent[b], -1
            rows.append(row)
            bounds.append(-half[a] * abs(normal[a] @ tangent[b]) - 0.05)
    matrix, bounds = np.array(rows), np.array(bounds)
    upper, lower = lengths - 0.05, np.maximum(0.05, half / 4)
    began = time.perf_counter()
    if method == "linear":
        result = linprog(
            np.ones(3),
            A_ub=matrix,
            b_ub=bounds,
            bounds=list(zip(lower, upper)),
            method="highs",
            options={"time_limit": 1.0},
        )
        if not result.success or result.status != 0:
            raise ValueError("three_arm_cut_fit_incomplete_or_infeasible")
        cuts = result.x
    elif method == "common":
        denominator = -matrix.sum(axis=1)
        if np.any(denominator <= 0):
            raise ValueError("three_arm_common_cut_direction")
        cuts = np.full(3, max(np.max(-bounds / denominator), np.max(lower)))
    else:
        raise ValueError("known experimental cut method required")
    if (
        not np.isfinite(cuts).all()
        or np.any(cuts > upper + 1e-10)
        or np.any(cuts < lower - 1e-10)
        or np.max(matrix @ cuts - bounds) > 1e-10
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
    polygons = [shapely.MultiPoint(profiles.reshape(-1, 2)).convex_hull]
    frames, roles, ports, links, caps, sources, stations = (
        [np.eye(2) / min(0.025, float(grades.min()))],
        [1],
        [],
        [],
        [],
        [-1],
        [],
    )
    for arm in range(3):
        length = np.linalg.norm(ends[arm])
        terminal = length - min(2.0, 0.2 * (length - cuts[arm]))
        station = [cuts[arm]]
        if proximal and length - cuts[arm] > 20:
            station.extend(cuts[arm] + np.array([0.8, 1.2]) * (2 * half[arm]))
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
            frames.append(np.stack([tangent[arm] / grades[arm], normal[arm] / 0.025]))
        ports.extend(sections[:-1])
        links.extend(
            [[0, first]] + [[first + j, first + j + 1] for j in range(len(station) - 2)]
        )
        caps.append(sections[-1])
        stations.append(list(map(float, station)))
    return polygons, frames, roles, ports, links, caps, sources, stations


def layout(ends, widths, grades, *, proximal=True, cut_method="linear"):
    ends, half, grades = (
        np.asarray(ends, float),
        np.asarray(widths, float) / 2,
        np.asarray(grades, float),
    )
    if (
        grades.shape != (3,)
        or not np.isfinite(grades).all()
        or np.any(grades <= 0)
        or np.any(grades > 1)
    ):
        raise ValueError("three bounded source gradients required")
    cuts, tangent, normal, report = profile_cuts(ends, half, method=cut_method)
    polygons, frames, roles, ports, links, caps, sources, stations = arm_polygons(
        ends, half, grades, cuts, tangent, normal, proximal
    )
    if any(p.geom_type != "Polygon" or not p.is_valid or p.area <= 0 for p in polygons):
        raise ValueError("three_arm_invalid_polygon")
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("three_arm_overlap_or_nonconforming_parts")
    complete = shapely.coverage_union_all(polygons)
    if complete.geom_type != "Polygon" or not complete.is_valid:
        raise ValueError("three_arm_disconnected_domain")
    triangles, owners = [], []
    for part, polygon in enumerate(polygons):
        tri = shapely.get_coordinates(
            shapely.constrained_delaunay_triangles(polygon)
        ).reshape(-1, 4, 2)[:, :3]
        triangles.extend(tri)
        owners.extend([part] * len(tri))
    vertices, indices = np.unique(
        np.array(triangles).reshape(-1, 2), axis=0, return_inverse=True
    )
    faces, owners = indices.reshape(-1, 3), np.asarray(owners)
    lookup = {tuple(p): i for i, p in enumerate(vertices)}
    indexed = lambda sections: np.array(
        [[lookup[tuple(p)] for p in section] for section in sections], dtype=np.int64
    )
    report.update(
        stations_m=stations,
        parts=len(polygons),
        prior_vertices=len(vertices),
        prior_faces=len(faces),
        whole_ports=len(ports),
        domain_area_m2=complete.area,
        part_source_arms=sources,
        recipe="outshine/experimental-three-arm-ground/1",
        proximal_profile_bands=proximal,
    )
    return {
        "xy": vertices,
        "faces": faces,
        "owners": owners,
        "roles": np.asarray(roles),
        "ports": indexed(ports),
        "links": np.asarray(links),
        "caps": indexed(caps),
        "frames": np.asarray(frames)[owners],
    }, report
