"""Facet-integral and independent section checks in the explicitly bound chart."""

from dataclasses import replace
from types import SimpleNamespace

import numpy as np
import shapely
from roof_components import above
from shapely.affinity import affine_transform
from structural_shell_geometry import mesh_section
from terraced_crown_probe import roof_checks
from terraced_mass_admission import closed


def expected_section(product, relative_height_m):
    plan = product.hip_plan
    pieces = [
        above(
            {"polygon": f.polygon, "plane": np.r_[f.gradient, f.intercept_m]},
            np.zeros(2),
            relative_height_m,
        )
        for f in plan.facets
    ]
    basis = np.asarray(product.chart_basis_xy)
    origin = np.asarray(product.chart_origin_xy) + np.asarray(plan.origin) @ basis
    return affine_transform(shapely.union_all(pieces), [*basis.T.ravel(), *origin])


def check(owner):
    products = owner.physical_roof()
    crown = owner.roof_crown
    if crown.kind != "hip":
        effective = SimpleNamespace(
            plan=replace(owner.plan, crown=crown), physical_roof=owner.physical_roof
        )
        return {
            "effective_kind": crown.kind,
            "failure": owner.roof_failure,
            "retained": roof_checks(effective),
        }
    product = products[0]
    if product.source_ids != (crown.identifier,):
        raise ValueError("Hip product lost its stable source-owned roof identity")
    closed(product)
    plan = product.hip_plan
    factor = plan.factor(crown.top_m - crown.deck_m, "byHeight")
    volume = sum(
        f.polygon.area
        * (
            np.asarray(f.gradient) @ np.asarray(f.polygon.centroid.coords[0])
            + f.intercept_m
        )
        * factor
        for f in plan.facets
    )
    area = plan.footprint.area + sum(
        f.polygon.area * np.sqrt(1 + (factor * np.linalg.norm(f.gradient)) ** 2)
        for f in plan.facets
    )
    core = owner.plan.body.core
    vertices = np.array(product.mesh["vertices"], copy=True)
    vertices[:, :2] = (vertices[:, :2] - core.origin_xy) @ np.array(
        [core.across_xy, core.along_xy]
    ).T
    mesh = dict(product.mesh, vertices=vertices)
    errors = []
    for t in (0.05, 0.25, 0.50, 0.75, 0.95):
        height = (crown.top_m - crown.deck_m) * t
        expected = expected_section(product, height / factor)
        actual = mesh_section(mesh, product.vertex_roots, crown.deck_m + height)
        errors.append(actual.symmetric_difference(expected).area)
    xyz = vertices[mesh["faces"]]
    cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    top = xyz[cross[:, 2] > 0]
    coverage = shapely.union_all([shapely.Polygon(p[:, :2]) for p in top])
    domain = shapely.from_wkb(crown.domain_wkb)
    footprint_error = coverage.symmetric_difference(domain).area
    overlaps = abs(float(cross[cross[:, 2] > 0, 2].sum() / 2) - coverage.area)
    if (
        max(
            abs(volume - product.volume_m3),
            abs(area - product.area_m2),
            footprint_error,
            overlaps,
            *errors,
        )
        > 1e-7
    ):
        raise ValueError(
            f"Hip crown disagrees with intended facet integrals/sections: volume={volume - product.volume_m3}, area={area - product.area_m2}, footprint={footprint_error}, overlap={overlaps}, sections={errors}"
        )
    if (
        vertices[:, 2].min() < crown.deck_m - 1e-9
        or abs(vertices[:, 2].max() - crown.top_m) > 1e-9
    ):
        raise ValueError("Actual hip source height or opaque body contact changed")
    return {
        "effective_kind": "hip",
        "source_ids": list(product.source_ids),
        "backend": plan.method,
        "volume_m3": product.volume_m3,
        "area_m2": product.area_m2,
        "volume_error_m3": abs(volume - product.volume_m3),
        "area_error_m2": abs(area - product.area_m2),
        "footprint_xor_m2": footprint_error,
        "projection_overlap_m2": overlaps,
        "section_xor_m2": errors,
        "courtyards": len(domain.interiors),
        "closed_triangles": len(mesh["faces"]),
        "scope": "Closed actual roof, analytic independent facet integrals and five sections in ENU, source eaves/courtyards and height. Positional geometry only; no radiance/controller/World/device acceptance.",
    }
