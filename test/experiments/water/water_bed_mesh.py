"""A bounded bed surface from an admitted water cut and explicit depth/shore facts.

The water asset stays unchanged. Bed facets retain original water-face lineage;
they do not reuse its geographic mask as a new bed-query authority.
"""

import math
from dataclasses import dataclass

import numpy as np
from water_indexed_cell import IndexedCell


@dataclass(frozen=True)
class BedLayout:
    water: IndexedCell
    top_vertices: np.ndarray
    faces: np.ndarray
    parent_faces: np.ndarray
    boundary: np.ndarray
    bank_vertices: np.ndarray


@dataclass(frozen=True)
class BedMesh:
    water: IndexedCell
    vertices: np.ndarray
    faces: np.ndarray
    parent_faces: np.ndarray
    boundary: np.ndarray
    normals: np.ndarray
    report: dict


def _freeze(array, dtype):
    array = np.asarray(array, dtype=dtype)
    array.setflags(write=False)
    return array


def refine_bed(water, bank_edges):
    """Caller identifies actual shores; unqualified cuts must not become shores.

    Six children per water face add shared edge midpoints and one face centre.
    This avoids the zero-depth result of tapering boundary-only water vertices.
    """
    if not isinstance(water, IndexedCell) or not 0 < len(water.faces) <= 2048:
        raise ValueError("bounded admitted indexed water cell required")
    world, faces = water.world, water.faces
    if (
        world.ndim != 2
        or world.shape[1] != 3
        or len(world) > 8192
        or not np.isfinite(world).all()
        or np.any(abs(world) > 1e8)
        or faces.shape != (len(faces), 3)
        or faces.dtype.kind not in "ui"
        or np.any(faces < 0)
        or np.any(faces >= len(world))
    ):
        raise ValueError("finite water vertices and valid bounded faces required")
    bank_edges = np.asarray(bank_edges)
    if bank_edges.ndim == 1 and bank_edges.size == 0:
        bank_edges = np.empty((0, 2), dtype=np.uint32)
    if (
        bank_edges.ndim != 2
        or len(water.boundary) > 3 * len(faces)
        or len(bank_edges) > len(water.boundary)
        or bank_edges.shape != (len(bank_edges), 2)
        or bank_edges.dtype.kind not in "ui"
    ):
        raise ValueError("bounded original bank-edge indices required")
    boundary = {tuple(sorted(map(int, e))) for e in water.boundary}
    banks = {tuple(sorted(map(int, e))) for e in bank_edges}
    if not banks <= boundary:
        raise ValueError("actual bank edges must belong to the water boundary")
    edges = np.unique(
        np.sort(
            np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]),
            axis=1,
        ),
        axis=0,
    )
    if len(world) + len(edges) + len(faces) > 16384:
        raise ValueError("bed vertex budget exceeded")
    edge_ids = {tuple(map(int, e)): len(world) + i for i, e in enumerate(edges)}
    top = np.vstack([world, world[edges].mean(axis=1), world[faces].mean(axis=1)])
    children, parents = [], []
    for owner, face in enumerate(faces):
        centre = len(world) + len(edges) + owner
        for a, b in zip(face, np.roll(face, -1), strict=True):
            middle = edge_ids[tuple(sorted((int(a), int(b))))]
            children.extend(((int(a), middle, centre), (middle, int(b), centre)))
            parents.extend((owner, owner))
    shores = set()
    for a, b in banks:
        shores.update((a, b, edge_ids[(a, b)]))
    split_boundary = []
    for a, b in sorted(boundary):
        middle = edge_ids[(a, b)]
        split_boundary.extend(((a, middle), (middle, b)))
    return BedLayout(
        water,
        _freeze(top, float),
        _freeze(children, np.uint32),
        _freeze(parents, np.uint32),
        _freeze(split_boundary, np.uint32).reshape(-1, 2),
        _freeze(sorted(shores), np.uint32),
    )


def build_bed(layout, depths_m, up):
    """Materialize explicit targets; audit whole facets against their water face.

    The64ULP coordinate allowance is a Float64 materialization check, not an
    excavation tolerance or proof for Float32/native terrain. Shore depths are0.
    The caller owns consistent depth recipes and qualified external bed ports.
    """
    top = layout.top_vertices
    depth, up = np.asarray(depths_m, float), np.asarray(up, float)
    if (
        depth.shape != (len(top),)
        or up.shape != top.shape
        or not np.isfinite(depth).all()
        or not np.isfinite(up).all()
        or np.any(depth < 0)
        or np.any(depth > 1e8)
        or np.any(abs(up) > 2)
        or np.any(abs(np.linalg.norm(up, axis=1) - 1) > 1e-12)
        or np.any(depth[layout.bank_vertices] != 0)
    ):
        raise ValueError(
            "finite nonnegative depths, unit Up and zero true-shore depth required"
        )
    vertices = top - depth[:, None] * up
    vertices[depth == 0] = top[depth == 0]
    return bed_from_vertices(layout, vertices, target_depth_m=float(depth.max()))


def bed_from_vertices(layout, vertices, *, target_depth_m=None):
    """Audit a fitted or direct surface with unchanged known bank vertices."""
    top = layout.top_vertices
    vertices = np.asarray(vertices, float)
    if (
        vertices.shape != top.shape
        or not np.isfinite(vertices).all()
        or not np.array_equal(vertices[layout.bank_vertices], top[layout.bank_vertices])
    ):
        raise ValueError("finite bed vertices and unchanged known banks required")
    source = layout.water.world[layout.water.faces]
    normal = np.cross(source[:, 1] - source[:, 0], source[:, 2] - source[:, 0])
    length = np.linalg.norm(normal, axis=1)
    if not np.isfinite(vertices).all() or np.any(length <= 0):
        raise ValueError("finite bed and nondegenerate water required")
    normal /= length[:, None]
    parent = layout.parent_faces
    triangles = vertices[layout.faces]
    bed_normal = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    bed_length = np.linalg.norm(bed_normal, axis=1)
    projected_area = np.einsum("ij,ij->i", bed_normal, normal[parent])
    if np.any(bed_length <= 0) or np.any(projected_area <= 0):
        raise ValueError("bed has a degenerate or folded facet")
    penetration = np.einsum(
        "fij,fj->fi", triangles - source[parent, :1], normal[parent]
    )
    allowance = 64 * math.ulp(max(1.0, float(abs(top).max())))
    maximum = float(penetration.max())
    if maximum > allowance:
        raise ValueError("whole bed facet pierces its water face")
    bed_normal /= bed_length[:, None]
    return BedMesh(
        layout.water,
        _freeze(vertices, float),
        layout.faces,
        parent,
        layout.boundary,
        _freeze(bed_normal, float),
        {
            "faces": len(parent),
            "vertices": len(vertices),
            "original_water_faces": len(layout.water.faces),
            "maximum_water_plane_penetration_m": maximum,
            "coordinate_roundoff_allowance_m": allowance,
            "minimum_projected_double_area_m2": float(projected_area.min()),
            "maximum_target_depth_m": target_depth_m,
            "maximum_water_normal_depth_m": float(-penetration.min()),
            "source_csr_copies": 0,
            "source_lineage": "Original water-face overlap candidates; not recomputed child source CSR.",
            "output_array_bytes": sum(
                a.nbytes
                for a in (vertices, layout.faces, parent, layout.boundary, bed_normal)
            ),
            "scope": "Numerically checked local bed facets with original-face source lineage. Global self-intersection, external bed ports, depth/shore completeness, terrain excavation and native/Float32 admission remain separate.",
        },
    )
