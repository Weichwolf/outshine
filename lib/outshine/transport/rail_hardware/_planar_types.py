"""Owned metric rail products on one caller-selected affine bed plane."""

from dataclasses import dataclass

import numpy as np
import shapely

from ...contracts import Mesh
from ._types import RailHardware


@dataclass(frozen=True, slots=True, eq=False)
class PlanarRailHardware:
    """Rigidly transformed profiles/instances; finite contact admission is external.

    plane_frame columns are two metric bed axes and its upward unit normal.
    sleeper_bottoms contains all four actual lower corners per instance.
    horizontal retains the immutable local construction for independent replay.
    """

    rails: Mesh
    sleeper_transforms: np.ndarray
    footprints: tuple[shapely.Polygon, ...]
    sleeper_bottoms: np.ndarray
    plane_origin: np.ndarray
    plane_frame: np.ndarray
    horizontal: RailHardware

    def __post_init__(self):
        if not isinstance(self.rails, Mesh) or not isinstance(
            self.horizontal, RailHardware
        ):
            raise TypeError("owned rail mesh and horizontal construction required")
        origin = np.array(self.plane_origin, dtype=float, copy=True)
        frame = np.array(self.plane_frame, dtype=float, copy=True)
        transforms = np.array(self.sleeper_transforms, dtype=float, copy=True)
        bottoms = np.array(self.sleeper_bottoms, dtype=float, copy=True)
        if (
            origin.shape != (3,)
            or frame.shape != (3, 3)
            or transforms.ndim != 3
            or transforms.shape[1:] != (3, 4)
            or len(transforms) > 262144
            or bottoms.shape != (len(transforms), 4, 3)
            or not all(
                np.isfinite(a).all() for a in [origin, frame, transforms, bottoms]
            )
        ):
            raise ValueError(
                "finite bounded planar rail frame/instances/corners required"
            )
        if (
            np.max(abs(frame.T @ frame - np.eye(3))) > 1e-12
            or np.linalg.det(frame) <= 0
            or frame[2, 2] <= 0
            or np.any(np.linalg.det(transforms[:, :, :3]) <= 1e-14)
        ):
            raise ValueError("upward right-handed metric rail frame required")
        shapes = tuple(self.footprints)
        if (
            len(shapes) != len(transforms) + 2
            or len(transforms) != len(self.horizontal.sleeper_transforms)
            or not np.all(shapely.is_valid(shapes))
            or np.any(shapely.is_empty(shapes))
            or np.any(shapely.get_type_id(shapes) != 3)
            or np.any(shapely.has_z(shapes))
        ):
            raise ValueError(
                "complete finite projected rail/sleeper footprints required"
            )
        for name, value in [
            ("plane_origin", origin),
            ("plane_frame", frame),
            ("sleeper_transforms", transforms),
            ("sleeper_bottoms", bottoms),
        ]:
            value.setflags(write=False)
            object.__setattr__(self, name, value)
        object.__setattr__(self, "footprints", shapes)
