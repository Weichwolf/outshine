"""Independent facet integrals, source UV/height maps and exact-knot sections."""

import numpy as np
import shapely
from hip_crown_geometry import HEIGHT_EPSILON_M
from hip_crown_geometry import build as hip_build
from roof_components import above
from structural_shell_geometry import native
from terraced_mass_admission import closed


def integrals(product, crown):
    factor = product.hip_plan.factor(crown.top_m - crown.deck_m, "byHeight")
    knot = (crown.top_m - crown.deck_m) * crown.run_fraction
    a, b = (
        crown.height_fraction / crown.run_fraction,
        ((1 - crown.height_fraction) / (1 - crown.run_fraction)),
    )
    volume, area = 0.0, product.hip_plan.footprint.area
    for facet in product.hip_plan.facets:
        upper = above(
            {
                "polygon": facet.polygon,
                "plane": np.r_[facet.gradient, facet.intercept_m],
            },
            np.zeros(2),
            knot / factor,
        )
        centre = np.asarray(facet.polygon.centroid.coords[0])
        base_integral = (
            facet.polygon.area
            * factor
            * (np.asarray(facet.gradient) @ centre + facet.intercept_m)
        )
        hinge = 0.0
        if not upper.is_empty:
            centre = np.asarray(upper.centroid.coords[0])
            hinge = upper.area * (
                factor * (np.asarray(facet.gradient) @ centre + facet.intercept_m)
                - knot
            )
        volume += a * base_integral + (b - a) * hinge
        g = factor * np.linalg.norm(facet.gradient)
        area += (facet.polygon.area - upper.area) * np.hypot(1, a * g)
        area += upper.area * np.hypot(1, b * g)
    return float(volume), float(area), factor


def source_interpolation(product, mesh, original, crown):
    source = np.asarray(product.source_triangle_ids)
    if (
        source.shape != (len(mesh["faces"]),)
        or np.any(source < 0)
        or np.any(source >= len(original.mesh["faces"]))
    ):
        raise ValueError("Broken crown has an invalid original triangle binding")
    parent_ids = original.mesh["faces"][source]
    parent = original.mesh["vertices"][parent_ids]
    child = mesh["vertices"][mesh["faces"]]
    u, v = parent[:, 1, :2] - parent[:, 0, :2], parent[:, 2, :2] - parent[:, 0, :2]
    delta = child[:, :, :2] - parent[:, None, 0, :2]
    determinant = u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]
    wb = (
        delta[:, :, 0] * v[:, None, 1] - delta[:, :, 1] * v[:, None, 0]
    ) / determinant[:, None]
    wc = (
        u[:, None, 0] * delta[:, :, 1] - u[:, None, 1] * delta[:, :, 0]
    ) / determinant[:, None]
    weights = np.stack((1 - wb - wc, wb, wc), axis=2)
    expected_uv = np.einsum("fki,fij->fkj", weights, original.mesh["uv"][parent_ids])
    actual_uv = mesh["uv"][mesh["faces"]]
    height = np.einsum("fki,fi->fk", weights, parent[:, :, 2])
    t = (height - crown.deck_m) / (crown.top_m - crown.deck_m)
    y = np.where(
        t <= crown.run_fraction,
        t * crown.height_fraction / crown.run_fraction,
        crown.height_fraction
        + (t - crown.run_fraction)
        * (1 - crown.height_fraction)
        / (1 - crown.run_fraction),
    )
    expected_z = crown.deck_m + (crown.top_m - crown.deck_m) * y
    uv_error = float(abs(actual_uv - expected_uv).max())
    height_error = float(abs(child[:, :, 2] - expected_z).max())
    knot = crown.deck_m + (crown.top_m - crown.deck_m) * crown.run_fraction
    straddles = (height.min(axis=1) < knot - HEIGHT_EPSILON_M) & (
        height.max(axis=1) > knot + HEIGHT_EPSILON_M
    )
    if (
        uv_error > 1e-9
        or height_error > HEIGHT_EPSILON_M
        or weights.min() < -1e-7
        or np.any(straddles)
        or not np.array_equal(mesh["materials"], original.mesh["materials"][source])
    ):
        raise ValueError(
            "Broken crown disagrees with its original UV/height/material map"
        )
    return {"maximum_uv_error": uv_error, "maximum_height_map_error_m": height_error}


def check(owner):
    (product,) = owner.physical_roof()
    crown = owner.roof_crown
    if crown.kind != "broken-hip":
        raise ValueError(
            "Requested broken crown retained an explicitly unserved old roof"
        )
    if product.source_ids != (crown.identifier,):
        raise ValueError("Broken crown lost original owner/profile identity")
    closed(product)
    expected_volume, expected_area, factor = integrals(product, crown)
    vertices = np.array(product.mesh["vertices"], copy=True)
    core = owner.plan.body.core
    vertices[:, :2] = (vertices[:, :2] - core.origin_xy) @ np.array(
        [core.across_xy, core.along_xy]
    ).T
    mesh = dict(product.mesh, vertices=vertices)
    (original,), _ = hip_build(crown.base_hip)
    interpolation = source_interpolation(product, mesh, original, crown)
    ratios = (
        0.05,
        0.25,
        crown.height_fraction - 0.0001,
        crown.height_fraction,
        crown.height_fraction + 0.0001,
        0.95,
    )
    sections, source_area_errors = [], []
    rise = crown.top_m - crown.deck_m
    actual_solid = native(mesh, product.vertex_roots).scale((1000,) * 3)
    original_solid = native(original.mesh, original.vertex_roots).scale((1000,) * 3)
    for ratio in ratios:
        inverse = (
            ratio * crown.run_fraction / crown.height_fraction
            if ratio <= crown.height_fraction
            else crown.run_fraction
            + (ratio - crown.height_fraction)
            * (1 - crown.run_fraction)
            / (1 - crown.height_fraction)
        )
        threshold = rise * inverse / factor
        section_area = sum(
            above(
                {"polygon": f.polygon, "plane": np.r_[f.gradient, f.intercept_m]},
                np.zeros(2),
                threshold,
            ).area
            for f in product.hip_plan.facets
        )
        expected = original_solid.slice((crown.deck_m + rise * inverse) * 1000)
        actual = actual_solid.slice((crown.deck_m + rise * ratio) * 1000)
        sections.append(((actual - expected).area() + (expected - actual).area()) / 1e6)
        source_area_errors.append(abs(expected.area() / 1e6 - section_area))
    knot_area_error = sections[3]
    xyz = vertices[mesh["faces"]]
    normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    top = xyz[normals[:, 2] > 0]
    domain = shapely.from_wkb(crown.domain_wkb)
    polygons = shapely.polygons(top[:, :, :2])
    projection = shapely.union_all(polygons)
    footprint_error = projection.symmetric_difference(domain).area
    overlaps = abs(float(shapely.area(polygons).sum()) - projection.area)
    errors = [
        abs(expected_volume - product.volume_m3),
        abs(expected_area - product.area_m2),
        footprint_error,
        overlaps,
        knot_area_error,
        *sections,
        *source_area_errors,
    ]
    if (
        max(errors) > 1e-7
        or abs(vertices[:, 2].min() - crown.deck_m) > HEIGHT_EPSILON_M
        or abs(vertices[:, 2].max() - crown.top_m) > HEIGHT_EPSILON_M
    ):
        raise ValueError(
            f"Broken crown violates original facet/section/height checks: {errors}"
        )
    return interpolation | {
        "volume_error_m3": errors[0],
        "area_error_m2": errors[1],
        "footprint_xor_m2": footprint_error,
        "projection_overlap_m2": overlaps,
        "section_xor_m2": sections,
        "native_source_facet_area_error_m2": source_area_errors,
        "exact_knot_area_error_m2": knot_area_error,
        "courtyards": len(domain.interiors),
        "closed_triangles": len(mesh["faces"]),
        "maximum_source_height_correction_m": product.maximum_source_height_correction_m,
        "scope": "Independent original facet integrals/UV/height/material maps and sections including native exact knot; original source eave/courtyards/top. No native World/radiance/device acceptance.",
    }
