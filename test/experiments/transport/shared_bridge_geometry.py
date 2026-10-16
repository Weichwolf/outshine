"""Shared corridor coverage and one height owner across same-level source joins."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import conform_triangles
from transport_vertex_pool import welded_vertices


def corridor(source):
    xy = np.asarray(source["xy"], float)
    width = source["width_m"]
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or not 2 <= len(xy) <= 256
        or not np.isfinite(xy).all()
        or np.any(np.linalg.norm(np.diff(xy, axis=0), axis=1) < 1e-6)
        or not np.isfinite(width)
        or not 0 < width <= 30
    ):
        raise ValueError("bounded finite corridor axis and width required")
    shape = shapely.LineString(xy).buffer(
        width / 2, cap_style="flat", join_style="mitre", mitre_limit=2
    )
    if shape.is_empty or not shape.is_valid:
        raise ValueError("invalid complete source corridor")
    pads = source.get("junction_pads_xy_m", [])
    if not isinstance(pads, list) or len(pads) > 8:
        raise ValueError("bounded explicit junction pad list required")
    if not pads:
        return shape
    for coordinates in pads:
        points = np.asarray(coordinates, float)
        if (
            points.ndim != 2
            or points.shape[1] != 2
            or not 3 <= len(points) <= 8
            or not np.isfinite(points).all()
        ):
            raise ValueError("finite small junction pad polygon required")
        pad = shapely.Polygon(points)
        if (
            not pad.is_valid
            or not 1e-6 < pad.area <= 30**2
            or not pad.equals(pad.convex_hull)
            or shape.intersection(pad).area <= 1e-8
            or not any(
                np.linalg.norm(points - point, axis=1).max() <= 30
                and pad.buffer(1e-8).covers(shapely.Point(point))
                for point in xy[[0, -1]]
            )
        ):
            raise ValueError("connected convex endpoint junction pad required")
        shape = shape.union(pad)
    if shape.geom_type != "Polygon" or not shape.is_valid:
        raise ValueError("junction pad changed the connected source domain")
    return shape


def layout(sources):
    if not 1 <= len(sources) <= 64:
        raise ValueError("component source budget")
    identifiers = [s["source_id"] for s in sources]
    if len(set(identifiers)) != len(identifiers):
        raise ValueError("one component product per unique source required")
    shapes = [corridor(source) for source in sources]
    domain = shapely.union_all(shapes)
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    if not 1 <= len(triangles) <= 8192:
        raise ValueError("component triangle budget")
    coordinates = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    xy, remap, displacement = welded_vertices(xy, 1e-8)
    faces = remap[inverse].reshape(-1, 3)
    signed = np.cross(
        np.c_[xy[faces[:, 1]] - xy[faces[:, 0]], np.zeros(len(faces))],
        np.c_[xy[faces[:, 2]] - xy[faces[:, 0]], np.zeros(len(faces))],
    )[:, 2]
    if np.any(signed == 0):
        raise ValueError("collapsed corridor face")
    faces[signed < 0] = faces[signed < 0, ::-1]
    faces, conformance = conform_triangles(xy, faces)
    if len(xy) > 8192 or len(faces) > 8192:
        raise ValueError("conforming component budget")
    pieces = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(pieces):
        raise ValueError("component triangles overlap or do not conform")
    error = float(shapely.coverage_union_all(pieces).symmetric_difference(domain).area)
    if error > max(1e-7, domain.area * 1e-10):
        raise ValueError("component changed source coverage")
    target, weights = np.zeros(len(xy)), np.zeros(len(xy))
    points = shapely.points(xy)
    for source, shape in zip(sources, shapes):
        touched = shapely.dwithin(shape, points, 1e-8)
        line = shapely.LineString(source["xy"])
        station = shapely.line_locate_point(line, points[touched])
        heights = np.asarray(source["end_heights_m"], float)
        if heights.shape != (2,) or not np.isfinite(heights).all():
            raise ValueError("finite original endpoint height priors required")
        target[touched] += np.interp(station, [0, line.length], heights)
        weights[touched] += 1
    if np.any(weights == 0):
        raise ValueError("component vertex lost its source")
    return (
        Mesh(np.c_[xy, target / weights], faces),
        domain,
        {
            "source_ids": identifiers,
            "vertices": len(xy),
            "faces": len(faces),
            "coverage_error_m2": error,
            "numeric_weld_m": displacement,
            "conformance": conformance,
            "prior": "Mean of all covering sources' linear endpoint-height priors",
        },
    )
