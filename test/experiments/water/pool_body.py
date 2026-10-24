"""Declared ground-pool shell and separate excavation/rim-grade contact surface."""

import numpy as np
import shapely


def positive_faces(points, faces):
    faces = np.array(faces, dtype=np.int64, copy=True)
    a, b = (
        points[faces[:, 1]] - points[faces[:, 0]],
        points[faces[:, 2]] - points[faces[:, 0]],
    )
    area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    if np.any(area == 0):
        raise ValueError("pool plan has degenerate triangles")
    faces[area < 0] = faces[area < 0, ::-1]
    return faces


def ring_indices(polygon, lookup):
    coordinates = np.array(polygon.exterior.coords[:-1])
    a, b = coordinates, np.roll(coordinates, -1, axis=0)
    if np.sum(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]) < 0:
        coordinates = coordinates[::-1]
    return np.array([lookup[tuple(p)] for p in coordinates], dtype=np.int64)


def wall_faces(ring, low, high, *, inward=False):
    a, b = ring, np.roll(ring, -1)
    faces = np.vstack(
        (
            np.column_stack((low + a, low + b, high + b)),
            np.column_stack((low + a, high + b, high + a)),
        )
    )
    return faces[:, ::-1] if inward else faces


def _layout(water, width):
    normal = np.cross(water[:, 1] - water[:, 0], water[:, 2] - water[:, 0])
    if np.any(normal[:, 2] <= 0):
        raise ValueError("positive original water facets required")
    polygons = shapely.polygons(water[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("conforming nonoverlapping original water footprint required")
    domain = shapely.coverage_union_all(polygons)
    if domain.geom_type != "Polygon" or len(domain.interiors):
        raise ValueError(
            "islands, holes and multiple components require another declared recipe"
        )
    outer = shapely.buffer(domain, width, join_style="mitre")
    if (
        outer.geom_type != "Polygon"
        or len(outer.interiors)
        or not outer.covers(domain)
        or outer.area <= domain.area
        or not outer.boundary.disjoint(domain)
    ):
        raise ValueError("pool wall offset is not a simple covering component")
    band = shapely.difference(outer, domain)
    rim_polygons = shapely.get_parts(shapely.constrained_delaunay_triangles(band))
    if not shapely.coverage_is_valid(rim_polygons) or not shapely.coverage_union_all(
        rim_polygons
    ).equals(band):
        raise ValueError("pool wall-band triangulation is not conforming and complete")
    rim_xy = shapely.get_coordinates(rim_polygons).reshape(-1, 4, 2)[:, :3]
    plan, inverse = np.unique(
        np.concatenate((water[:, :, :2], rim_xy)).reshape(-1, 2),
        axis=0,
        return_inverse=True,
    )
    if len(plan) > 16384:
        raise ValueError("pool body vertex budget exceeded")
    floor = positive_faces(plan, inverse[: len(water) * 3].reshape(-1, 3))
    rim = positive_faces(plan, inverse[len(water) * 3 :].reshape(-1, 3))
    lookup = {tuple(p): i for i, p in enumerate(plan)}
    inner_ring, outer_ring = ring_indices(domain, lookup), ring_indices(outer, lookup)
    source_height = {}
    for point in water.reshape(-1, 3):
        key = tuple(point[:2])
        if key in source_height and source_height[key] != point[2]:
            raise ValueError("original water has ambiguous shared XY heights")
        source_height[key] = point[2]
    return plan, floor, rim, inner_ring, outer_ring, source_height, lookup


def build(
    water_triangles,
    reference_up,
    *,
    kind,
    ground_role,
    footprint_role,
    depth_m=2.0,
    floor_thickness_m=0.2,
    wall_width_m=0.3,
    freeboard_m=0.2,
):
    """Materialize recipe dimensions; source kind alone never declares ground contact."""
    if (
        kind != "swimming_pool"
        or ground_role != "declared_ground"
        or footprint_role != "declared_complete_local_component"
    ):
        raise ValueError(
            "explicit pool, ground and complete local-component declarations required"
        )
    if not all(
        np.isfinite(v) and 0 < v <= 10
        for v in (depth_m, floor_thickness_m, wall_width_m, freeboard_m)
    ):
        raise ValueError("finite positive bounded pool recipe dimensions required")
    water = np.array(water_triangles, dtype=float, copy=True)
    if (
        water.ndim != 3
        or water.shape[1:] != (3, 3)
        or not 0 < len(water) <= 2048
        or not np.isfinite(water).all()
        or np.any(abs(water) > 1e8)
    ):
        raise ValueError("bounded finite original water facets required")
    plan, floor, rim, inner_ring, outer_ring, source_height, lookup = _layout(
        water, wall_width_m
    )
    reference = np.array(reference_up(plan), dtype=float, copy=True)
    if (
        reference.shape != (len(plan),)
        or not np.isfinite(reference).all()
        or np.any(abs(reference) > 1e8)
    ):
        raise ValueError(
            "finite existing-height reference at new outer points required"
        )
    for point, height in source_height.items():
        reference[lookup[point]] = height
    return _assemble(
        water,
        plan,
        floor,
        rim,
        inner_ring,
        outer_ring,
        reference,
        depth_m,
        floor_thickness_m,
        freeboard_m,
    )


def _assemble(
    water,
    plan,
    floor,
    rim,
    inner_ring,
    outer_ring,
    reference,
    depth_m,
    floor_thickness_m,
    freeboard_m,
):
    bottom_z, floor_z, rim_z = (
        reference - depth_m - floor_thickness_m,
        reference - depth_m,
        reference + freeboard_m,
    )
    if not np.all(floor_z > bottom_z) or not np.all(rim_z > reference):
        raise ValueError("pool layer separation lost during materialization")
    n = len(plan)
    vertices = np.vstack(
        (
            np.column_stack((plan, bottom_z)),
            np.column_stack((plan, floor_z)),
            np.column_stack((plan, rim_z)),
        )
    )
    bottom = np.vstack((floor, rim))[:, ::-1]
    components = (
        bottom,
        floor + n,
        rim + 2 * n,
        wall_faces(inner_ring, n, 2 * n, inward=True),
        wall_faces(outer_ring, 0, 2 * n),
    )
    faces = np.vstack(components)
    parts = np.concatenate(
        [np.full(len(f), i, np.uint8) for i, f in enumerate(components)]
    )
    used, remap = np.unique(faces, return_inverse=True)
    vertices, faces = vertices[used], remap.reshape(-1, 3)
    grade_z = bottom_z.copy()
    grade_z[outer_ring] = rim_z[outer_ring]
    result = {
        "vertices": vertices,
        "faces": faces,
        "parts": parts,
        "water_triangles": water,
        "plan_xy": plan,
        "floor_plan_faces": floor,
        "rim_plan_faces": rim,
        "bottom_height_m": bottom_z,
        "rim_height_m": rim_z,
        "contact_vertices": np.column_stack((plan, grade_z)),
        "contact_faces": np.vstack((floor, rim)),
        "outer_ring": outer_ring,
        "inner_ring": inner_ring,
    }
    for value in result.values():
        value.setflags(write=False)
    return result
