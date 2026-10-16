"""Ground-anchored opaque blade contacts, not a canopy lighting model."""

import numpy as np
from raster_batch import fragments
from raster_rows import fragments as row_fragments


def query_domain(lower, upper, direction, size):
    lower, upper, direction = (np.asarray(v, float) for v in (lower, upper, direction))
    if (
        lower.shape != (2,)
        or upper.shape != (2,)
        or direction.shape != (3,)
        or not np.isfinite(np.r_[lower, upper, direction]).all()
        or np.any(upper <= lower)
        or np.max(np.abs(np.r_[lower, upper])) > 100
        or abs(np.linalg.norm(direction) - 1) > 1e-12
        or direction[2] < 0.25
        or type(size) is not int
        or not 16 <= size <= 1024
        or size & (size - 1)
    ):
        raise ValueError(
            "bounded footprint, unit upper direction and power-of-two16..1024 required"
        )
    return lower, upper, direction


def guard_bounds(lower, upper, direction, radius, height):
    """Conservative roots whose physical blades can cross footprint rays above z=0."""
    lower, upper, direction = query_domain(lower, upper, direction, 16)
    if not np.isfinite([radius, height]).all() or radius < 0 or height < 0:
        raise ValueError("finite nonnegative physical form extents required")
    shift = height * direction[:2] / direction[2]
    return lower + np.minimum(shift, 0) - radius, upper + np.maximum(shift, 0) + radius


def projected_mesh(vertices, faces, lower, upper, direction, size):
    lower, upper, direction = query_domain(lower, upper, direction, size)
    if (
        vertices.ndim != 2
        or vertices.shape[1] != 3
        or len(vertices) > 1_000_000
        or not np.isfinite(vertices).all()
        or np.max(np.abs(vertices), initial=0) > 100
        or faces.ndim != 2
        or faces.shape[1] != 3
        or faces.dtype.kind not in "iu"
        or len(faces) > 500_000
        or np.min(faces, initial=0) < 0
        or np.max(faces, initial=-1) >= len(vertices)
    ):
        raise ValueError("bounded finite metric triangle reference required")
    projected = vertices.copy()
    projected[:, :2] -= vertices[:, 2, None] * direction[:2] / direction[2]
    return projected, lower, upper


def batch_winners(triangle, pixel, z, count, method):
    if method == "sort":
        order = np.lexsort((triangle, z, pixel))
        chosen = order[np.r_[pixel[order][1:] != pixel[order][:-1], True]]
        return pixel[chosen], triangle[chosen], z[chosen]
    depth = np.full(count, -np.inf)
    face = np.full(count, -1, np.int64)
    np.maximum.at(depth, pixel, z)
    foremost = z == depth[pixel]
    np.maximum.at(face, pixel[foremost], triangle[foremost])
    chosen = np.flatnonzero(face >= 0)
    return chosen, face[chosen], depth[chosen]


def contacts(
    vertices,
    faces,
    lower,
    upper,
    direction,
    size,
    pair_budget=262144,
    method="scatter",
    sampler="boxes",
):
    projected, lower, upper = projected_mesh(
        vertices, faces, lower, upper, direction, size
    )
    if type(pair_budget) is not int or not size * size <= pair_budget <= 4_194_304:
        raise ValueError("bounded pair batch with room for one full footprint required")
    if method not in ("sort", "scatter"):
        raise ValueError("explicit sort/scatter winner comparison required")
    if sampler not in ("boxes", "rows"):
        raise ValueError("explicit boxes/rows candidate sampler required")
    depth = np.full(size * size, -np.inf)
    face = np.full(size * size, -1, np.int32)
    work = {"accepted_fragments": 0, "batches": 0, "max_fragment_batch": 0}
    triangles_xy = (projected[:, :2] - lower) / (upper - lower) * size
    triangles_xy = triangles_xy[faces]
    first = np.maximum(np.floor(triangles_xy.min(1)).astype(int), 0)
    last = np.minimum(np.ceil(triangles_xy.max(1)).astype(int), size)
    span = np.maximum(last - first, 0)
    ab, ac = (
        triangles_xy[:, 1] - triangles_xy[:, 0],
        triangles_xy[:, 2] - triangles_xy[:, 0],
    )
    determinant = ab[:, 0] * ac[:, 1] - ab[:, 1] * ac[:, 0]
    counts = np.where(abs(determinant) >= 1e-12, span[:, 0] * span[:, 1], 0)
    work.update(
        input_triangles=len(faces),
        bounding_box_pairs=int(counts.sum()),
        input_geometry_bytes=vertices.nbytes + faces.nbytes,
        projected_vertex_copy_bytes=projected.nbytes,
    )
    iterator = (
        row_fragments(projected, faces, lower, upper, size, pair_budget, work)
        if sampler == "rows"
        else fragments(projected, faces, lower, upper, size, pair_budget)
    )
    for triangle, _, pixel, z in iterator:
        valid = z >= 0
        triangle, pixel, z = triangle[valid], pixel[valid], z[valid]
        work["accepted_fragments"] += len(pixel)
        work["max_fragment_batch"] = max(work["max_fragment_batch"], len(pixel))
        work["batches"] += 1
        if not len(pixel):
            continue
        # Preserve canonical face-index ties across batch partitions.
        p, t, h = batch_winners(triangle, pixel, z, size * size, method)
        better = (h > depth[p]) | ((h == depth[p]) & (t > face[p]))
        depth[p[better]], face[p[better]] = h[better], t[better]
    covered = face >= 0
    height = np.where(covered, depth, 0)
    normal = np.zeros((size * size, 3))
    triangles = vertices[faces]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    length = np.linalg.norm(normals, axis=1)
    if np.any(length == 0):
        raise ValueError("nondegenerate physical triangles required")
    normals /= length[:, None]
    normal[covered] = normals[face[covered]]
    work["returned_array_bytes"] = (
        face.nbytes + height.nbytes + normal.nbytes + covered.size * 8
    )
    work["scatter_grid_initialization_bytes"] = (
        work["batches"] * size * size * 16 if method == "scatter" else 0
    )
    # Geometric normals remain authored; orientation does not change opaque coverage.
    return {
        "face": face.reshape(size, size),
        "height": height.reshape(size, size),
        "normal": normal.reshape(size, size, 3),
        "coverage": covered.reshape(size, size).astype(np.float64),
        "work": work,
    }


def footprint_mips(coverage, height, normal):
    """Exact equal-area box means; moments stay premultiplied by blade coverage."""
    base = np.concatenate(
        (
            coverage[..., None],
            (coverage * height)[..., None],
            normal * coverage[..., None],
        ),
        axis=-1,
    )
    levels = [base]
    while base.shape[0] > 1:
        n = base.shape[0] // 2
        base = base.reshape(n, 2, n, 2, 5).mean(axis=(1, 3))
        levels.append(base)
    return levels
