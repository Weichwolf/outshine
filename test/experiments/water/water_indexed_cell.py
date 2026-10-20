"""Exact indexing of admitted parameter cuts; no coordinate repair (#963).

The finite input owns the mask and physical qualification. This stage preserves
every triangle position, source membership and parent normal, then checks the
indexed planar complex. Point-pinched or nonconforming cuts refuse explicitly.
This Python product does not define a native asset/GPU/cache format.
"""

import dataclasses
import hashlib
from dataclasses import dataclass

import numpy as np
import shapely
from scipy.sparse import coo_matrix
from scipy.sparse.csgraph import connected_components
from water_parameter_cut import local_sources


@dataclass(frozen=True)
class IndexedCell:
    cell: tuple
    context: str
    source_keys: tuple
    parameter: np.ndarray
    world: np.ndarray
    faces: np.ndarray
    parents: np.ndarray
    parent_normals: np.ndarray
    membership: object
    boundary: np.ndarray
    report: dict


def owned(array, dtype=None):
    result = np.array(array, dtype=dtype, copy=True)
    result.flags.writeable = False
    return result


def copy_membership(membership, faces, sources):
    offsets, members = membership.offsets, membership.members
    if (
        offsets.shape != (faces + 1,)
        or offsets.dtype.kind not in "ui"
        or offsets[0] != 0
        or offsets[-1] != len(members)
        or np.any(offsets[1:] <= offsets[:-1])
        or not 0 < len(members) <= 100000
        or members.dtype.kind not in "ui"
        or np.any(members < 0)
        or np.any(members >= sources)
        or membership.status.shape != (faces,)
        or np.any(membership.status)
        or membership.representative.shape != (faces,)
        or membership.representative.dtype.kind not in "ui"
        or np.any(membership.representative < 0)
        or np.any(membership.representative >= sources)
        or membership.member_area_m2.shape != (len(members),)
        or not np.isfinite(membership.member_area_m2).all()
        or np.any(membership.member_area_m2 <= 0)
        or membership.uncovered_area_m2.shape != (faces,)
        or not np.isfinite(membership.uncovered_area_m2).all()
        or np.any(membership.uncovered_area_m2 < 0)
    ):
        raise ValueError("complete bounded face-aligned source CSR required")
    arrays = {
        name: owned(getattr(membership, name))
        for name in (
            "offsets",
            "members",
            "representative",
            "member_area_m2",
            "uncovered_area_m2",
            "status",
        )
    }
    return dataclasses.replace(membership, **arrays), sum(
        a.nbytes for a in arrays.values()
    )


def topology(points, faces, mask):
    directed = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    edges, inverse, counts = np.unique(
        np.sort(directed, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    if np.any(counts > 2):
        raise ValueError("nonmanifold indexed edge")
    winding = np.zeros(len(edges), dtype=int)
    np.add.at(winding, inverse, np.where(directed[:, 0] < directed[:, 1], 1, -1))
    if np.any(winding[counts == 2]):
        raise ValueError("inconsistent indexed face winding")
    boundary = edges[counts == 1]
    degree = np.bincount(boundary.ravel(), minlength=len(points))
    if np.any((degree != 0) & (degree != 2)):
        raise ValueError("nonconforming or pinched boundary vertex")
    links = coo_matrix(
        (np.ones(len(edges)), edges.T), shape=(len(points), len(points))
    ).tocsr()
    components, _ = connected_components(links, directed=False)
    boundary_links = coo_matrix(
        (np.ones(len(boundary)), boundary.T), shape=(len(points), len(points))
    ).tocsr()
    _, labels = connected_components(boundary_links, directed=False)
    loops = len(np.unique(labels[np.unique(boundary)]))
    polygons = shapely.get_parts(mask)
    expected_components = len(polygons)
    holes = sum(len(p.interiors) for p in polygons)
    euler = len(points) - len(edges) + len(faces)
    if (components, loops, euler) != (
        expected_components,
        expected_components + holes,
        expected_components - holes,
    ):
        raise ValueError("indexed topology differs from source components or holes")
    return boundary, {
        "vertices": len(points),
        "edges": len(edges),
        "faces": len(faces),
        "components": int(components),
        "boundary_loops": loops,
        "holes": holes,
        "euler_characteristic": euler,
    }


def index_cell(cut, polygons):
    """Own indexed arrays after validating a previously admitted whole-cell cut.

    Exact XY identity is the only merge criterion. Equal XY with different ECEF
    values refuses; no displacement threshold or height/normal refit is used.
    Face order stays unchanged, so source CSR and parent ownership stay aligned.
    """
    if cut.report.get("state") != "diagnostic_cut" or not cut.context:
        raise ValueError("qualified whole-cell parameter cut required")
    n = len(cut.parameter)
    if (
        not 0 < n <= 2048
        or cut.parameter.shape != (n, 3, 2)
        or cut.world.shape != (n, 3, 3)
        or cut.parents.shape != (n,)
        or cut.normals.shape != (n, 3)
        or cut.parents.dtype.kind not in "ui"
        or np.any(cut.parents < 0)
        or np.any(cut.parents >= 8)
        or not all(
            np.isfinite(a).all() for a in (cut.parameter, cut.world, cut.normals)
        )
    ):
        raise ValueError("bounded finite canonical parameter cut required")
    _, mask = local_sources(cut.cell, polygons, cut.source_keys, 32768)
    if hashlib.sha256(shapely.normalize(mask).wkb).hexdigest() != cut.report.get(
        "mask_digest"
    ):
        raise ValueError("source mask differs from qualified cut")
    points, first, inverse = np.unique(
        cut.parameter.reshape(-1, 2), axis=0, return_index=True, return_inverse=True
    )
    world = cut.world.reshape(-1, 3)[first]
    if not np.array_equal(world[inverse], cut.world.reshape(-1, 3)):
        raise ValueError("equal parameter coordinates have different world values")
    faces = inverse.reshape(-1, 3)
    vectors = points[faces[:, 1:]] - points[faces[:, :1]]
    area = vectors[:, 0, 0] * vectors[:, 1, 1] - vectors[:, 0, 1] * vectors[:, 1, 0]
    if np.any(area >= 0):
        raise ValueError("degenerate or reversed parameter triangle")
    boundary, audit = topology(points, faces, mask)
    normals = np.zeros((int(cut.parents.max()) + 1, 3))
    for parent in np.unique(cut.parents):
        selected = cut.normals[cut.parents == parent]
        if np.any(selected != selected[0]):
            raise ValueError("one canonical root has inconsistent normals")
        normals[parent] = selected[0]
    membership, csr_bytes = copy_membership(cut.membership, n, len(cut.source_keys))
    arrays = {
        "parameter": owned(points),
        "world": owned(world),
        "faces": owned(faces, np.uint32),
        "parents": owned(cut.parents, np.uint8),
        "parent_normals": owned(normals),
        "boundary": owned(boundary, np.uint32),
    }
    numerical = sum(a.nbytes for a in arrays.values()) + csr_bytes
    report = {
        "state": "indexed_finite_cell",
        "topology": audit,
        "soup_numerical_bytes": cut.report["numerical_array_bytes"],
        "indexed_numerical_bytes": numerical,
        "owned_array_copy_bytes": numerical,
        "unique_index_work_arrays_bytes": first.nbytes + inverse.nbytes,
        "world_gather_bytes": world.nbytes,
        "world_equality_reconstruction_bytes": cut.world.nbytes,
        "unique_parameter_input_vertices": 3 * n,
        "geometry_displacement_m": 0.0,
        "geos_workspace_bytes": None,
        "scope": "unchanged finite admitted cut; no native or worldwide admission",
    }
    return IndexedCell(
        cut.cell,
        cut.context,
        cut.source_keys,
        membership=membership,
        report=report,
        **arrays,
    )
