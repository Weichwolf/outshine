"""Explicit, versioned construction choices; work admission is not identity."""

from collections import Counter

from ...contracts import immutable

THREE_ARM_RECIPE = "outshine/coarse-three-arm-ground/2"
THREE_ARM_PARAMETERS = immutable(
    {
        "version": THREE_ARM_RECIPE,
        "topology": "whole-three-span-four-node-single-degree3-ground-star",
        "cut_method": "highs-linear-minimum-sum",
        "cut_and_remaining_arm_reserve_m": 0.05,
        "minimum_cut_half_width_fraction": 0.25,
        "cut_residual_limit_m": 1e-10,
        "original_hub_profiles": "full-width-in-cut-lower-bounds-and-convex-hull",
        "arm_length_domain_m": (0.1, 512.0),
        "width_domain_m": (0.0, 30.0),
        "maximum_source_gradient": 1.0,
        "proximal_station_trigger_m": 20.0,
        "proximal_station_width_factors": (0.8, 1.2),
        "maximum_terminal_band_m": 2.0,
        "terminal_band_remaining_arm_fraction": 0.2,
        "arm_cross_gradient": 0.025,
        "core_gradient": "isotropic-min(0.025,minimum-arm-gradient)",
        "triangulation": "GEOS-constrained-Delaunay-per-exact-shared-polygon",
        "frame": "original-source-junction-WGS84-ENU",
        "observations": "actual-bound-DEM-at-all-original-generated-vertices",
    }
)


def known_geometry_recipe(value):
    return value is None or (isinstance(value, str) and value == THREE_ARM_RECIPE)


def source_star(index, selected):
    """Topology/profile guard only; common original-incidence guards still follow."""
    _, spans, _ = index
    degree = Counter(n for edge in selected for n in spans[edge].nodes)
    centres = [n for n, count in degree.items() if count == 3]
    if len(selected) != 3 or len(degree) != 4 or len(centres) != 1:
        return None, "three_arm_ground_topology_required"
    for edge in selected:
        span = spans[edge]
        if span.bridge or span.tunnel or span.layer != 0:
            return None, "three_arm_ground_profile_required"
        if (
            span.maximum_gradient is None
            or not 0
            < span.maximum_gradient
            <= THREE_ARM_PARAMETERS["maximum_source_gradient"]
            or not THREE_ARM_PARAMETERS["width_domain_m"][0]
            < span.width_m
            <= THREE_ARM_PARAMETERS["width_domain_m"][1]
        ):
            return None, "missing_source_profile_recipe"
    centre = centres[0]
    leaves = tuple(next(n for n in spans[e].nodes if n != centre) for e in selected)
    return (centre, leaves), ""
