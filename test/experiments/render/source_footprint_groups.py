"""Original BSC3 floor-footprint measures; not opaque geometry or World readiness."""

import numpy as np
import shapely
from column_roof_patches import bottom_m, project_patch, tangent_basis
from urban_parent_areas import region_areas


def read_footprints(parent, cells):
    """Read prepared forms without interpreting native view/triangle packages."""
    basis = tangent_basis(parent.plans)
    geometries, identities, supported, needs = [], [], [], []
    shapes, kinds = 0, {}
    for _, ordinals, surfaces in parent.iter_forms(cells):
        for identity, surface in zip(ordinals, surfaces, strict=True):
            identities.append(int(identity))
            supported.append(False)
            parts = []
            try:
                if surface is None or not surface["shapes"]:
                    raise ValueError("missing original footprint forms")
                for shape in surface["shapes"]:
                    shapes += 1
                    kind = str(shape["roof_kind"])
                    kinds[kind] = kinds.get(kind, 0) + 1
                    polygon = shapely.Polygon(shape["ring"], shape["holes"])
                    if not polygon.is_valid or polygon.area <= 0:
                        raise ValueError("invalid original footprint")
                    floor = bottom_m(surface, shape)
                    footprint, _, _, _ = project_patch(
                        surface, polygon, np.array([0.0, 0.0, floor]), basis, floor
                    )
                    parts.append(footprint)
                geometry = shapely.union_all(parts)
                if not geometry.is_valid or geometry.area <= 0:
                    raise ValueError("invalid combined source footprint")
            except (
                ValueError,
                np.linalg.LinAlgError,
                shapely.errors.GEOSException,
            ) as error:
                needs.append({"source": int(identity), "reason": str(error)})
                continue
            geometries.append(geometry)
            supported[-1] = True
    return (
        geometries,
        {
            "source_ids": np.asarray(identities, np.uint32),
            "source_supported": np.asarray(supported, bool),
            "basis": basis,
        },
        {"shapes": shapes, "roof_kinds": kinds, "needs": needs},
    )


def union_inventory(geometries):
    """Geometric union only; equal anonymous external IDs never merge sources."""
    if not geometries:
        raise ValueError(
            "known source footprints required; missing input is not empty World"
        )
    union = shapely.union_all(geometries)
    components = shapely.get_parts(union)
    tree = shapely.STRtree(geometries)
    membership = tree.query(components, predicate="intersects")
    shared_area = shapely.area(
        shapely.intersection(
            components[membership[0]], np.asarray(geometries, object)[membership[1]]
        )
    )
    membership = membership[:, shared_area > 0]
    sizes = np.bincount(membership[0], minlength=len(components))
    total = float(shapely.area(geometries).sum())
    work = {
        "sources": len(geometries),
        "source_area_sum_m2": total,
        "union_area_m2": float(union.area),
        "overlap_fraction_of_sum": 1 - union.area / total,
        "source_coordinates": int(shapely.get_num_coordinates(geometries).sum()),
        "union_coordinates": int(shapely.get_num_coordinates(union)),
        "union_components": len(components),
        "union_holes": int(shapely.get_num_interior_rings(components).sum()),
        "largest_component_sources": int(sizes.max()),
    }
    if len(geometries) > 1:
        indices, distances = tree.query_nearest(
            geometries, exclusive=True, all_matches=False, return_distance=True
        )
        nearest = np.full(len(geometries), np.inf)
        nearest[indices[0]] = distances
        touching = tree.query(geometries, predicate="intersects")
        others = touching[0] != touching[1]
        nearest[touching[0, others]] = 0
        work["nearest_source_gap_quantiles_m"] = np.quantile(
            nearest, [0, 0.25, 0.5, 0.75, 0.9, 1]
        ).tolist()
    return union, work


def coverage_groups(geometry, pitch_m):
    """Clipped plan-area cell averages; no height, shading or ray opacity."""
    if not np.isfinite(pitch_m) or pitch_m <= 0 or geometry.is_empty:
        raise ValueError(
            "nonempty known footprint and finite positive group pitch required"
        )
    bounds = np.array(geometry.bounds).reshape(2, 2)
    if not np.isfinite(bounds).all() or np.max(np.abs(bounds / pitch_m)) >= 2**50:
        raise ValueError("finite representable group coordinates required")
    low = np.floor(bounds[0] / pitch_m).astype(np.int64)
    high = np.ceil(bounds[1] / pitch_m).astype(np.int64)
    shape = high - low
    if np.prod(shape) > 1048576 or np.any(shape <= 0):
        raise ValueError("bounded source-footprint group grid required")
    x = np.arange(low[0], high[0] + 1) * pitch_m
    y = np.arange(low[1], high[1] + 1) * pitch_m
    areas, work = region_areas(geometry, x, y, "direct")
    area = pitch_m**2
    fraction = areas / area
    # For a binary footprint, a cell constant q has L2 integral A*q*(1-q).
    # The minimizer is q=covered_area/A, not an opaque any-touch cell.
    report = {
        "group_cells": int(fraction.size),
        "nonempty_groups": int(np.count_nonzero(areas)),
        "area_intersections": work["intersection_tests"],
        "intersection_input_coordinates": work["intersection_input_coordinates"],
        "integrated_coverage_m2": float(areas.sum()),
        "opaque_any_touch_area_m2": float(np.count_nonzero(areas) * area),
        "cell_average_plan_l2_integral_m2": float((areas * (1 - fraction)).sum()),
        "opaque_any_touch_plan_l2_integral_m2": float(
            np.count_nonzero(areas) * area - areas.sum()
        ),
    }
    return {
        "fractions": fraction,
        "low_xy_m": low.astype(float) * pitch_m,
        "pitch_m": np.array(pitch_m),
    }, report
