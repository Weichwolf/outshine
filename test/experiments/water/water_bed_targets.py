"""Procedural bed-height targets from an existing geographic water observation.

These point targets retain the water query's mask, source membership and pending
states. They are not a triangulated bed, terrain modification or observed depth.
"""

import math
from dataclasses import dataclass
from numbers import Real

import numpy as np
from water_geographic_query import GeographicSamples


@dataclass(frozen=True)
class BedTargets:
    water: GeographicSamples
    height_m: np.ndarray
    height_bounds_m: np.ndarray
    report: dict


def bed_targets(water, *, depth_m):
    """Apply one explicit nominal depth to the same geographic query locations.

    Bounds cover arithmetic in the stored model, not bathymetric uncertainty.
    Independent bed meshes still need full-face, shore and neighbour admission.
    No source, facet, normal, depth recipe or water height is changed here.
    """
    if (
        not isinstance(water, GeographicSamples)
        or not 1 <= len(water.states) <= 4096
        or not isinstance(depth_m, Real)
        or isinstance(depth_m, (bool, np.bool_))
    ):
        raise ValueError("bounded geographic water samples and numeric depth required")
    try:
        depth = float(depth_m)
    except OverflowError as error:
        raise ValueError("finite nonnegative procedural depth required") from error
    if not math.isfinite(depth) or depth < 0:
        raise ValueError("finite nonnegative procedural depth required")
    if not math.isfinite(math.nextafter(-depth, -math.inf)):
        raise ValueError("finite bed targets and arithmetic bounds required")
    wet = np.array(water.states) == "water"
    heights = np.full(len(wet), np.nan)
    bounds = np.full((len(wet), 2), np.nan)
    heights[wet] = water.height_m[wet] - depth
    if depth == 0:
        bounds[wet] = water.height_bounds_m[wet]
    else:
        bounds[wet, 0] = np.nextafter(water.height_bounds_m[wet, 0] - depth, -np.inf)
        bounds[wet, 1] = np.nextafter(water.height_bounds_m[wet, 1] - depth, np.inf)
    if not np.isfinite(heights[wet]).all() or not np.isfinite(bounds[wet]).all():
        raise ValueError("finite bed targets and arithmetic bounds required")
    heights.setflags(write=False)
    bounds.setflags(write=False)
    return BedTargets(
        water,
        heights,
        bounds,
        {
            "queries_reused": len(wet),
            "water_targets": int(wet.sum()),
            "additional_geometry_queries": 0,
            "output_array_bytes": heights.nbytes + bounds.nbytes,
            "nominal_depth_m": depth,
            "scope": "Procedural pointwise targets, not whole-face terrain/bed admission or observed bathymetry. Unavailable support and source identities are those of the retained water observation.",
        },
    )
