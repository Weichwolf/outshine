"""Whole original rectangles and local bevel joins on one bounded ground patch."""

import numpy as np
import shapely
from outshine_ref.transport.canonical_ports._ground_domain import mesh_parts


def layout(source, xy, budget, *, shared=True):
    if type(shared) is not bool:
        raise ValueError("cover_sharing_must_be_boolean")
    xy = np.asarray(xy, float)
    if xy.shape != (len(source.node_ids), 2) or not np.isfinite(xy).all():
        raise ValueError("cover_invalid_node_positions")
    if len(np.unique(xy, axis=0)) != len(xy):
        raise ValueError("cover_distinct_nodes_at_same_position")
    if np.max(np.ptp(xy, axis=0)) > 50:
        raise ValueError("cover_extent_budget")
    local = {n: i for i, n in enumerate(source.node_ids)}
    edges = np.array([[local[n] for n in edge] for edge in source.bands])
    ends = xy[edges]
    delta = ends[:, 1] - ends[:, 0]
    lengths = np.linalg.norm(delta, axis=1)
    if np.any(lengths < 1e-6) or np.any(lengths > 30):
        raise ValueError("cover_span_length_domain")
    tangent = delta / lengths[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    sections = (
        ends[:, :, None]
        + np.array([-1, 1])[None, None, :, None]
        * source.width_m
        / 2
        * normal[:, None, None]
    )
    band_indices = (
        np.arange(len(edges))
        if shared
        else np.array([binding[1] for binding in source.bindings])
    )
    corners = sections[band_indices].reshape(-1, 4, 2)[:, [0, 1, 3, 2]]
    rectangles = shapely.polygons(corners)
    first_member = {}
    for i, band in enumerate(band_indices):
        first_member.setdefault(int(band), i)
    unique_rectangles = rectangles[[first_member[i] for i in range(len(edges))]]
    for i, first in enumerate(edges):
        for j in range(i):
            if not set(first) & set(edges[j]) and unique_rectangles[i].intersects(
                unique_rectangles[j]
            ):
                raise ValueError("cover_nonincident_band_intersection")
    joins = []
    for node in range(len(xy)):
        at = np.argwhere(edges == node)
        if len(at) < 2:
            continue
        hull = shapely.MultiPoint(
            sections[at[:, 0], at[:, 1]].reshape(-1, 2)
        ).convex_hull
        if hull.geom_type == "Polygon":
            for edge, rectangle in zip(edges, unique_rectangles, strict=True):
                if node not in edge and hull.intersects(rectangle):
                    raise ValueError("cover_nonincident_join_intersection")
            joins.append(hull)
    domain = shapely.normalize(shapely.union_all([*rectangles, *joins]))
    if (
        domain.geom_type != "Polygon"
        or not domain.is_valid
        or domain.is_empty
        or len(domain.interiors)
    ):
        raise ValueError("cover_invalid_or_holed_patch")
    if shapely.get_num_coordinates(domain) > min(256, budget.surface.max_vertices):
        raise ValueError("cover_domain_vertex_budget")
    grade = min(0.025, source.maximum_gradient)
    raw, work = mesh_parts(
        [domain], [np.eye(2) / grade], [1], [], [], [], maximum_holes=0
    )
    if (
        len(raw["xy"]) > min(budget.terrain_samples, budget.surface.max_vertices)
        or len(raw["faces"]) > budget.surface.max_faces
        or len(raw["xy"]) > budget.surface.max_output_vertices
        or len(raw["faces"]) > budget.surface.max_output_faces
        or budget.surface.max_parts < 1
    ):
        raise ValueError("cover_mesh_or_sample_budget")
    work.update(
        source_members=len(source.members),
        physical_bands=len(edges),
        constructed_band_polygons=len(rectangles),
        constructed_band_vertices=4 * len(rectangles),
        junction_polygons=len(joins),
        boolean_input_polygons=len(rectangles) + len(joins),
        isotropic_grade_limit=grade,
    )
    return raw, domain, sections, work
