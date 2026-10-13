"""Collapse only sub-tolerance Boolean fragments with a valid manifold link."""

import manifold3d as manifold
import numpy as np


def remove_fragments(raw, tolerance=1e-7):
    vertices = np.asarray(raw.vert_properties)[:, :3]
    faces = np.asarray(raw.tri_verts, dtype=np.int64).copy()
    threshold = np.linalg.norm(np.ptp(vertices, axis=0)) ** 2 * 1e-15
    used, displacements = set(), []
    for _ in range(33):
        triangles = vertices[faces]
        cross = np.cross(
            triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        )
        small = np.flatnonzero(np.linalg.norm(cross, axis=1) <= threshold)
        if not len(small):
            break
        if len(displacements) == 32:
            raise ValueError("fragment cleanup exceeds bounded work")
        triangle = triangles[small[0]]
        lengths = np.linalg.norm(triangle - np.roll(triangle, -1, axis=0), axis=1)
        if max(lengths) > tolerance:
            raise ValueError("small face exceeds local fragment tolerance")
        edge = int(np.argmin(lengths))
        a, b = faces[small[0], [edge, (edge + 1) % 3]]
        if a in used or b in used:
            raise ValueError("repeated collapse could accumulate displacement")
        around_a, around_b = np.any(faces == a, axis=1), np.any(faces == b, axis=1)
        shared = around_a & around_b
        neighbors_a = set(faces[around_a].ravel()) - {a, b}
        neighbors_b = set(faces[around_b].ravel()) - {a, b}
        opposite = set(faces[shared].ravel()) - {a, b}
        if (
            sum(shared) != 2
            or len(opposite) != 2
            or neighbors_a & neighbors_b != opposite
        ):
            raise ValueError("fragment edge violates manifold link condition")
        changed = around_b & ~shared
        replacement = faces[changed].copy()
        replacement[replacement == b] = a
        points = vertices[replacement]
        new_cross = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        if np.any(np.sum(new_cross * cross[changed], axis=1) <= 0):
            raise ValueError("fragment collapse would invert a face")
        faces[faces == b] = a
        faces = faces[~shared]
        used.update((a, b))
        displacements.append(float(lengths[edge]))
    indices, inverse = np.unique(faces, return_inverse=True)
    cleaned = manifold.Mesh64(
        vertices[indices], inverse.reshape(-1, 3).astype(np.uint64)
    )
    if manifold.Manifold(cleaned).status() != manifold.Error.NoError:
        raise ValueError("fragment cleanup produced invalid topology")
    return cleaned, {
        "collapsed_fragment_edges": len(displacements),
        "maximum_collapse_m": max(displacements, default=0.0),
        "collapse_tolerance_m": tolerance,
    }
