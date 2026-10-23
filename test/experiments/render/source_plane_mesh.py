"""Triangulated exact contour masks and explicit Float32 mesh accounting."""

import numpy as np
import shapely
from packed_normals import pack


def triangulate(domain):
    polygons = shapely.get_parts(shapely.constrained_delaunay_triangles(domain))
    if not len(polygons):
        raise ValueError("nonempty constrained polygon triangulation required")
    difference = abs(float(shapely.area(polygons).sum()) - domain.area)
    if difference > 1e-9 * max(1, domain.area):
        raise ValueError("triangulation must preserve projected source area")
    return np.asarray([shapely.get_coordinates(p)[:3] for p in polygons])


def pack_mesh(triangles, normals, group_ids):
    if not len(triangles) or len(triangles) > 131072:
        raise ValueError("bounded nonempty triangle output required")
    original = np.asarray(triangles)
    xyz = original.astype(np.float32)
    normal = np.asarray(normals, float)
    words = pack(normal)
    rows = np.column_stack((xyz.reshape(-1, 3).view(np.uint32), np.repeat(words, 3)))
    unique, inverse = np.unique(rows, axis=0, return_inverse=True)
    positions = unique[:, :3].copy().view(np.float32)
    index_type = np.uint16 if len(positions) <= 65536 else np.uint32
    indices = inverse.reshape(-1, 3).astype(index_type)
    geometric = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    reverse = np.einsum("ij,ij->i", geometric, normal) < 0
    indices[reverse] = indices[reverse][:, [0, 2, 1]]
    return {
        "positions": positions,
        "indices": indices,
        "normal_words": unique[:, 3].astype(np.uint32),
        "triangle_groups": np.asarray(group_ids, np.uint16),
    }, {
        "vertices": len(positions),
        "triangles": len(indices),
        "maximum_float32_vertex_rounding_m": float(
            np.linalg.norm(xyz.astype(float) - original, axis=2).max()
        ),
        "float32_degenerate_triangles": int(
            (np.linalg.norm(geometric, axis=1) == 0).sum()
        ),
    }


def union_mesh(groups):
    triangles, normals, owners = [], [], []
    for identity, group in enumerate(groups):
        local = triangulate(group.domain)
        world = local @ group.axes + group.offset * group.normal
        triangles.extend(world)
        normals.extend([group.normal] * len(world))
        owners.extend([identity] * len(world))
    return pack_mesh(triangles, normals, owners)


def unshifted_mesh(patches):
    triangles, normals, owners = [], [], []
    for identity, patch in enumerate(patches):
        local = triangulate(patch.domain)
        world = patch.origin + local @ patch.axes
        triangles.extend(world)
        normals.extend([patch.normal] * len(world))
        owners.extend([identity] * len(world))
    return pack_mesh(triangles, normals, owners)
