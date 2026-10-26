"""Library adapter, archived-product record and transform-only cost control."""

from dataclasses import dataclass

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.transport.rail_hardware import (
    RailRecipe,
)
from outshine_ref.transport.rail_hardware import (
    build_planar_rail_hardware as build_tilted,
)
from outshine_ref.transport.rail_hardware import (
    rail_bed_frame as plane_frame,
)

__all__ = ["TiltedRail", "build_tilted", "plane_frame", "transform_points"]

DEFAULT_RECIPE = RailRecipe()


@dataclass(frozen=True)
class TiltedRail:
    """Actual archived export for independent replay, without reconstruction."""

    rails: Mesh
    sleeper_transforms: np.ndarray
    footprints: tuple
    sleeper_bottoms: tuple
    plane_origin: np.ndarray
    plane_frame: np.ndarray
    horizontal: object


def transform_points(points, origin, frame, *, method="batch"):
    if method == "batch":
        return points @ frame.T + origin
    if method == "scalar":
        return np.array([frame @ point + origin for point in points])
    raise ValueError("known equivalent rigid transform required")
