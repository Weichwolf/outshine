"""Round contours before triangulation, removing only adjacent identical words."""

import numpy as np
import shapely


def packed_domain(domain):
    if (
        domain.geom_type not in ("Polygon", "MultiPolygon")
        or domain.has_z
        or domain.is_empty
        or not domain.is_valid
        or shapely.get_num_coordinates(domain) > 16384
    ):
        raise ValueError("valid nonempty original contour required")
    polygons, removed, maximum, input_nodes, kept_nodes = [], 0, 0.0, 0, 0
    for part in shapely.get_parts(domain):
        rings = []
        for ring in (part.exterior, *part.interiors):
            original = shapely.get_coordinates(ring)[:-1]
            if len(original) > 16384 or not np.isfinite(original).all():
                raise ValueError("bounded finite contour required")
            rounded = original.astype(np.float32)
            if not np.isfinite(rounded).all():
                raise ValueError("finite Float32 contour required")
            words = rounded.view(np.uint32)
            keep = np.any(words != np.roll(words, 1, axis=0), axis=1)
            points = rounded[keep].astype(float)
            if len(points) < 3 or len(np.unique(words[keep], axis=0)) != len(points):
                raise ValueError(
                    "contour nodes collapse in Float32 or share nonadjacent words"
                )
            packed_ring = shapely.LinearRing(points)
            if not packed_ring.is_simple or packed_ring.is_ccw != ring.is_ccw:
                raise ValueError("Float32 ring changes orientation or simplicity")
            rings.append(points)
            input_nodes += len(original)
            kept_nodes += len(points)
            removed += len(original) - len(points)
            maximum = max(
                maximum, float(np.linalg.norm(rounded - original, axis=1).max())
            )
        polygons.append(shapely.Polygon(rings[0], rings[1:]))
    result = polygons[0] if len(polygons) == 1 else shapely.MultiPolygon(polygons)
    if result.is_empty or not result.is_valid:
        raise ValueError("Float32 contour changes polygon topology")
    return result, {
        "adjacent_duplicate_words_removed": removed,
        "input_ring_vertices": input_nodes,
        "retained_ring_vertices": kept_nodes,
        "known_float32_cast_array_bytes": input_nodes * 2 * 4,
        "known_kept_double_array_bytes": kept_nodes * 2 * 8,
        "maximum_xy_vertex_rounding_m": maximum,
        "rounding_added_plan_area_m2": float(shapely.difference(result, domain).area),
        "rounding_removed_plan_area_m2": float(shapely.difference(domain, result).area),
        "scope": "Exact Float32-word neighbour removal before triangulation; no tolerance, nonadjacent welding, hole/component deletion, invalid-polygon repair or surface/coverage admission.",
    }
