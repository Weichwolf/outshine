"""Clip original native roof plans into analytic planar domains, without Fine meshes."""

from fractions import Fraction

import numpy as np
import shapely


def footprint(shape):
    polygon = shapely.Polygon(shape["ring"], shape["holes"])
    if polygon.is_empty or not polygon.is_valid or polygon.area <= 0:
        raise ValueError("valid nonempty original roof footprint required")
    return polygon


def band_domain(shape, band, polygon):
    centre, axis = np.asarray(shape["centre"]), np.asarray(shape["axis_u"])
    normal = np.array([-axis[1], axis[0]])
    coordinates = shapely.get_coordinates(polygon) - centre
    u, v = coordinates @ axis, coordinates @ normal
    padding = max(1.0, np.abs(coordinates).max() * 1e-12)
    interval = band["v_interval"]
    low = v.min() - padding if interval["min"] is None else interval["min"]
    high = v.max() + padding if interval["max"] is None else interval["max"]
    if low >= high:
        raise ValueError("positive original roof-band interval required")
    corners = np.array(
        [
            [u.min() - padding, low],
            [u.max() + padding, low],
            [u.max() + padding, high],
            [u.min() - padding, high],
        ]
    )
    mask = shapely.Polygon(centre + corners[:, :1] * axis + corners[:, 1:] * normal)
    domain = polygon.intersection(mask)
    gradient = band["slope_v"] * normal
    intercept = band["height_at_v0_m"] - gradient @ centre
    return domain, np.r_[gradient, intercept]


def planar_roof_domains(shape):
    """Return supplied planar domains and provenance; nonplanar roofs remain explicit."""
    polygon = footprint(shape)
    pieces = []
    for index, band in enumerate(shape["roof_bands"] or ()):
        domain, plane = band_domain(shape, band, polygon)
        if not domain.is_empty:
            pieces.append(
                {
                    "kind": "band",
                    "index": index,
                    "domain": domain,
                    "plane_enu": plane,
                    "normal_enu": np.asarray(band["normal_enu"]),
                    "v_interval": band["v_interval"],
                }
            )
    if pieces:
        return pieces
    hip = shape.get("hip")
    if shape["roof_kind"] != 2 or hip is None or not hip["available"]:
        return []
    origin = np.asarray(hip["origin"])
    scale = shape["rise_m"] / hip["maximum_distance_m"]
    for index, facet in enumerate(hip["facets"]):
        domain = polygon.intersection(
            shapely.Polygon(np.asarray(facet["ring"]) + origin)
        )
        gradient = scale * np.asarray(facet["gradient"])
        intercept = (
            shape["roof_deck_m"] + scale * facet["intercept_m"] - gradient @ origin
        )
        if not domain.is_empty:
            pieces.append(
                {
                    "kind": "hip",
                    "index": index,
                    "domain": domain,
                    "plane_enu": np.r_[gradient, intercept],
                    "normal_enu": np.asarray(facet["normal_enu"]),
                    "source_ring": facet["source_ring"],
                    "source_edge": facet["source_edge"],
                }
            )
    return pieces


def triangulate_domain(piece):
    """Emit this selected planar domain only; preserve holes and disconnected parts.

    Coordinates are the experiment's Float64 geometry, not a native mesh parity
    claim. Each emitted triangle is its own exactly planar occluder candidate.
    Opacity, acceptance, owner bounds and observer side remain caller decisions.
    """
    triangles = []
    normal = piece["normal_enu"]
    for polygon in shapely.get_parts(piece["domain"]):
        if polygon.geom_type != "Polygon" or polygon.area <= 0:
            continue
        for triangle in shapely.get_parts(
            shapely.constrained_delaunay_triangles(polygon)
        ):
            if not polygon.covers(triangle):
                raise ValueError("triangulation leaves its selected roof domain")
            xy = np.asarray(triangle.exterior.coords)[:3]
            plane = piece["plane_enu"]
            xyz = np.column_stack((xy, xy @ plane[:2] + plane[2]))
            signed_area = triangle_orientation(xy)
            if (signed_area < 0) != (normal[2] < 0):
                xyz = xyz[[0, 2, 1]]
            if signed_area == 0:
                raise ValueError("degenerate emitted roof triangle")
            triangles.append(xyz)
    return np.asarray(triangles).reshape(-1, 3, 3)


def triangle_orientation(xy):
    """Resolve cancellation in very thin but nondegenerate represented triangles."""
    delta = xy[1:] - xy[0]
    terms = delta[0, 0] * delta[1, 1], delta[0, 1] * delta[1, 0]
    area = terms[0] - terms[1]
    if abs(area) > 32 * np.finfo(float).eps * sum(abs(term) for term in terms):
        return area
    points = [[Fraction(float(value)) for value in point] for point in xy]
    a, b, c = points
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def checked_union(pieces):
    """Reject a coverage statistic if GEOS loses an input domain's interior.

    This is a diagnostic guard, not a universal exact union proof. Never snap,
    fill, buffer or repair domains merely to make a coverage statistic pass.
    The selection consumer uses individual accepted triangles, not this union.
    """
    domains = [piece["domain"] for piece in pieces]
    result = shapely.union_all(domains)
    for domain in domains:
        for polygon in shapely.get_parts(domain):
            if (
                polygon.geom_type == "Polygon"
                and polygon.area > 0
                and not result.covers(polygon.representative_point())
            ):
                raise ValueError("GEOS union lost an input domain interior")
    return result
