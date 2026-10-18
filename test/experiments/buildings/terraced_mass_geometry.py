"""Closed exterior mass union, independent of detailed rooms or presentation LOD."""

from dataclasses import replace

import numpy as np
import shapely
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from outshine_ref.buildings.rooflight_panels import triangle_surface


def bands(plan, *, original=False):
    if original:
        return (
            PrismVolume(
                plan.identifier + "/source",
                shapely.from_wkb(plan.source_wkb),
                plan.bottom_m,
                plan.top_m,
                12,
            ),
        )
    values = []
    for storey in plan.storeys:
        polygon = shapely.from_wkb(storey.footprint_wkb)
        if values and polygon.wkb == values[-1].polygon.wkb:
            last = values.pop()
            values.append(
                PrismVolume(last.identifier, polygon, last.low_m, storey.top_m, 12)
            )
        else:
            values.append(
                PrismVolume(
                    storey.identifier, polygon, storey.bottom_m, storey.top_m, 12
                )
            )
    return tuple(values)


def physical(plan, *, original=False):
    sources = bands(plan, original=original)
    try:
        return union_prisms(sources, max_triangles=32768)
    except ValueError as error:
        if str(error) != "Prism source rejected: Error.NotManifold":
            raise
    resolved = tuple(
        replace(
            s, polygon=shapely.set_precision(shapely.set_precision(s.polygon, 1e-10), 0)
        )
        for s in sources
    )
    changed = sum(
        a.polygon.symmetric_difference(b.polygon).area * (a.high_m - a.low_m)
        for a, b in zip(sources, resolved, strict=True)
    )
    if changed > 1e-7:
        raise ValueError("Mass contact resolution exceeds1e-7 cubic metres")
    return union_prisms(resolved, max_triangles=32768)


def presentation(product):
    mesh = triangle_surface(product.mesh, 12)
    top = mesh["normals"][mesh["faces"][:, 0], 2] > 0.5
    mesh["materials"][top] = 9
    mesh["materials"][mesh["normals"][mesh["faces"][:, 0], 2] < -0.5] = 0
    if not all(np.isfinite(v).all() for v in mesh.values()):
        raise ValueError("Mass presentation produced nonfinite attributes")
    return mesh
