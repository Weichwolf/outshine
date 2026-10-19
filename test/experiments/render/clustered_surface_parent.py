"""Geometry-only parent with a continuous two-sided displacement witness.

Each triangle is mapped affinely, kept unchanged, or has its entire image in
another retained triangle. A collapsed image without that witness stays original.
No source ownership, opacity, appearance or topology is inferred from positions.
"""

from dataclasses import dataclass

import numpy as np
from facing_intervals import dot, geometric_normals, length, point, sub


@dataclass(frozen=True)
class SurfaceParent:
    positions: np.ndarray
    triangles: np.ndarray
    original_vertex_ids: np.ndarray
    original_triangle_ids: np.ndarray
    corner_map: np.ndarray
    mode: np.ndarray
    covered_by: np.ndarray
    error_m: float
    work: dict


def _cycles(triangles):
    start = triangles.argmin(axis=1)
    return np.take_along_axis(triangles, (start[:, None] + np.arange(3)) % 3, axis=1)


def cluster_parent(positions, triangles, *, cell_m, max_triangles=2000000):
    """Bounded offline candidate; output positions are original encoded vertices.

    mode 0 = original, 1 = retained affine image, 2 = duplicate affine image,
    3 = collapsed affine image covered by a retained edge/vertex. covered_by
    addresses an output triangle. All original triangles have one witness.
    """
    positions, triangles = np.asarray(positions), np.asarray(triangles)
    if (
        positions.ndim != 2
        or positions.shape[1:] != (3,)
        or positions.dtype.kind != "f"
        or len(positions) > 6000000
        or not np.isfinite(positions).all()
        or np.abs(positions).max(initial=0) > 1e8
        or triangles.ndim != 2
        or triangles.shape[1:] != (3,)
        or triangles.dtype.kind not in "iu"
        or (triangles < 0).any()
        or triangles.max(initial=0) >= len(positions)
        or not np.isfinite(cell_m)
        or not 1e-6 <= cell_m <= 100000
        or type(max_triangles) is not int
        or not 0 <= max_triangles <= 2000000
    ):
        raise ValueError("bounded finite indexed geometry and grid required")
    if len(triangles) > max_triangles:
        raise ValueError("offline triangle budget: retain original product")
    if not len(triangles):
        raise ValueError("nonempty reference required; retain empty product")
    xyz = positions.astype(np.float64)
    used = np.unique(triangles)
    bins = np.floor((xyz[used] - xyz[used].min(axis=0)) / cell_m).astype(np.int64)
    _, first, inverse = np.unique(bins, axis=0, return_index=True, return_inverse=True)
    representatives = np.arange(len(xyz))
    representatives[used] = used[first[inverse]]
    images = representatives[triangles]
    reference, candidate = xyz[triangles], xyz[images]
    normals = geometric_normals(candidate)
    good = (length(normals)[0] > 1e-12) & (
        dot(geometric_normals(reference), normals)[0] > 0
    )
    rows = np.flatnonzero(good)
    _, first_image, duplicate = np.unique(
        _cycles(images[rows]), axis=0, return_index=True, return_inverse=True
    )
    owners = rows[first_image]
    cover = np.arange(len(triangles), dtype=np.int64)
    cover[rows] = owners[duplicate]
    mode = np.zeros(len(triangles), dtype=np.uint8)
    mode[rows] = 2
    mode[owners] = 1
    # These dictionaries are offline scratch, never a proposed frame data structure.
    vertices, edges = {}, {}
    for owner in owners:
        a, b, c = map(int, images[owner])
        for v in (a, b, c):
            vertices.setdefault(v, int(owner))
        for p, q in ((a, b), (b, c), (c, a)):
            edges.setdefault((min(p, q), max(p, q)), int(owner))
    for row in np.flatnonzero(~good):
        unique = sorted(set(map(int, images[row])))
        owner = None
        if len(unique) == 1:
            owner = vertices.get(unique[0])
        elif len(unique) == 2:
            owner = edges.get(tuple(unique))
        if owner is not None:
            cover[row] = owner
            mode[row] = 3
    kept = np.flatnonzero(mode <= 1)
    indexed = triangles[kept].copy()
    mapped = mode[kept] == 1
    indexed[mapped] = images[kept[mapped]]
    original_ids, compact = np.unique(indexed, return_inverse=True)
    parent_rows = np.full(len(triangles), -1, dtype=np.int64)
    parent_rows[kept] = np.arange(len(kept))
    covered_by = parent_rows[cover]
    if (covered_by < 0).any():
        raise AssertionError("image witness did not survive")
    displacement = length(sub(point(reference), point(candidate)))[1]
    displacement[(reference == candidate).all(axis=2)] = 0
    displacement[mode == 0] = 0
    bound = float(displacement.max(initial=0))
    arrays = (images, mode, covered_by, original_ids, kept)
    work = {
        "input_vertices": len(xyz),
        "referenced_vertices": len(used),
        "reference_triangles": len(triangles),
        "grid_clusters": len(first),
        "mapped_corner_distance_tests": 3 * len(triangles),
        "normal_tests": len(triangles),
        "unique_oriented_images": len(owners),
        "edge_insert_attempts": 3 * len(owners),
        "edge_entries": len(edges),
        "vertex_entries": len(vertices),
        "collapsed_witness_tests": int((~good).sum()),
        "unchanged_triangles": int((mode == 0).sum()),
        "duplicate_images": int((mode == 2).sum()),
        "covered_collapses": int((mode == 3).sum()),
        "parent_triangles": len(kept),
        "parent_vertices": len(original_ids),
        "reference_geometry_bytes": positions.nbytes + triangles.nbytes,
        "parent_position_bytes": positions[original_ids].nbytes,
        "parent_index_bytes_u32": 12 * len(kept),
        "witness_array_bytes": sum(a.nbytes for a in arrays),
        "known_large_array_bytes": sum(
            a.nbytes
            for a in (xyz, bins, inverse, representatives, reference, candidate)
        ),
        "gathered_reference_corner_bytes": reference.nbytes,
        "gathered_candidate_corner_bytes": candidate.nbytes,
    }
    return SurfaceParent(
        positions[original_ids],
        compact.reshape(-1, 3).astype(np.uint32),
        original_ids,
        kept,
        images,
        mode,
        covered_by,
        bound,
        work,
    )
