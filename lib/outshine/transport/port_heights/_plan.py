"""Owned finite products and explicit analytic patch evaluation."""

from collections.abc import Mapping
from copy import deepcopy
from dataclasses import dataclass
from math import isfinite
from types import MappingProxyType

import numpy as np

from ...contracts import Mesh
from ._arrays import immutable_mesh, owned
from ._field import PlaneField


@dataclass(frozen=True, slots=True)
class PortHeightPlan:
    """A C1 mathematical road field and its distinct conforming C0 road/soil mesh.

    All index maps refer to the original PortHeightInput order. Source metadata,
    metric frame and kind remain caller-bound. Use SurfaceSampler on ``mesh`` for
    actual finite contacts with an explicit output face budget; ``evaluate`` evaluates the chosen analytic
    road patch, including mathematical continuation outside its original domain.
    No automatic surface selection, terrain C1, dynamics or LOD claim is implied.
    """

    field: PlaneField
    mesh: Mesh
    plane_coefficients: np.ndarray
    datum_m: float
    face_parts: np.ndarray
    grade_frames: np.ndarray
    source_faces: np.ndarray
    original_vertex_map: np.ndarray
    port_edges: np.ndarray
    port_ids: np.ndarray
    height_error_bounds_m: np.ndarray
    targets_m: np.ndarray
    parameter_intervals: np.ndarray

    def __post_init__(self):
        if not isinstance(self.field, PlaneField) or not isinstance(self.mesh, Mesh):
            raise TypeError("owned port field and finite Mesh required")
        if not isfinite(self.datum_m):
            raise ValueError("finite original vertical datum required")
        object.__setattr__(self, "mesh", immutable_mesh(self.mesh))
        n, f, p = (
            len(self.mesh.vertices),
            len(self.mesh.triangles),
            len(self.port_edges),
        )
        for name, dtype, shape, integer in (
            ("plane_coefficients", float, (3 * len(self.field.origins),), False),
            ("face_parts", np.int64, (f,), True),
            ("grade_frames", float, (f, 2, 2), False),
            ("source_faces", np.int64, (f,), True),
            ("original_vertex_map", np.int64, (len(self.original_vertex_map),), True),
            ("port_edges", np.int64, (p, 2), True),
            ("port_ids", np.int64, (p,), True),
            ("height_error_bounds_m", float, (f,), False),
            ("targets_m", float, (n,), False),
        ):
            object.__setattr__(
                self,
                name,
                owned(getattr(self, name), dtype, shape, name, integer=integer),
            )
        object.__setattr__(
            self,
            "parameter_intervals",
            owned(
                self.parameter_intervals,
                float,
                (f, 2),
                "parameter intervals",
                finite=False,
            ),
        )
        if np.any(self.original_vertex_map < 0) or np.any(
            self.original_vertex_map >= n
        ):
            raise ValueError(
                "original vertex anchors must refer to the actual finite mesh"
            )
        if np.any(self.port_edges < 0) or np.any(self.port_edges >= n):
            raise ValueError("whole port edges must refer to the actual finite mesh")
        if np.any(self.face_parts < -1) or np.any(
            self.face_parts >= len(self.field.plane_ids)
        ):
            raise ValueError(
                "face modules must belong to the analytic plan or exterior soil"
            )
        if np.any(self.source_faces < 0) or np.any(self.height_error_bounds_m < 0):
            raise ValueError("source face IDs and error bounds must be nonnegative")

    def evaluate(self, xy_m, part_ids):
        """Return absolute analytic patch heights and XY gradients, not contacts."""
        height, gradient = self.field.evaluate(xy_m, part_ids, self.plane_coefficients)
        return height + self.datum_m, gradient


@dataclass(frozen=True, slots=True)
class PortHeightFit:
    """An accepted complete local plan, or an explicit reason for every input part."""

    plan: PortHeightPlan | None
    report: Mapping[str, object]

    def __post_init__(self):
        if self.plan is not None and not isinstance(self.plan, PortHeightPlan):
            raise TypeError("owned PortHeightPlan or explicit absence required")
        object.__setattr__(
            self, "report", MappingProxyType(deepcopy(dict(self.report)))
        )
