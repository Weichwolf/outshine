"""Immutable snapshots with local replacement witnesses and unchanged old meshes."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import shapely

from ._patch import SurfacePatch, boundary, readonly
from ._query import query


@dataclass(frozen=True, slots=True, init=False, eq=False)
class SurfacePatches:
    patches: tuple[SurfacePatch, ...]
    _active: tuple[np.ndarray, ...]
    _counts: tuple[int, ...]
    max_patches: int

    def __init__(self, root, *, max_patches=1024):
        if not isinstance(root, SurfacePatch):
            raise TypeError("one validated root patch required")
        if type(max_patches) is not int or not 1 <= max_patches <= 4096:
            raise ValueError("bounded resident patch capacity required")
        object.__setattr__(self, "patches", (root,))
        object.__setattr__(
            self, "_active", (readonly(np.ones(len(root.mesh.triangles), bool)),)
        )
        object.__setattr__(self, "max_patches", max_patches)
        object.__setattr__(self, "_counts", (len(root.mesh.triangles),))

    @property
    def active_faces(self):
        return MappingProxyType(
            {p.patch_id: a for p, a in zip(self.patches, self._active, strict=True)}
        )

    def replace(self, patch, removals, *, max_removed=2048, max_new_faces=8192):
        if (
            not isinstance(patch, SurfacePatch)
            or patch.frame != self.patches[0].frame
            or patch.kind != self.patches[0].kind
        ):
            raise ValueError(
                "replacement needs the same explicit frame and surface kind"
            )
        if len(self.patches) >= self.max_patches or patch.patch_id in self.active_faces:
            raise ValueError("resident patch budget or unique identity exhausted")
        if (
            type(max_removed) is not int
            or not 1 <= max_removed <= 65536
            or type(max_new_faces) is not int
            or not 1 <= max_new_faces <= 65536
            or len(patch.mesh.triangles) > max_new_faces
        ):
            raise ValueError("bounded local replacement capacities required")
        schema = {k: v.dtype for k, v in self.patches[0].face_data.items()}
        if schema != {k: v.dtype for k, v in patch.face_data.items()}:
            raise ValueError("unchanged face-attribute schema required")
        chosen = self._removals(removals, max_removed)
        old_polygons, old_edges = [], []
        for i, indices in chosen.items():
            old_polygons.extend(self.patches[i]._polygons[indices])
            old_edges.extend(boundary(self.patches[i].mesh, indices))
        # Internal seams between selected patches cancel by their complete XYZ edge.
        from collections import Counter

        counts = Counter(old_edges)
        if max(counts.values(), default=0) > 2:
            raise ValueError("ambiguous selected patch boundary")
        edges = {edge for edge, count in counts.items() if count == 1}
        if edges != boundary(patch.mesh, np.arange(len(patch.mesh.triangles))):
            raise ValueError("replacement changes whole exact XYZ boundary edges")
        region = shapely.union_all(old_polygons)
        if region.symmetric_difference(shapely.union_all(patch._polygons)).area > 1e-7:
            raise ValueError("replacement changes whole selected XY coverage")
        active, copied = list(self._active), 0
        for i, indices in chosen.items():
            values = active[i].copy()
            values[indices] = False
            active[i] = readonly(values)
            copied += values.nbytes
        active.append(readonly(np.ones(len(patch.mesh.triangles), bool)))
        result = object.__new__(SurfacePatches)
        object.__setattr__(result, "patches", (*self.patches, patch))
        object.__setattr__(result, "_active", tuple(active))
        counts = list(self._counts)
        for i, indices in chosen.items():
            counts[i] -= len(indices)
        object.__setattr__(result, "_counts", (*counts, len(patch.mesh.triangles)))
        object.__setattr__(result, "max_patches", self.max_patches)
        return result, {
            "removed_faces": sum(len(v) for v in chosen.values()),
            "new_faces": len(patch.mesh.triangles),
            "exact_boundary_edges": len(edges),
            "changed_old_mask_bytes": copied,
            "old_geometry_copy_bytes": 0,
            "scope": "Exact finite patch composition, not new height/physics/LOD admission.",
        }

    def _removals(self, removals, maximum):
        if not isinstance(removals, dict) or not removals:
            raise ValueError("explicit nonempty patch/local-face removals required")
        if sum(len(v) for v in removals.values()) > maximum:
            raise ValueError("local removed-face budget exhausted")
        lookup = {p.patch_id: i for i, p in enumerate(self.patches)}
        result = {}
        for key, ids in removals.items():
            if key not in lookup:
                raise ValueError("unknown old patch identity")
            i, raw = lookup[key], np.asarray(ids)
            if (
                raw.ndim != 1
                or not len(raw)
                or raw.dtype.kind not in "iu"
                or raw.min() < 0
                or raw.max() >= len(self._active[i])
                or len(np.unique(raw)) != len(raw)
                or not self._active[i][raw].all()
            ):
                raise ValueError("distinct existing active local face IDs required")
            result[i] = np.sort(raw.astype(np.int64))
        return result

    def query(self, bounds, *, max_faces=2048, max_candidates=2000000, method="index"):
        return query(self, bounds, max_faces, max_candidates, method)
