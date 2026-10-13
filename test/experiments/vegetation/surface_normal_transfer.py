"""Preserve unchanged corner normals and project changed corners to reference faces."""

import numpy as np
from spine_frames import unit


def interpolate_normals(reference, points, face_indices):
    triangle = reference["vertices"][reference["faces"][face_indices]]
    a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    n = np.cross(a, b)
    squared = np.sum(n**2, axis=1)
    relative = points - triangle[:, 0]
    u = np.sum(np.cross(relative, b) * n, axis=1) / squared
    v = np.sum(np.cross(a, relative) * n, axis=1) / squared
    weights = np.column_stack((1 - u - v, u, v))
    if np.min(weights) < -1e-3:
        raise ValueError("reference ray hit has inconsistent barycentric coordinates")
    weights = np.maximum(weights, 0)
    weights /= np.sum(weights, axis=1)[:, None]
    corners = reference["normals"].reshape(-1, 3, 3)[face_indices]
    return unit(np.sum(corners * weights[:, :, None], axis=1))


def restore_normals(solid, reference, raw):
    vertices = np.asarray(raw.vert_properties)[:, :3]
    faces = np.asarray(raw.tri_verts, dtype=np.int64)
    point_dtype = np.dtype([("x", np.float64), ("y", np.float64), ("z", np.float64)])
    original_keys = (
        np.ascontiguousarray(reference["vertices"]).view(point_dtype).ravel()
    )
    wanted = np.ascontiguousarray(vertices).view(point_dtype).ravel()
    order = np.argsort(original_keys)
    original_vertex = order[np.searchsorted(original_keys[order], wanted)]
    np.testing.assert_array_equal(reference["vertices"][original_vertex], vertices)
    old_faces = np.asarray(reference["faces"], dtype=np.int64)
    face_dtype = np.dtype([("a", np.int64), ("b", np.int64), ("c", np.int64)])
    original_face_keys = (
        np.ascontiguousarray(np.sort(old_faces, axis=1)).view(face_dtype).ravel()
    )
    source_faces = original_vertex[faces]
    wanted_faces = (
        np.ascontiguousarray(np.sort(source_faces, axis=1)).view(face_dtype).ravel()
    )
    face_order = np.argsort(original_face_keys)
    candidate = np.minimum(
        np.searchsorted(original_face_keys[face_order], wanted_faces),
        len(old_faces) - 1,
    )
    mapping = face_order[candidate]
    unchanged = original_face_keys[mapping] == wanted_faces
    normals = np.empty((len(faces), 3, 3))
    corner = np.argmax(
        old_faces[mapping[unchanged], :, None] == source_faces[unchanged, None], axis=1
    )
    normals[unchanged] = reference["normals"].reshape(-1, 3, 3)[
        mapping[unchanged, None], corner
    ]
    changed = np.flatnonzero(~unchanged)
    triangles = vertices[faces[changed]]
    geometric = unit(
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    )
    centroids = np.mean(triangles, axis=1)
    points, hit_faces = [], []
    epsilon = 1e-6
    for triangle, normal, centre in zip(triangles, geometric, centroids, strict=True):
        for point in triangle:
            query = point + 1e-4 * (centre - point)
            hits = [
                h
                for h in solid.ray_cast(
                    query + epsilon * normal, query - epsilon * normal
                )
                if np.dot(h.normal, normal) > 0
            ]
            if not hits:
                raise ValueError(
                    "simplified corner has no oriented reference surface hit"
                )
            hit = min(hits, key=lambda h: abs(h.distance - 0.5))
            points.append(hit.position)
            hit_faces.append(hit.face_id)
    if len(changed):
        normals[changed] = interpolate_normals(
            reference, np.asarray(points), np.asarray(hit_faces, dtype=int)
        ).reshape(-1, 3, 3)
    return {
        "vertices": vertices.copy(),
        "faces": faces.astype(np.int32),
        "normals": normals.reshape(-1, 3),
    }, {
        "unchanged_faces": int(np.count_nonzero(unchanged)),
        "projected_corners": len(points),
        "projection_ray_half_length_m": epsilon,
        "corner_interior_fraction": 1e-4,
    }
