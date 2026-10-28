"""Bounded display-only slabs for original normal-offset road recipes.

Native Lower=S-t*N. This alternative uses Lower_display=S_display-t*Up;
the full normal departure consumes the same metric budget as Upper subdivision.
An explicit affine fallback handles straight crossfall ramps with constant cap normals.
Welded closed geometry grants no final ports, materials, physics or World authority.
"""

import numpy as np
import shapely
from road_surface_lod import limits, prepare, prepare_uniform
from road_surface_model import Curve, derivative_limits


def normal_departure(curve):
    """Bound t*|N-Up| over all original intervals and the complete road width."""
    h1, _ = derivative_limits(curve.height, curve.lengths)
    b1, _ = derivative_limits(curve.bank, curve.lengths)
    bank = np.max(abs(curve.bank), axis=1)
    q = np.tan(bank)
    q1 = b1 / np.cos(bank) ** 2
    scale = 1 - curve.width * curve.curvature_max
    if np.any(scale <= 0):
        raise ValueError("Folded full-width normal-offset recipe")
    slope = (h1 + curve.width * q1) / scale
    radius = np.hypot(slope, q)
    if not np.isfinite(radius).all():
        raise ValueError("Unbounded normal departure")
    length = np.hypot(1, radius)
    # This form avoids cancellation when the original normal is nearly vertical.
    distance = (
        curve.thickness * (radius / length) * np.sqrt(2 * (length / (length + 1)))
    )
    distance = np.where(distance > 0, np.nextafter(distance, np.inf), 0)
    return float(distance.max()), float(scale.min())


def lower_rounding_allowance(curve):
    """Reserve a whole Float32 height ULP above the possible rounded Lower."""
    z = float(np.max(abs(curve.height))) + curve.width * float(
        np.tan(np.max(abs(curve.bank)))
    )
    maximum = float(np.finfo(np.float32).max)
    if not np.isfinite(z) or z + curve.thickness >= maximum / 2:
        raise ValueError("Lower Float32 magnitude unavailable")
    top = np.nextafter(np.float32(z), np.float32(np.inf))
    lower = np.nextafter(np.float32(float(top) + curve.thickness), np.float32(np.inf))
    return float(np.spacing(lower))


def closed_indices(top, vertices):
    edges = np.concatenate([top[:, [0, 1]], top[:, [1, 2]], top[:, [2, 0]]])
    keys = np.sort(edges, axis=1)
    _, first, counts = np.unique(keys, axis=0, return_index=True, return_counts=True)
    if np.any(counts > 2):
        raise ValueError("Nonmanifold original Upper topology")
    boundary = edges[first[counts == 1]]
    a, b = boundary.T
    walls = np.stack((a, a + vertices, b, b, a + vertices, b + vertices), axis=1)
    return np.concatenate((top, top[:, [0, 2, 1]] + vertices, walls.reshape(-1, 3)))


def qualify_body(positions, faces, upper_faces, thickness):
    """Check complete stored geometry; the projected Upper must be one simple band."""
    triangles = positions[faces].astype(float)
    normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if not np.isfinite(triangles).all() or np.any(np.linalg.norm(normal, axis=1) == 0):
        raise ValueError("Degenerate closed display face")
    if np.any(normal[:upper_faces, 2] <= 0):
        raise ValueError("Folded stored Upper geometry")
    if not shapely.coverage_is_valid(shapely.polygons(triangles[:upper_faces, :, :2])):
        raise ValueError("Overlapping stored Upper geometry")
    if len(np.unique(positions, axis=0)) != len(positions):
        raise ValueError("Coincident closed display vertices")
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    keys, inverse, counts = np.unique(
        np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
    if np.any(counts != 2) or np.any(np.bincount(inverse, weights=signs) != 0):
        raise ValueError("Open or inconsistently oriented closed display body")
    if len(positions) - len(keys) + len(faces) != 2:
        raise ValueError("Closed display body topology")
    shifted = triangles - positions[0]
    volume = float(
        np.einsum(
            "ij,ij->i", shifted[:, 0], np.cross(shifted[:, 1], shifted[:, 2])
        ).sum()
        / 6
    )
    if not np.isfinite(volume) or volume <= 0:
        raise ValueError("Nonpositive closed display volume")
    area = float(normal[:upper_faces, 2].sum() / 2)
    return {
        "closed_oriented_edges": len(keys),
        "euler_characteristic": 2,
        "signed_volume_m3": volume,
        "projected_upper_area_m2": area,
        "nominal_vertical_volume_m3": thickness * area,
    }


def prepare_closed(
    recipe,
    error_m,
    *,
    max_payload_bytes,
    method="adaptive",
    max_stations=262144,
    refine_normal_bound=False,
    affine_lower=False,
    affine_width_boxes=16,
):
    if recipe["form"] != "closed_shell":
        raise ValueError("Actual closed-shell source recipe required")
    if type(max_payload_bytes) is not int or max_payload_bytes <= 0:
        raise ValueError("Explicit positive closed display payload budget required")
    if method not in ("adaptive", "uniform"):
        raise ValueError("Known same-bound Upper subdivision method required")
    if type(refine_normal_bound) is not bool:
        raise ValueError("Explicit normal-hull refinement choice required")
    if type(affine_lower) is not bool:
        raise ValueError("Explicit affine lower choice required")
    limits(error_m, max_stations)
    curve = Curve(
        {key: recipe[key] for key in ("range_m", "section_m", "seams", "intervals")}
    )
    if not np.isfinite(curve.thickness) or curve.thickness <= 0:
        raise ValueError("Positive original closed-shell thickness required")
    columns, bands = (2, 1) if curve.shoulder == 0 else (4, 3)
    bytes_per_station = 28 * columns + 8 + 52 * (bands + 1)
    capacity = min(max_stations, (max_payload_bytes + 52) // bytes_per_station)
    if capacity < 2:
        raise ValueError("Closed display payload capacity")
    departure, scale = normal_departure(curve)
    rounding = lower_rounding_allowance(curve)
    if rounding >= curve.thickness:
        raise ValueError("Positive thickness is not guaranteed after Float32 storage")
    upper_budget = float(np.nextafter(error_m - departure - rounding, -np.inf))
    original_departure = departure
    subdivided = False
    affine = None
    stretch = 1
    if upper_budget <= 0 and refine_normal_bound:
        from road_normal_hulls import refined_normal_departure

        departure = min(departure, refined_normal_departure(curve))
        upper_budget = float(np.nextafter(error_m - departure - rounding, -np.inf))
        subdivided = True
    if upper_budget <= 0 and affine_lower:
        from road_affine_lower import plan_lower

        affine = plan_lower(curve, boxes=affine_width_boxes)
        departure, rounding = affine["departure"], affine["rounding"]
        stretch = affine["stretch"]
        upper_budget = float(
            np.nextafter((error_m - departure - rounding) / stretch, -np.inf)
        )
    if upper_budget <= 0:
        raise ValueError("Normal-offset departure exhausts complete-body budget")
    build = prepare if method == "adaptive" else prepare_uniform
    upper = build(curve, upper_budget, max_stations=capacity)
    used = np.unique(upper["indices"])
    mapping = np.full(upper["positions_enh_m"].size // 3, -1, dtype=np.int64)
    mapping[used] = np.arange(len(used))
    top = upper["positions_enh_m"].reshape(-1, 3)[used]
    if affine is None:
        lower_exact = top.astype(float)
        lower_exact[:, 2] -= curve.thickness
        lower = lower_exact.astype(np.float32)
        actual_rounding = float(
            abs(lower[:, 2].astype(float) - lower_exact[:, 2]).max()
        )
    else:
        from road_affine_lower import lower_positions

        lower, actual_rounding = lower_positions(top, affine)
    if actual_rounding > rounding:
        raise ValueError("Lower rounding exceeds reserved bound")
    positions = np.vstack((top, lower))
    top_faces = mapping[upper["indices"]].astype(np.uint32)
    faces = closed_indices(top_faces, len(top))
    roles = np.r_[
        np.zeros(len(top_faces), dtype=np.uint8),
        np.ones(len(top_faces), dtype=np.uint8),
        np.full(len(faces) - 2 * len(top_faces), 2, dtype=np.uint8),
    ]
    source_vertices = used.astype(np.uint32)
    payload = sum(
        value.nbytes
        for value in (positions, faces, roles, source_vertices, upper["stations_m"])
    )
    if payload > max_payload_bytes:
        raise ValueError("Closed display payload bound exceeded")
    quality = qualify_body(positions, faces, len(top_faces), curve.thickness)
    if affine is not None:
        from road_closed_exact import qualify_intersections

        quality["exact_intersections"] = qualify_intersections(positions, faces)
        quality["nominal_vertical_volume_scope"] = (
            "Original thickness times projected Upper area; not affine body volume"
        )
    bound = stretch * upper["maximum_stored_bound_m"] + departure + rounding
    if bound > error_m:
        raise ValueError("Stored complete-body bound exceeds request")
    return {
        **{
            key: value
            for key, value in upper.items()
            if key not in ("positions_enh_m", "indices")
        },
        "recipe_version": "outshine/direct-affine-road-display/1"
        if affine is not None
        else "outshine/direct-vertical-road-display/2"
        if subdivided
        else "outshine/direct-vertical-road-display/1",
        "positions_enh_m": positions,
        "indices": faces,
        "face_roles": roles,
        "upper_source_vertex_ids": source_vertices,
        "upper_vertex_count": len(top),
        "normal_offset_departure_bound_m": departure,
        "minimum_analytic_offset_scale": scale,
        "lower_rounding_allowance_m": rounding,
        "maximum_actual_lower_rounding_m": actual_rounding,
        "upper_maximum_stored_bound_m": upper["maximum_stored_bound_m"],
        "upper_maximum_model_bound_m": upper["maximum_model_bound_m"],
        "upper_maximum_vertex_quantization_m": upper["maximum_vertex_quantization_m"],
        "maximum_model_bound_m": stretch * upper["maximum_model_bound_m"] + departure,
        "maximum_vertex_quantization_m": stretch
        * upper["maximum_vertex_quantization_m"]
        + rounding,
        "maximum_stored_bound_m": bound,
        "maximum_complete_body_bound_m": bound,
        "payload_bytes": payload,
        "quality": quality,
        **(
            {
                "original_normal_offset_departure_bound_m": original_departure,
                "normal_bound_subdivision_boxes": 2 * len(curve.lengths),
            }
            if subdivided
            else {}
        ),
        **(
            {
                "affine_lower": {
                    key: value.tolist() if isinstance(value, np.ndarray) else value
                    for key, value in affine.items()
                }
            }
            if affine is not None
            else {}
        ),
    }
