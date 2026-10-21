"""Continuous projected side room along a fixed route in one explicit support layer."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh


def projected_support(mesh):
    if (
        not isinstance(mesh, Mesh)
        or not 1 <= len(mesh.triangles) <= 4096
        or abs(mesh.vertices[:, :2]).max(initial=0) > 1e7
    ):
        raise ValueError("owned bounded nonempty support Mesh required")
    xy = mesh.vertices[mesh.triangles, :2]
    polygons = shapely.polygons(xy)
    selected = polygons[shapely.area(polygons) > 0]
    domain = shapely.union_all(selected)
    if (
        domain.is_empty
        or not domain.is_valid
        or domain.geom_type not in ("Polygon", "MultiPolygon")
    ):
        raise ValueError("support projection needs a valid nonempty area")
    return domain, {
        "input_faces": len(mesh.triangles),
        "union_operands": len(selected),
        "zero_projected_area_faces": len(polygons) - len(selected),
        "projected_coordinate_array_bytes": xy.nbytes,
        "polygon_pointer_array_bytes": polygons.nbytes,
        "selected_pointer_array_bytes": selected.nbytes,
    }


def side_clearance(domain, xy):
    if (
        domain.is_empty
        or not domain.is_valid
        or domain.geom_type not in ("Polygon", "MultiPolygon")
        or shapely.get_num_coordinates(domain) > 65536
        or not 1 <= len(xy) <= 16384
    ):
        raise ValueError("valid bounded support area and nonempty route required")
    points = np.asarray(xy, dtype=float)
    if points.ndim != 2 or points.shape[1] != 2 or not np.isfinite(points).all():
        raise ValueError("finite XY route points required")
    if abs(points).max() > 1e7 or max(abs(value) for value in domain.bounds) > 1e7:
        raise ValueError("bounded local support coordinates required")
    route = (
        shapely.Point(points[0])
        if np.all(points == points[0])
        else shapely.LineString(points)
    )
    covered = bool(domain.covers(route))
    boundary = domain.boundary
    distance = float(shapely.distance(route, boundary))
    witness = shapely.shortest_line(route, boundary)
    if not np.isfinite(distance) or witness.is_empty:
        raise ValueError("finite projected boundary distance required")
    return {
        "covered": covered,
        "available_radius_m": distance if covered else None,
        "status": "unsupported_route"
        if not covered
        else "boundary_contact"
        if distance == 0
        else "positive_side_room",
        "nearest_boundary_distance_m": distance,
        "witness_xy_m": shapely.get_coordinates(witness).tolist(),
        "route_length_m": float(route.length),
        "scope": "Numerical Cartesian distance for the complete fixed XY route to the selected layer boundary, including holes. For a covered route it is the limiting radius of a continuous horizontal disk sweep. No new body size, alternative-route optimum, vertical clearance or outside-domain admission; an artificial cut can have zero capacity pending neighbors.",
    }


def measure_routes(mesh, routes, *, method="shared"):
    if method not in ("shared", "rebuild") or not 1 <= len(routes) <= 64:
        raise ValueError("known bounded projected-route batch required")
    domain, work = projected_support(mesh)
    totals = dict(work, projection_builds=1, route_queries=0)
    results = []
    for index, route in enumerate(routes):
        if method == "rebuild" and index > 0:
            domain, work = projected_support(mesh)
            for key, value in work.items():
                totals[key] += value
            totals["projection_builds"] += 1
        results.append(side_clearance(domain, np.asarray(route["xyz_m"])[:, :2]))
        totals["route_queries"] += 1
    return results, totals
