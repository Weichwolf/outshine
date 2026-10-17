"""One owned finite height surface and immutable local face attributes."""

from collections import Counter
from collections.abc import Mapping
from dataclasses import dataclass, field
from types import MappingProxyType

import numpy as np
import shapely

from ...contracts import LocalFrame, Mesh
from ._coverage import proves_single_boundary


def label(value):
    if not isinstance(value, str) or not value.strip() or len(value) > 512:
        raise ValueError("bounded nonempty patch identity/kind/attribute required")


def readonly(value):
    array = np.ascontiguousarray(value)
    return np.frombuffer(array.tobytes(), dtype=array.dtype).reshape(array.shape)


def boundary(mesh, indices):
    edges = Counter()
    for face in mesh.triangles[indices]:
        vertices = mesh.vertices[face]
        for a, b in ((0, 1), (1, 2), (2, 0)):
            edges[tuple(sorted((tuple(vertices[a]), tuple(vertices[b]))))] += 1
    if max(edges.values(), default=0) > 2:
        raise ValueError("nonmanifold patch boundary")
    return {edge for edge, count in edges.items() if count == 1}


@dataclass(frozen=True, slots=True, eq=False)
class SurfacePatch:
    patch_id: str
    frame: LocalFrame
    kind: str
    mesh: Mesh
    face_data: Mapping[str, np.ndarray] = field(default_factory=dict)
    max_faces: int = 65536
    _polygons: object = field(init=False, repr=False)
    _tree: object = field(init=False, repr=False)
    _bounds: tuple = field(init=False, repr=False)

    @property
    def bounds_xy_m(self):
        return self._bounds

    def __post_init__(self):
        label(self.patch_id)
        label(self.kind)
        if not isinstance(self.frame, LocalFrame) or not isinstance(self.mesh, Mesh):
            raise TypeError("explicit frame and owned mesh required")
        n = len(self.mesh.triangles)
        if (
            type(self.max_faces) is not int
            or not 1 <= self.max_faces <= 65536
            or not 1 <= n <= self.max_faces
            or len(self.mesh.vertices) > 65536
        ):
            raise ValueError("bounded nonempty individual surface patch required")
        if np.max(abs(self.mesh.vertices), initial=0) > 1e9:
            raise ValueError("bounded metric patch coordinates required")
        xyz = self.mesh.vertices[self.mesh.triangles]
        normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        if not np.isfinite(normal).all() or np.any(normal[:, 2] <= 0):
            raise ValueError("positive finite projected patch triangles required")
        xy, inverse = np.unique(self.mesh.vertices[:, :2], axis=0, return_inverse=True)
        low, high = np.full(len(xy), np.inf), np.full(len(xy), -np.inf)
        np.minimum.at(low, inverse, self.mesh.vertices[:, 2])
        np.maximum.at(high, inverse, self.mesh.vertices[:, 2])
        if np.any(low != high):
            raise ValueError("one exact height per explicit XY coordinate required")
        polygons = shapely.polygons(xyz[:, :, :2])
        if not proves_single_boundary(
            xy, inverse[self.mesh.triangles], polygons
        ) and not shapely.coverage_is_valid(polygons):
            raise ValueError("nonoverlapping edge-matched surface patch required")
        if not isinstance(self.face_data, Mapping) or len(self.face_data) > 8:
            raise ValueError("bounded explicit face-attribute mapping required")
        data = {}
        for key, values in self.face_data.items():
            label(key)
            array = np.asarray(values)
            if (
                array.shape != (n,)
                or array.dtype.kind not in "buif"
                or array.dtype.itemsize > 8
                or not np.isfinite(array).all()
            ):
                raise ValueError(
                    "one finite primitive attribute per local face required"
                )
            data[key] = readonly(array)
        polygons.flags.writeable = False
        object.__setattr__(self, "face_data", MappingProxyType(data))
        object.__setattr__(self, "_polygons", polygons)
        object.__setattr__(self, "_tree", shapely.STRtree(polygons))
        object.__setattr__(self, "_bounds", (*xy.min(axis=0), *xy.max(axis=0)))
