"""Piecewise planar native roof recipes, with explicit local unsupported needs."""

import numpy as np
import shapely


def polygonal_domain(domain):
    """Retain polygonal area after clipping; boundary-only contacts carry no area."""
    if isinstance(domain, (shapely.Polygon, shapely.MultiPolygon)):
        return domain
    if isinstance(domain, shapely.GeometryCollection):
        parts = [polygonal_domain(p) for p in domain.geoms]
        parts = [p for p in parts if p.area > 0]
        return shapely.union_all(parts) if parts else shapely.Polygon()
    return shapely.Polygon()


def roof_patches(shape):
    polygon = shapely.Polygon(shape["ring"], shape["holes"])
    if not polygon.is_valid or polygon.area <= 0:
        raise ValueError("invalid original footprint")
    kind, rise = shape["roof_kind"], shape["rise_m"]
    deck = shape["seat_m"] + shape["foot_m"] + shape["eaves_m"]
    if kind == 0:
        return [(polygon, np.array([0.0, 0.0, deck + rise]))]
    if kind == 6:
        raise ValueError("curved dome requires a finer or original product")
    if kind == 2:
        hip = shape["hip"]
        if not hip or "error" in hip:
            raise ValueError("missing original hip facets")
        scale = rise / hip["maximum_distance_m"]
        origin = np.asarray(hip["origin"])
        patches = []
        for facet in hip["facets"]:
            gradient = scale * np.asarray(facet["gradient"])
            plane = np.r_[
                gradient, deck + scale * facet["intercept_m"] - gradient @ origin
            ]
            face = shapely.Polygon(np.asarray(facet["ring"]) + origin)
            patch = polygonal_domain(shapely.intersection(polygon, face))
            if patch.area > 0:
                patches.append((patch, plane))
        union = shapely.union_all([p for p, _ in patches])
        if shapely.area(shapely.symmetric_difference(union, polygon)) > 1e-7 * max(
            polygon.area, 1
        ):
            raise ValueError("hip facets do not cover the original footprint")
        return patches
    centre, u = np.array(shape["centre"]), np.array(shape["axis_u"])
    v = np.array([-u[1], u[0]])
    axis = u if kind == 5 else v
    other = np.array([-axis[1], axis[0]])
    xy = np.asarray(shape["ring"]) - centre
    low, high = (xy @ axis).min() - 1, (xy @ axis).max() + 1
    half = shape["half_v_m"]
    if half <= 0:
        raise ValueError("positive roof half width required")
    if kind == 1:
        bands = [(low, 0, rise / half, rise), (0, high, -rise / half, rise)]
    elif kind == 3:
        bands = [(low, high, rise / (2 * half), rise / 2)]
    elif kind == 4:
        b = shape["break_fraction_v"] * half
        br = shape["break_rise_m"]
        a = br / max(half - b, 0.001)
        c = (rise - br) / max(b, 0.001)
        bands = [
            (low, -b, a, a * half),
            (-b, 0, c, rise),
            (0, b, -c, rise),
            (b, high, -a, a * half),
        ]
    elif kind == 5:
        period = max(shape["period_m"], 1)
        half_u = shape["half_u_m"]
        bands = []
        for k in range(
            int(np.floor((low + half_u) / period)),
            int(np.ceil((high + half_u) / period)),
        ):
            start = k * period - half_u
            peak, end = start + 0.85 * period, start + period
            bands.extend(
                [
                    (
                        start,
                        peak,
                        rise / (0.85 * period),
                        -start * rise / (0.85 * period),
                    ),
                    (peak, end, -rise / (0.15 * period), end * rise / (0.15 * period)),
                ]
            )
    else:
        raise ValueError("unknown original roof kind")
    across = max(abs(xy @ other)) + 1
    result = []
    for a, b, slope, intercept in bands:
        ring = centre + np.array(
            [
                axis * a - other * across,
                axis * b - other * across,
                axis * b + other * across,
                axis * a + other * across,
            ]
        )
        patch = polygonal_domain(shapely.intersection(polygon, shapely.Polygon(ring)))
        if patch.area > 0:
            gradient = slope * axis
            result.append(
                (patch, np.r_[gradient, deck + intercept - gradient @ centre])
            )
    return result


def bottom_m(surface, shape):
    if shape["foot_m"] == 0:
        return shape["sole_m"]
    lower = shape["seat_m"] + shape["foot_m"] - 0.3
    minimum = surface["minimum_height_m"]
    return max(minimum, lower) if minimum != 0 else lower


def tangent_basis(plans):
    frames = np.array([s["frame"] for s in plans["summaries"] if s["supported"]])
    up = frames[:, 3].mean(0)
    up /= np.linalg.norm(up)
    east = frames[:, 1].mean(0)
    east -= (east @ up) * up
    east /= np.linalg.norm(east)
    return np.stack((east, np.cross(up, east), up))


def project_patch(surface, polygon, plane, basis, bottom):
    """Project the actual roof; vertical walls use a declared common-up approximation."""
    local_basis = np.array([surface[k] for k in ("east", "north", "up")])
    transform = local_basis @ basis.T
    origin = np.array(surface["origin"]) @ basis.T
    # Roof mapping from source XY to common XY is affine and retains curvature tilt.
    matrix = transform[:2] + plane[:2, None] * transform[2]
    offset = origin + plane[2] * transform[2]
    affine = [
        matrix[0, 0],
        matrix[1, 0],
        matrix[0, 1],
        matrix[1, 1],
        offset[0],
        offset[1],
    ]
    from shapely.affinity import affine_transform

    footprint = affine_transform(polygon, affine)
    gradient = np.linalg.solve(matrix[:, :2], matrix[:, 2])
    height = np.r_[gradient, offset[2] - offset[:2] @ gradient]
    corners = shapely.get_coordinates(polygon)
    bottom_z = (
        origin + np.column_stack((corners, np.full(len(corners), bottom))) @ transform
    )[:, 2].min()
    lateral_wall_shift = np.linalg.norm(transform[2, :2]) * np.max(
        np.abs(corners @ plane[:2] + plane[2] - bottom)
    )
    return footprint, height, bottom_z, lateral_wall_shift
