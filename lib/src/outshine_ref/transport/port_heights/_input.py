"""Validated input ownership and source-index-preserving internal layout adapters."""

from dataclasses import dataclass

import numpy as np

from ...contracts import Mesh
from ._arrays import immutable_mesh, owned


@dataclass(frozen=True, slots=True)
class PortHeightInput:
    """A conforming shared XY surface with height priors and whole road ports.

    Part roles are corridor=0, junction=1; face part -1 denotes exterior soil.
    Ports are whole indexed edges, each joining one corridor and one junction.
    Grade frames normalize each triangle's XY gradient into its allowed ellipse.
    Every part/vertex must be used. Face order and module indices remain stable;
    clockwise triangles are normalized without changing their geometry or IDs.
    Frame, surface kind, original source IDs/attributes and deferred source reasons
    remain caller-owned, as with fit_surface_heights and SurfaceSampler.
    """

    surface: Mesh
    face_parts: np.ndarray
    part_roles: np.ndarray
    port_edges: np.ndarray
    port_parts: np.ndarray
    grade_frames: np.ndarray

    def __post_init__(self):
        if not isinstance(self.surface, Mesh):
            raise TypeError("one explicit Mesh with original height priors required")
        n, f = len(self.surface.vertices), len(self.surface.triangles)
        m, p = len(self.part_roles), len(self.port_edges)
        if not 0 < n <= 65536 or not 0 < f <= 65536 or not 0 < m <= 8192 or p > 16384:
            raise ValueError("port input exceeds the structural capture limits")
        for name, dtype, shape, integer in (
            ("face_parts", np.int64, (f,), True),
            ("part_roles", np.int64, (m,), True),
            ("port_edges", np.int64, (p, 2), True),
            ("port_parts", np.int64, (p, 2), True),
            ("grade_frames", float, (f, 2, 2), False),
        ):
            object.__setattr__(
                self,
                name,
                owned(getattr(self, name), dtype, shape, name, integer=integer),
            )
        if not np.isin(self.part_roles, [0, 1]).all():
            raise ValueError("part roles must be corridor=0 or junction=1")
        if np.any(self.face_parts < -1) or np.any(self.face_parts >= m):
            raise ValueError("face parts must be existing modules or exterior soil")
        if np.any(self.port_edges < 0) or np.any(self.port_edges >= n):
            raise ValueError("port endpoints must be existing source vertices")
        if np.any(self.port_parts < 0) or np.any(self.port_parts >= m):
            raise ValueError("port modules must exist")
        if np.any(self.port_parts[:, 0] == self.port_parts[:, 1]):
            raise ValueError("ports need two different source modules")
        if (
            abs(self.surface.vertices).max() > 4e7
            or abs(self.grade_frames).max() > 1e12
        ):
            raise ValueError("bounded metric coordinates and grade frames required")
        if np.any(np.linalg.det(self.grade_frames) == 0):
            raise ValueError("nonsingular grade frames required")
        self._own_surface()

    def _own_surface(self):
        surface = immutable_mesh(self.surface)
        faces = surface.triangles.copy()
        q = surface.vertices[faces, :2]
        a, b = q[:, 1] - q[:, 0], q[:, 2] - q[:, 0]
        area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
        if np.any(area == 0):
            raise ValueError("positive projected source areas required")
        if len(np.unique(faces)) != len(surface.vertices):
            raise ValueError("every source vertex must be referenced")
        road = self.face_parts[self.face_parts >= 0]
        if not np.array_equal(np.unique(road), np.arange(len(self.part_roles))):
            raise ValueError("every retained source module needs faces")
        faces[area < 0] = faces[area < 0][:, [0, 2, 1]]
        object.__setattr__(
            self, "surface", immutable_mesh(Mesh(surface.vertices, faces))
        )


def prepare_input(source):
    xy, faces = source.surface.vertices[:, :2], source.surface.triangles
    road = source.face_parts >= 0
    vertices, indices = np.unique(faces[road], return_inverse=True)
    lookup = np.full(len(xy), -1, int)
    lookup[vertices] = np.arange(len(vertices))
    if np.any(lookup[source.port_edges] < 0):
        raise ValueError("whole port vertices must belong to road faces")
    product = {
        "vertices": xy[vertices],
        "indices": indices.reshape(-1, 3),
        "face_owners": source.face_parts[road],
        "part_roles": source.part_roles,
        "port_edges": lookup[source.port_edges],
        "port_parts": source.port_parts,
    }
    captured = {
        "ground_xy": xy,
        "ground_faces": faces,
        "ground_frames": source.grade_frames,
        "ground_targets": source.surface.vertices[:, 2],
        "road_frames": source.grade_frames[road],
        "road_vertex_map": vertices,
    }
    original = {
        "xy": xy,
        "faces": faces,
        "parts": source.face_parts,
        "frames": source.grade_frames,
        "targets": source.surface.vertices[:, 2],
        "roots": np.arange(len(faces)),
        "port_roots": {
            tuple(sorted(edge)): i for i, edge in enumerate(source.port_edges)
        },
    }
    return product, captured, original
