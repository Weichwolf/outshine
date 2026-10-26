"""Bounded hardware on an explicitly horizontal selected rail bed (#250)."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine_ref.contracts import Mesh


@dataclass(frozen=True, slots=True)
class RailRecipe:
    gauge_m: float = 1.435
    sleeper_length_m: float = 2.6
    sleeper_width_m: float = 0.24
    sleeper_height_m: float = 0.18
    sleeper_spacing_m: float = 0.6
    maximum_turn_deg: float = 35.0

    def __post_init__(self):
        values = np.array(
            [
                self.gauge_m,
                self.sleeper_length_m,
                self.sleeper_width_m,
                self.sleeper_height_m,
                self.sleeper_spacing_m,
                self.maximum_turn_deg,
            ]
        )
        if not np.isfinite(values).all() or np.any(values <= 0):
            raise ValueError("finite positive rail recipe dimensions required")
        if not 0.5 <= self.gauge_m <= 2.0 or not 0 < self.maximum_turn_deg <= 60:
            raise ValueError("supported explicit gauge/turn range required")
        if self.sleeper_length_m < self.gauge_m + 0.222:
            raise ValueError("sleeper must support both entire rail feet")
        if self.sleeper_spacing_m <= self.sleeper_width_m:
            raise ValueError("separate sleepers require spacing greater than width")


@dataclass(frozen=True, slots=True)
class RailBudget:
    maximum_stations: int = 4096
    maximum_sleepers: int = 8192
    maximum_rail_vertices: int = 98304

    def __post_init__(self):
        for value in (
            self.maximum_stations,
            self.maximum_sleepers,
            self.maximum_rail_vertices,
        ):
            if type(value) is not int or not 1 <= value <= 262144:
                raise ValueError("bounded positive integer rail budget required")


@dataclass(frozen=True, slots=True, eq=False)
class RailHardware:
    """Owned closed rail meshes, one shared unit sleeper and immutable instances.

    Footprints use the caller's horizontal local XY frame. They are not an
    automatic terrain/contact selection or a train-path certificate.
    """

    rails: Mesh
    sleeper: Mesh
    sleeper_transforms: np.ndarray
    footprints: tuple[shapely.Polygon, ...]

    def __post_init__(self):
        if not isinstance(self.rails, Mesh) or not isinstance(self.sleeper, Mesh):
            raise TypeError("owned reference meshes required")
        transforms = np.array(self.sleeper_transforms, dtype=float, copy=True)
        if (
            transforms.ndim != 3
            or transforms.shape[1:] != (3, 4)
            or len(transforms) > 262144
            or not np.isfinite(transforms).all()
        ):
            raise ValueError("bounded finite Nx3x4 sleeper transforms required")
        if np.any(np.linalg.det(transforms[:, :, :3]) <= 1e-14):
            raise ValueError("positive nondegenerate sleeper transforms required")
        shapes = tuple(self.footprints)
        if (
            len(shapes) != len(transforms) + 2
            or not np.all(shapely.is_valid(shapes))
            or np.any(shapely.is_empty(shapes))
            or np.any(shapely.get_type_id(shapes) != 3)
            or np.any(shapely.has_z(shapes))
        ):
            raise ValueError("complete valid rail/sleeper footprints required")
        transforms.setflags(write=False)
        object.__setattr__(self, "sleeper_transforms", transforms)
        object.__setattr__(self, "footprints", shapes)
