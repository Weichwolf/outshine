"""Bounded resident region extraction, with exact shared-coordinate canonicalisation."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import shapely

from ...contracts import Mesh
from ._patch import readonly


@dataclass(frozen=True, slots=True, eq=False)
class SurfaceQuery:
    mesh: Mesh | None
    face_keys: tuple
    face_data: object
    reason: str
    report: object

    def __post_init__(self):
        object.__setattr__(
            self,
            "face_data",
            MappingProxyType({k: readonly(v) for k, v in self.face_data.items()}),
        )
        object.__setattr__(self, "report", MappingProxyType(dict(self.report)))


def extract(field, selected, note):
    raw, keys = [], []
    attributes = {k: [] for k in field.patches[0].face_data}
    for i, ids in selected:
        patch = field.patches[i]
        raw.append(patch.mesh.vertices[patch.mesh.triangles[ids]])
        keys.extend((patch.patch_id, int(j)) for j in ids)
        for k, values in attributes.items():
            values.append(patch.face_data[k][ids])
    xyz = np.concatenate(raw).reshape(-1, 3)
    vertices, inverse = np.unique(xyz, axis=0, return_inverse=True)
    mesh = Mesh(vertices, inverse.reshape(-1, 3))
    note.update(
        raw_triangle_xyz_bytes=xyz.nbytes,
        unique_vertex_bytes=vertices.nbytes,
        canonical_face_label_bytes=inverse.nbytes,
        owned_mesh_bytes=mesh.vertices.nbytes + mesh.triangles.nbytes,
    )
    return SurfaceQuery(
        mesh,
        tuple(keys),
        {k: np.concatenate(v) for k, v in attributes.items()},
        "",
        note,
    )


def query(field, bounds, maximum, candidates, method):
    if (
        type(maximum) is not int
        or not 1 <= maximum <= 65536
        or type(candidates) is not int
        or not 0 <= candidates <= 2000000
        or method not in ("index", "scan")
    ):
        raise ValueError("known method and bounded local query capacities required")
    box = np.asarray(bounds, dtype=float)
    if box.shape != (4,) or not np.isfinite(box).all() or np.any(box[:2] >= box[2:]):
        raise ValueError("finite positive XY query box required")
    region = shapely.box(*box)
    note = {
        "resident_patches": len(field.patches),
        "patch_bounds_tests": 0,
        "candidate_upper_bound": 0,
        "face_tests": 0,
        "returned_faces": 0,
    }
    selected = []
    for i, patch in enumerate(field.patches):
        note["patch_bounds_tests"] += 1
        bound = np.asarray(patch._bounds)
        if (
            not field._counts[i]
            or np.any(bound[:2] > box[2:])
            or np.any(bound[2:] < box[:2])
        ):
            continue
        potential = len(patch.mesh.triangles)
        if note["candidate_upper_bound"] + potential > candidates:
            return SurfaceQuery(None, (), {}, "candidate_budget_exhausted", note)
        note["candidate_upper_bound"] += potential
        if method == "index":
            ids = patch._tree.query(region, predicate="intersects")
            note["face_tests"] += len(ids)
        else:
            ids = np.flatnonzero(shapely.intersects(patch._polygons, region))
            note["face_tests"] += potential
        ids = np.sort(ids[field._active[i][ids]])
        note["returned_faces"] += len(ids)
        if note["returned_faces"] > maximum:
            return SurfaceQuery(None, (), {}, "local_face_budget_exhausted", note)
        if len(ids):
            selected.append((i, ids))
    if not selected:
        return SurfaceQuery(None, (), {}, "outside_surface", note)
    return extract(field, selected, note)
