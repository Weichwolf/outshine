"""Remove redundant loft-plane edges before generating any tread intersections."""

import numpy as np
import shapely
from stair_band_remesh import _groups, _plane_residual, _triangulate
from stair_junction_treads import stair_recipes, unique_caps


def coarsen_envelope(plan, heights):
    faces, masks = unique_caps(plan)
    if len(faces) > 8192 or len(plan["xy"]) > 4096 or not 1 <= len(plan["rows"]) <= 64:
        raise ValueError("bounded prepared envelope required before coarsening")
    if heights.shape != (len(plan["xy"]),) or not np.isfinite(heights).all():
        raise ValueError("one finite height per envelope vertex required")
    points = np.c_[plan["xy"], heights]
    recipes, _levels = stair_recipes(plan, heights)
    by_owner = {recipe["owner"]: recipe for recipe in recipes}
    flights = {flight["owner"]: flight for flight in plan["stair_lofts"]}
    if not flights or flights.keys() != by_owner.keys():
        raise ValueError("each envelope source needs one declared straight loft")
    selected, groups = _groups(points, faces, masks, None, flights)
    protected_ids = set(np.unique(faces[~selected]).tolist())
    protected_xy = {tuple(points[v, :2]) for v in protected_ids}
    output, owners = faces[~selected].tolist(), masks[~selected].tolist()
    work = {
        "input_faces": len(faces),
        "groups": len(groups),
        "ring_visits": 0,
        "removed_ring_vertices": 0,
        "maximum_chain_vertex_deviation_m": 0.0,
        "group_area_error_m2": 0.0,
        "maximum_loft_height_residual_m": 0.0,
    }
    for (mask, _step, _region), indices in groups.items():
        owner = mask.bit_length() - 1
        vertices = np.unique(faces[indices])
        residual = _plane_residual(
            points,
            vertices,
            flights[owner],
            by_owner[owner],
            plan["stair_directions"][owner],
        )
        work["maximum_loft_height_residual_m"] = max(
            work["maximum_loft_height_residual_m"], residual
        )
        lookup = {tuple(points[v, :2]): int(v) for v in vertices}
        if len(lookup) != len(vertices):
            raise ValueError("one envelope plane cannot own ambiguous XY identities")
        shape = shapely.union_all(shapely.polygons(points[faces[indices], :2]))
        rebuilt, measured = _triangulate(shape, protected_xy, lookup)
        if not (set(vertices.tolist()) & protected_ids) <= set(
            np.array(rebuilt).ravel().tolist()
        ):
            raise ValueError("envelope coarsening lost a shared source vertex")
        if len(output) + len(rebuilt) > 8192:
            raise ValueError("coarsened envelope face budget")
        output.extend(rebuilt)
        owners.extend([mask] * len(rebuilt))
        for key, value in measured.items():
            work[key] = (
                max(work[key], value)
                if key.startswith("maximum")
                else work[key] + value
            )
    output, owners = np.array(output, dtype=int), np.array(owners, dtype=masks.dtype)
    result = dict(plan)
    result["parts"] = [
        output[(owners & (1 << i)) != 0] for i in range(len(plan["rows"]))
    ]
    result["part_global_faces"] = [
        np.flatnonzero((owners & (1 << i)) != 0) for i in range(len(plan["rows"]))
    ]
    if stair_recipes(result, heights)[0] != recipes:
        raise ValueError("envelope coarsening changed the declared landing levels")
    work["output_faces"] = len(output)
    work["selected_array_copy_bytes"] = int(
        output.nbytes
        + owners.nbytes
        + sum(a.nbytes for a in result["parts"])
        + sum(a.nbytes for a in result["part_global_faces"])
    )
    work["xyz_working_array_bytes"] = points.nbytes
    work["copy_scope"] = (
        "Selected face/ownership arrays and the XYZ working array only; result retains the original XY/height arrays. Not all GEOS/NumPy temporary allocations."
    )
    return result, work
