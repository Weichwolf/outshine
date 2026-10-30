"""Existing GEOS original coverage and signed single-plane error components."""

import numpy as np
import shapely
from compare_building_sample_reuse import aligned_blocks
from field_plane_integrals import project_coordinates
from field_scale_projection import project
from field_view_plane import FieldPlane
from triangle_coverage_reference import front_source, integrate


def geometry_controls(source, grid, view, plane):
    positions = source["positions"].astype(float)
    right, up = grid.axes()
    d = np.asarray(grid.toward_eye)
    local = positions - grid.centre
    coordinates = (
        np.column_stack((local @ right, local @ up, local @ d)) / grid.half_extent_m
    )
    if np.any(abs(coordinates[:, :2]) > 1) or np.any(abs(coordinates[:, 2]) >= 2):
        raise ValueError(
            "Whole original geometry must fit actual capture XY/clip bounds"
        )
    camera = (
        np.asarray(view.eye),
        np.asarray(view.basis),
        view.focal_px,
        view.width,
        view.height,
    )
    box = np.array([[0, 0], [view.width, view.height]])
    selected, query_bytes = front_source(source, np.asarray(view.eye))
    original, _, original_work = integrate(selected, camera, box)
    captured, capture_bytes = front_source(source, toward_eye=d)
    pictures = {"original_geos_alpha": original}
    work = {"original": original_work}
    silhouettes = {}
    for name, support in (("flat", FieldPlane()), ("fitted", plane)):
        projected_source = {
            "positions": support.points(grid, coordinates[:, :2]),
            "indices": captured["indices"],
        }
        xy = project(projected_source["positions"], camera)
        if np.any(xy.min(0) < 0) or np.any(xy.max(0) > [view.width, view.height]):
            raise ValueError(
                "Original capture support would be lost outside native crop"
            )
        area, shape, cost = integrate(projected_source, camera, box)
        pictures[name + "_continuous_capture_alpha"] = area
        silhouettes[name] = shape
        work[name] = cost
    work.update(
        query_front_triangles=len(selected["indices"]),
        capture_front_triangles=len(captured["indices"]),
        front_selection_triangle_bytes=query_bytes + capture_bytes,
    )
    return pictures, silhouettes, work


def capture_points(silhouette, grid, view, native_mask):
    size = len(native_mask)
    ideal = np.zeros_like(native_mask)
    witnesses = []
    for begin in range(0, size, 16):
        y, x = np.mgrid[begin : min(begin + 16, size), :size]
        coordinates = np.column_stack((x.ravel() + 0.5, y.ravel() + 0.5))
        xy = project_coordinates(coordinates, size, grid, view, FieldPlane())
        points = shapely.covers(silhouette, shapely.points(xy)).reshape(x.shape)
        ideal[begin : begin + len(points)] = points
        for row, column in np.argwhere(
            points != native_mask[begin : begin + len(points)]
        ):
            witnesses.append(
                [int(column), int(begin + row), bool(native_mask[begin + row, column])]
            )
    return ideal, {
        "size": size,
        "native_hits": int(native_mask.sum()),
        "geos_hits": int(ideal.sum()),
        "mask_mismatches": len(witnesses),
        "witnesses_x_y_native": witnesses,
        "scope": "All source centres tested against existing Double original-capture silhouette; all boundary/backend discrepancies retained. No exact native predicate certificate.",
    }


def signed_components(
    original_native, actual_prefix, float64_prefix, ideal_points, geometry, grid, view
):
    ideal_field = np.zeros((*ideal_points.shape, 4))
    ideal_field[:, :, 3] = ideal_points
    direct = aligned_blocks(ideal_field, grid, view)
    if direct is None:
        raise ValueError(
            "This five-stage opponent requires disjoint aligned source means"
        )
    original = geometry["original_geos_alpha"]
    continuous = geometry["flat_continuous_capture_alpha"]
    reference = original_native[:, :, 3].astype(float)
    stages = {
        "pixel_reconstruction": actual_prefix[:, :, 3].astype(float)
        - float64_prefix[:, :, 3],
        "capture_predicate": float64_prefix[:, :, 3] - direct[0][:, :, 3],
        "point_discretization": direct[0][:, :, 3] - continuous,
        "flat_projection": continuous - original,
        "geos_native_reference": original - reference,
    }
    combined = actual_prefix[:, :, 3].astype(float) - reference
    energy = float(reference.sum())
    identity = float(abs(sum(stages.values()) - combined).max())
    if identity > 1e-12:
        raise ValueError(
            "Signed per-pixel coverage components do not reconstruct native error"
        )
    report = {
        "component_l1_over_native_original": {
            name: float(abs(value).sum() / energy) for name, value in stages.items()
        },
        "native_combined_l1": float(abs(combined).sum() / energy),
        "signed_identity_maximum_error": identity,
        "fitted_continuous_capture_vs_geos_original_l1": float(
            abs(geometry["fitted_continuous_capture_alpha"] - original).sum() / energy
        ),
        "scope": "Five signed component images telescope to actual native prefix−originalSS64. Absolute L1 norms do not add; cancellation remains visible. GEOS/ideal-camera versus native gap includes finite samples, camera jitter and backend/numerical differences; not certified SS64 convergence or a global error bound.",
    }
    return report, stages | {"native_combined": combined}
