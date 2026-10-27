"""Short planar end surface; shared original cap, curved constant-width axis."""

import math

import numpy as np
import shapely
from outshine_ref.contracts import Mesh, immutable
from outshine_ref.transport.canonical_ports._geodesy import frame, positions_in_frame
from outshine_ref.transport.canonical_ports._ground import strip_frame
from outshine_ref.transport.port_heights._world import normal_gradients

RECIPE = immutable(
    {
        "version": "outshine/source-bound-bridge-end-surface/1",
        "maximum_length_m": 8.0,
        "first_span_fraction": 0.25,
        "straight_lead_m": 0.25,
        "curve_segments": 8,
        "maximum_entry_turn_deg": 30.0,
    }
)


def end_axis(near_xy, direction, length):
    centre = near_xy.mean(axis=0)
    left = near_xy[1] - near_xy[0]
    left /= np.linalg.norm(left)
    start = np.array([left[1], -left[0]])
    if start @ direction < math.cos(math.radians(RECIPE["maximum_entry_turn_deg"])):
        raise ValueError("bridge_entry_direction_recipe")
    lead = min(RECIPE["straight_lead_m"], length / 8)
    end = centre + length * direction
    p0, p3 = centre + lead * start, end - lead * direction
    control = (length - 2 * lead) / 3
    p1, p2 = p0 + control * start, p3 - control * direction
    t = np.linspace(0, 1, RECIPE["curve_segments"] + 1)[:, None]
    curve = (1 - t) ** 3 * p0 + 3 * (1 - t) ** 2 * t * p1
    curve += 3 * (1 - t) * t**2 * p2 + t**3 * p3
    return np.vstack((centre, curve, end))


def strip_faces(rows):
    first = 2 * np.arange(rows - 1)
    return np.vstack(
        (np.c_[first, first + 2, first + 1], np.c_[first + 2, first + 3, first + 1])
    )


def end_geometry(anchor, port_anchor, near, gradient, intercept, other, width, budget):
    origin, axes = frame(anchor)
    target, target_axes = frame(port_anchor)
    rotation = target_axes.T @ axes
    translation = target_axes.T @ (origin - target)
    cap = near @ rotation.T + translation
    direction = positions_in_frame(port_anchor, np.array([other]), np.zeros(1))[0, :2]
    span_length = float(np.linalg.norm(direction))
    if not math.isfinite(span_length) or span_length <= 1e-6:
        raise ValueError("bridge_end_collapsed_source_span")
    direction /= span_length
    length = min(
        RECIPE["maximum_length_m"], RECIPE["first_span_fraction"] * span_length
    )
    if not 0.25 <= length or len(near) != 2:
        raise ValueError("bridge_end_extent_or_cap_recipe")
    left = np.array([-direction[1], direction[0]])
    order = np.argsort(cap[:, :2] @ left)
    cap, near = cap[order], near[order]
    axis = end_axis(cap[:, :2], direction, length)
    vertices, faces = 2 * len(axis), 2 * (len(axis) - 1)
    if (
        vertices > budget.surface.max_output_vertices
        or faces > budget.surface.max_output_faces
    ):
        raise ValueError("bridge_end_output_budget")
    _, tangents, miters = strip_frame(axis, width)
    xy = (
        axis[:, None]
        + np.array([-width / 2, width / 2])[None, :, None] * miters[:, None]
    )
    if float(np.linalg.norm(xy[0] - cap[:, :2], axis=1).max()) > 1e-7:
        raise ValueError("bridge_end_cap_width")
    xy[0] = cap[:, :2]
    normal = rotation @ np.r_[-gradient, 1.0]
    if not np.isfinite(normal).all() or normal[2] <= 0.25:
        raise ValueError("bridge_end_plane_frame_domain")
    constant = intercept + normal @ translation
    port_gradient, port_intercept = -normal[:2] / normal[2], constant / normal[2]
    port_vertices = np.c_[
        xy.reshape(-1, 2), port_intercept + xy.reshape(-1, 2) @ port_gradient
    ]
    source_vertices = (port_vertices - translation) @ rotation
    source_vertices[:2] = near
    triangles = strip_faces(len(axis))
    mesh = Mesh(source_vertices, triangles)
    polygon = shapely.Polygon(np.vstack((xy[:, 0], xy[::-1, 1])))
    if not polygon.is_valid or polygon.area <= 0:
        raise ValueError("bridge_end_folded_ribbon")
    corners = xy.reshape(-1, 2)[triangles]
    delta_a, delta_b = corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]
    areas = delta_a[:, 0] * delta_b[:, 1] - delta_a[:, 1] * delta_b[:, 0]
    polygons = shapely.polygons(corners)
    if (areas <= 0).any() or not shapely.coverage_is_valid(polygons):
        raise ValueError("bridge_end_finite_coverage")
    coverage_error = float(
        shapely.symmetric_difference(shapely.coverage_union_all(polygons), polygon).area
    )
    if coverage_error > 1e-9:
        raise ValueError("bridge_end_missing_surface")
    normals = np.c_[-tangents[:, 1], tangents[:, 0]]
    row_widths = xy[:, 1] - xy[:, 0]
    widths = np.einsum("ni,ni->n", (row_widths[:-1] + row_widths[1:]) / 2, normals)
    if float(abs(widths - width).max()) > 1e-7:
        raise ValueError("bridge_end_narrowed_profile")
    finite = normal_gradients(source_vertices[:, :2], triangles, source_vertices[:, 2])
    grade_error = float(np.linalg.norm(finite - gradient, axis=1).max())
    if grade_error > 1e-7:
        raise ValueError("bridge_end_plane_discontinuity")
    return mesh, {
        "recipe": RECIPE,
        "near_order": order.tolist(),
        "axis_port_enu": axis,
        "port_vertices": port_vertices,
        "port_gradient": port_gradient,
        "port_intercept": port_intercept,
        "first_span_length_m": span_length,
        "nominal_end_length_m": length,
        "maximum_axis_departure_m": float(abs((axis - axis[0]) @ left).max()),
        "minimum_width_m": float(widths.min()),
        "maximum_width_m": float(widths.max()),
        "maximum_finite_plane_gradient_error": grade_error,
        "coverage_error_m2": coverage_error,
        "longitudinal_grade_by_axis_segment": tangents @ port_gradient,
        "cross_grade_by_axis_segment": normals @ port_gradient,
        "grade_scope": "Whole planar segments in this explicit port-ENU chart; no whole-Bridge physical-frame/clearance admission",
        "vertices": vertices,
        "faces": faces,
        "known_output_payload_bytes": source_vertices.nbytes + triangles.nbytes,
        "source_cap_copied_payload_bytes": near.nbytes,
        "scope": "Metric top surface in the unchanged Ground source frame; whole shared original cap and affine terminal plane. No body thickness, terrain/crossing clearance, qualified contact, whole Bridge or World admission.",
    }
