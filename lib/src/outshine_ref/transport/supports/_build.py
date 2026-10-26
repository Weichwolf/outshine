"""Prepared bounded support builder; no automatic highest/lowest surface selection."""

import time
from dataclasses import dataclass, field

import numpy as np
import shapely

from ...contracts import Mesh
from ._geometry import arrangement, close, heights
from ._types import SupportBudget


class _Surface:
    def __init__(self, mesh, budget):
        if (
            not isinstance(mesh, Mesh)
            or not 0 < len(mesh.triangles) <= budget.maximum_surface_faces
        ):
            raise ValueError("bounded nonempty owned support height surface required")
        p = mesh.vertices[mesh.triangles, :2]
        a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
        area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
        if not np.isfinite(area).all() or np.any(area == 0):
            raise ValueError(
                "support height surface has degenerate or vertical projected faces"
            )
        self.mesh = mesh
        self.shapes = shapely.polygons(p)
        self.shapes.setflags(write=False)
        self.tree = shapely.STRtree(self.shapes)

    def patch(self, footprint, budget):
        hits = self.tree.query(footprint, predicate="intersects")
        if not 0 < len(hits) <= budget.maximum_patch_faces:
            raise ValueError("support contact patch absent or face budget")
        shapes = self.shapes[hits]
        if not shapely.coverage_is_valid(shapes):
            raise ValueError(
                "support selected contact is not an unambiguous conforming height surface"
            )
        pieces = shapely.intersection(shapes, footprint)
        covered = shapely.union_all(pieces[shapely.area(pieces) > 0])
        if footprint.difference(covered).area > 1e-8:
            raise ValueError("support footprint outside selected contact")
        used, inverse = np.unique(self.mesh.triangles[hits], return_inverse=True)
        return Mesh(self.mesh.vertices[used], inverse.reshape(-1, 3)), len(hits)


def _time_check(began, budget):
    if time.perf_counter() - began > budget.seconds:
        raise ValueError("support contact time budget")


@dataclass(frozen=True, slots=True, eq=False)
class SupportVolumeBuilder:
    """Retain two owned height meshes and prepare reusable bounded spatial indices.

    Preparation is measured separately; seconds is a cooperative per-build bound.
    All geometry uses explicit caller-local XY/Z metres, with no recipe or frame
    inference. Input height meshes must have nonzero projected triangle areas.
    """

    lower: Mesh
    upper: Mesh
    budget: SupportBudget = field(default_factory=SupportBudget)
    _lower: _Surface = field(init=False, repr=False)
    _upper: _Surface = field(init=False, repr=False)
    preparation_seconds: float = field(init=False)

    def __post_init__(self):
        if not isinstance(self.budget, SupportBudget):
            raise TypeError("explicit SupportBudget required")
        began = time.perf_counter()
        object.__setattr__(self, "_lower", _Surface(self.lower, self.budget))
        object.__setattr__(self, "_upper", _Surface(self.upper, self.budget))
        object.__setattr__(self, "preparation_seconds", time.perf_counter() - began)

    def build(self, footprint, *, minimum_height_m):
        """Return owned whole-contact volume/report, or None with a local reason.

        Invalid polygon/height parameters raise ValueError; finite contact,
        ambiguity, construction or work-budget absence returns no partial body.
        Supplied fields are assumed fixed during a call. Traffic, portals,
        existing piers, sources and physical safety belong to the caller.
        """
        if (
            not isinstance(footprint, shapely.Polygon)
            or footprint.is_empty
            or footprint.has_z
            or not np.isfinite(minimum_height_m)
            or minimum_height_m <= 0
        ):
            raise ValueError(
                "finite valid XY support polygon and positive height required"
            )
        began = time.perf_counter()
        if (
            shapely.get_num_coordinates(footprint)
            > self.budget.maximum_footprint_coordinates
        ):
            return None, {
                "valid": False,
                "reason": "support footprint coordinate budget",
                "seconds": time.perf_counter() - began,
            }
        if (
            not footprint.is_valid
            or not np.isfinite(shapely.get_coordinates(footprint)).all()
        ):
            raise ValueError("finite valid XY support polygon required")
        try:
            _time_check(began, self.budget)
            bottom, low_count = self._lower.patch(footprint, self.budget)
            top, high_count = self._upper.patch(footprint, self.budget)
            _time_check(began, self.budget)
            xy, faces, note = arrangement(footprint, bottom, top, self.budget)
            _time_check(began, self.budget)
            low, high = heights(bottom, xy, self.budget), heights(top, xy, self.budget)
            separation = high[:, 2] - low[:, 2]
            if not np.isfinite(separation).all() or separation.min() < minimum_height_m:
                raise ValueError("support insufficient whole contact height")
            product, volume = close(low, high, faces)
            shapes = shapely.polygons(low[faces, :2])
            if not shapely.coverage_is_valid(shapes):
                raise ValueError("support contact coverage invalid")
            error = (
                shapely.coverage_union_all(shapes).symmetric_difference(footprint).area
            )
            if error > 1e-8:
                raise ValueError("support contact footprint coverage failed")
            _time_check(began, self.budget)
        except (ValueError, shapely.errors.GEOSException) as failure:
            return None, {
                "valid": False,
                "reason": str(failure),
                "seconds": time.perf_counter() - began,
            }
        return product, dict(
            note,
            valid=True,
            reason="",
            seconds=time.perf_counter() - began,
            minimum_height_m=float(separation.min()),
            maximum_height_m=float(separation.max()),
            contact_faces=len(faces),
            source_face_candidates=low_count + high_count,
            coverage_error_m2=error,
            volume_m3=volume,
        )
