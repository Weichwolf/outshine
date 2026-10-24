"""Bounded planar arrangement for one declared continuous body-ground surface."""

import numpy as np
import shapely


def grid(x, y):
    xx, yy = np.meshgrid(x, y)
    xy = np.column_stack((xx.ravel(), yy.ravel()))
    ids = np.arange(len(xy)).reshape(xx.shape)
    a, b = ids[:-1, :-1].ravel(), ids[:-1, 1:].ravel()
    c, d = ids[1:, :-1].ravel(), ids[1:, 1:].ravel()
    faces = np.vstack((np.column_stack((a, b, d)), np.column_stack((a, d, c))))
    return xy, faces


def arrange(model, xy, faces, *, max_faces=16384, max_segments=32768, cancelled=None):
    """Node the entire finite arrangement; no coordinate snapping or partial output."""
    if cancelled is not None and cancelled():
        raise ValueError("contact arrangement cancelled")
    if len(faces) * 3 + len(model.sampler.mesh.triangles) * 3 > max_segments:
        raise ValueError("contact linework exceeds segment budget")
    original = shapely.polygons(xy[faces])
    if not np.isfinite(xy).all() or not shapely.coverage_is_valid(original):
        raise ValueError("finite conforming input grid required")
    caps = model.sampler._polygons
    domain = shapely.coverage_union_all(original)
    if not shapely.covers(domain, shapely.coverage_union_all(caps)):
        raise ValueError("the finite grid must contain the complete selected body")
    lines = np.concatenate((shapely.boundary(original), shapely.boundary(caps)))
    network = shapely.union_all(lines)
    if cancelled is not None and cancelled():
        raise ValueError("contact arrangement cancelled after noding")
    cells = shapely.get_parts(shapely.polygonize(shapely.get_parts(network)))
    if len(cells) > max_faces:
        raise ValueError("contact arrangement cell budget exhausted")
    polygons = shapely.get_parts(shapely.constrained_delaunay_triangles(cells))
    if len(polygons) > max_faces:
        raise ValueError("contact arrangement triangle budget exhausted")
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("contact arrangement has nonconforming output coverage")
    if not shapely.coverage_union_all(polygons).equals(domain):
        raise ValueError("contact arrangement does not preserve the complete domain")
    coordinates = shapely.get_coordinates(polygons).reshape(-1, 4, 2)[:, :3]
    a, b = coordinates[:, 1] - coordinates[:, 0], coordinates[:, 2] - coordinates[:, 0]
    cross = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    if np.any(cross == 0):
        raise ValueError("contact arrangement emitted zero-area triangles")
    coordinates[cross < 0] = coordinates[cross < 0, ::-1]
    points, inverse = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    if cancelled is not None and cancelled():
        raise ValueError("contact arrangement cancelled before height evaluation")
    return (
        points,
        inverse.reshape(-1, 3),
        {
            "input_triangles": len(faces),
            "cap_triangles": len(caps),
            "input_line_segments": len(lines) * 3,
            "noded_line_parts": len(shapely.get_parts(network)),
            "arrangement_cells": len(cells),
            "output_triangles": len(polygons),
            "output_vertices": len(points),
            "coordinate_gather_bytes": coordinates.nbytes,
            "scope": "GEOS node visits, internal copies and allocations are unmeasured; no exact-rational geometry guarantee.",
        },
    )


def conform(model, xy, faces, height_at, **options):
    points, indices, work = arrange(model, xy, faces, **options)
    z = np.asarray(height_at(points), dtype=float)
    if z.shape != (len(points),) or not np.isfinite(z).all():
        raise ValueError("contact height callback returned unresolved heights")
    return (
        np.column_stack((points, z)),
        indices,
        dict(work, output_numeric_bytes=points.nbytes + z.nbytes + indices.nbytes),
    )


def local_conform(model, xy, faces, height_at, **options):
    """Arrange only grid triangles whose XY boxes meet the complete cap box."""
    if options.get("cancelled") is not None and options["cancelled"]():
        raise ValueError("local contact cancelled")
    if len(faces) > options.get("max_faces", 16384):
        raise ValueError("local contact input exceeds triangle budget")
    cap_xy = model.sampler.mesh.vertices[:, :2]
    cap_low, cap_high = cap_xy.min(axis=0), cap_xy.max(axis=0)
    coordinates = xy[faces]
    selected = np.all(coordinates.max(axis=1) >= cap_low, axis=1) & np.all(
        coordinates.min(axis=1) <= cap_high, axis=1
    )
    points, part_faces, work = arrange(model, xy, faces[selected], **options)
    coordinates = np.concatenate((coordinates[~selected], points[part_faces]))
    if len(coordinates) > options.get("max_faces", 16384):
        raise ValueError("complete local contact output exceeds triangle budget")
    polygons = shapely.polygons(coordinates)
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("local contact join is not conforming")
    original = shapely.polygons(xy[faces])
    if not shapely.coverage_is_valid(original):
        raise ValueError("finite conforming input grid required")
    if not shapely.coverage_union_all(polygons).equals(
        shapely.coverage_union_all(original)
    ):
        raise ValueError("local contact join changed the complete domain")
    points, inverse = np.unique(coordinates.reshape(-1, 2), axis=0, return_inverse=True)
    if options.get("cancelled") is not None and options["cancelled"]():
        raise ValueError("local contact cancelled before height evaluation")
    z = np.asarray(height_at(points), dtype=float)
    if z.shape != (len(points),) or not np.isfinite(z).all():
        raise ValueError("contact height callback returned unresolved heights")
    return (
        np.column_stack((points, z)),
        inverse.reshape(-1, 3),
        dict(
            work,
            arranged_input_triangles=int(selected.sum()),
            input_triangles=len(faces),
            output_triangles=len(coordinates),
            output_vertices=len(points),
            complete_coordinate_gather_bytes=coordinates.nbytes,
            output_numeric_bytes=points.nbytes + z.nbytes + inverse.nbytes,
        ),
    )


def contact_error(model, vertices, faces):
    """Compare affine heights on all common-refinement polygon vertices."""
    polygons = shapely.polygons(vertices[faces, :2])
    caps = model.sampler._polygons
    pairs = shapely.STRtree(polygons).query(caps, predicate="intersects")
    overlap = shapely.intersection(caps[pairs[0]], polygons[pairs[1]])
    positive = shapely.area(overlap) > 0
    overlap, pairs = overlap[positive], pairs[:, positive]
    points, owners = shapely.get_coordinates(overlap, return_index=True)
    cap, terrain = pairs[:, owners]
    tri = vertices[faces[terrain]]
    u, v = tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]
    normal = np.cross(u, v)
    delta = points - tri[:, 0, :2]
    actual = tri[:, 0, 2] - np.sum(normal[:, :2] * delta, axis=1) / normal[:, 2]
    source = model.sampler.mesh.vertices[model.sampler.mesh.triangles[cap, 0]]
    expected = source[:, 2] + np.sum(
        model.sampler._gradient[cap] * (points - source[:, :2]), axis=1
    )
    error = actual - expected
    return {
        "intersecting_pairs": pairs.shape[1],
        "common_refinement_vertices": len(points),
        "maximum_above_underside_m": max(0.0, float(error.max())),
        "maximum_gap_below_underside_m": max(0.0, float(-error.min())),
        "compared_area_m2": float(shapely.area(overlap).sum()),
        "cap_area_m2": float(shapely.area(caps).sum()),
        "scope": "All vertices of positive-area GEOS common-refinement polygons; affine extrema occur there. Binary64/GEOS result, not exact-rational or native Float32 proof.",
    }
