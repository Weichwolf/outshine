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

NETWORK_RECIPE = "outshine/coarse-ground-tree/1"
NETWORK_PARAMETERS = immutable(
    {
        **dict(THREE_ARM_PARAMETERS),
        "version": NETWORK_RECIPE,
        "topology": "complete-source-tree-maximum8-spans-degree2-through6",
        "maximum_spans": 8,
        "maximum_degree": 6,
        "cut_method": "degree2-checked-common-otherwise-highs-linear",
        "interior_plane": "symmetric-proximal-width-bands-only-with-positive-reserve",
        "junction_anchor": "lowest-original-branch-node-identifier",
    }
)


def known_geometry_recipe(value):
    return value is None or (
        isinstance(value, str) and value in (THREE_ARM_RECIPE, NETWORK_RECIPE)
    )


def geometry_parameters(value):
    return NETWORK_PARAMETERS if value == NETWORK_RECIPE else THREE_ARM_PARAMETERS


def source_network(index, selected):
    _, spans, _ = index
    degree = Counter(n for edge in selected for n in spans[edge].nodes)
    if (
        not 3 <= len(selected) <= NETWORK_PARAMETERS["maximum_spans"]
        or len(degree) != len(selected) + 1
        or max(degree.values(), default=0) > NETWORK_PARAMETERS["maximum_degree"]
        or not any(count >= 3 for count in degree.values())
    ):
        return None, "bounded_ground_tree_topology_required"
    for edge in selected:
        span = spans[edge]
        if span.bridge or span.tunnel or span.layer != 0:
            return None, "ground_tree_profile_required"
        if (
            span.maximum_gradient is None
            or not 0 < span.maximum_gradient <= 1.0
            or not 0 < span.width_m <= 30.0
        ):
            return None, "missing_source_profile_recipe"
    branches = sorted(node for node, count in degree.items() if count >= 3)
    leaves = tuple(sorted(node for node, count in degree.items() if count == 1))
    return (branches[0], leaves), ""


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
