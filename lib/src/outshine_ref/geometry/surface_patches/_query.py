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


def capacities(maximum, candidates, method):
    if (
        type(maximum) is not int
        or not 1 <= maximum <= 65536
        or type(candidates) is not int
        or not 0 <= candidates <= 2000000
        or method not in ("index", "scan")
    ):
        raise ValueError("known method and bounded local query capacities required")


def region_query(field, region, distance, maximum, candidates, method):
    capacities(maximum, candidates, method)
    coordinates = shapely.get_coordinates(region)
    if (
        shapely.is_empty(region)
        or not shapely.is_valid(region)
        or not 1 <= len(coordinates) <= 65536
        or not np.isfinite(coordinates).all()
        or shapely.has_z(region)
        or not np.isfinite(distance)
        or not 0 <= distance <= 1e9
    ):
        raise ValueError(
            "bounded finite nonempty valid XY geometry and distance required"
        )
    box = np.asarray(region.bounds) + [-distance, -distance, distance, distance]
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
            ids = patch._tree.query(region, predicate="dwithin", distance=distance)
            note["face_tests"] += len(ids)
        else:
            ids = np.flatnonzero(shapely.dwithin(patch._polygons, region, distance))
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


def query(field, bounds, maximum, candidates, method):
    box = np.asarray(bounds, dtype=float)
    if box.shape != (4,) or not np.isfinite(box).all() or np.any(box[:2] >= box[2:]):
        raise ValueError("finite positive XY query box required")
    return region_query(field, shapely.box(*box), 0.0, maximum, candidates, method)


def faces(field, keys, maximum):
    capacities(maximum, 0, "index")
    if not isinstance(keys, (list, tuple)):
        raise TypeError("bounded explicit patch/local-face keys required")
    note = {"requested_faces": len(keys), "returned_faces": 0}
    if len(keys) > maximum:
        return SurfaceQuery(None, (), {}, "local_face_budget_exhausted", note)
    if not keys:
        return SurfaceQuery(None, (), {}, "outside_surface", note)
    removals = {}
    for key in keys:
        if (
            not isinstance(key, (list, tuple))
            or len(key) != 2
            or type(key[1]) is not int
        ):
            raise ValueError("explicit patch identity and integer face index required")
        removals.setdefault(key[0], []).append(key[1])
    chosen = field._removals(removals, maximum)
    note["returned_faces"] = len(keys)
    return extract(field, sorted(chosen.items()), note)
