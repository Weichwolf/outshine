"""Complete affine contact strips on an explicitly selected existing height mesh."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh


@dataclass
class ContactSurface:
    mesh: Mesh
    max_faces: int = 65536

    def __post_init__(self):
        if (
            not isinstance(self.mesh, Mesh)
            or not 0 < len(self.mesh.triangles) <= self.max_faces
        ):
            raise ValueError(
                "nonempty selected height mesh within the face budget required"
            )
        self.points = self.mesh.vertices[self.mesh.triangles]
        normal = np.cross(
            self.points[:, 1] - self.points[:, 0], self.points[:, 2] - self.points[:, 0]
        )
        if not np.isfinite(normal).all() or np.any(normal[:, 2] == 0):
            raise ValueError("finite nonvertical height triangles required")
        self.gradients = -normal[:, :2] / normal[:, 2:3]
        if not np.isfinite(self.gradients).all():
            raise ValueError("finite selected height gradients required")
        self.polygons = shapely.polygons(self.points[:, :, :2])
        self.tree = shapely.STRtree(self.polygons)

    def strip(self, center, direction, width, *, exterior_side, max_candidates=65536):
        center, direction = np.asarray(center, float), np.asarray(direction, float)
        if (
            center.shape != (2,)
            or direction.shape != (2,)
            or not np.isfinite([center, direction]).all()
        ):
            raise ValueError("finite XY center and direction required")
        if not np.isclose(np.linalg.norm(direction), 1, rtol=0, atol=1e-12):
            raise ValueError("unit longitudinal direction required")
        if (
            not np.isfinite(width)
            or not 0 < width <= 1e4
            or exterior_side not in (-1, 1)
        ):
            raise ValueError("bounded full width and explicit exterior side required")
        note = {
            "valid": False,
            "reason": "contact_candidate_budget",
            "candidate_count": 0,
        }
        if len(self.points) > max_candidates:
            return note
        transverse = np.array([-direction[1], direction[0]])
        ends = center + np.array([-width / 2, width / 2])[:, None] * transverse
        line = shapely.LineString(ends)
        ids = self.tree.query(line)
        note["candidate_count"] = len(ids)
        rounding = 64 * np.finfo(float).eps * max(1, abs(ends).max(), width)
        outside = ((self.points[ids, :, :2] - center) @ direction * exterior_side).max(
            axis=1
        )
        ids = ids[outside > rounding]
        intersections = shapely.intersection(self.polygons[ids], line)
        coordinates, parent = shapely.get_coordinates(intersections, return_index=True)
        if not len(coordinates):
            note["reason"] = "missing_contact_coverage"
            return note
        offsets = (coordinates - center) @ transverse
        low, high = np.full(len(ids), np.inf), np.full(len(ids), -np.inf)
        np.minimum.at(low, parent, offsets)
        np.maximum.at(high, parent, offsets)
        positive = high - low > rounding
        ids, low, high = ids[positive], low[positive], high[positive]
        order = np.lexsort((ids, high, low))
        ids, low, high = ids[order], low[order], high[order]
        covered = -width / 2
        for begin, end in zip(low, high):
            if begin < covered - rounding:
                note["reason"] = "ambiguous_contact_surface"
                return note
            if begin > covered + rounding:
                note["reason"] = "missing_contact_coverage"
                return note
            covered = max(covered, end)
        if covered < width / 2 - rounding or not len(ids):
            note["reason"] = "missing_contact_coverage"
            return note
        gradients = self.gradients[ids]
        anchor = self.points[ids, 0]
        heights = anchor[:, 2] + np.einsum(
            "ij,ij->i", center - anchor[:, :2], gradients
        )
        grades = gradients @ np.c_[direction, transverse]
        if np.ptp(heights) > 1e-7 or np.any(np.ptp(grades, axis=0) > 1e-10):
            note["reason"] = "nonplanar_contact_strip"
            return note
        note.update(
            valid=True,
            reason="",
            center_xy_m=center.tolist(),
            width_m=float(width),
            center_height_m=float(heights[0]),
            longitudinal_grade=float(grades[0, 0]),
            crossfall=float(grades[0, 1]),
            face_indices=ids.tolist(),
            transverse_intervals_m=np.c_[low, high].tolist(),
            exterior_side=exterior_side,
            full_width_height_spread_m=float(np.ptp(heights)),
            full_width_grade_spread=float(np.ptp(grades, axis=0).max()),
        )
        return note
