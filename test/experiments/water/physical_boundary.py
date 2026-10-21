"""One bounded canonical cut, before placement into independently loaded frames."""

from dataclasses import dataclass

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import axes, ecef


def to_world(anchor, points):
    return points @ axes(anchor[1], anchor[0]) + ecef(anchor[1], anchor[0], 0)


def from_world(anchor, points):
    return (points - ecef(anchor[1], anchor[0], 0)) @ axes(anchor[1], anchor[0]).T


def rotation(source, destination):
    return axes(source[1], source[0]) @ axes(destination[1], destination[0]).T


def signed_twice_area(vertices, faces):
    edges = vertices[faces, :2][:, 1:] - vertices[faces, :2][:, :1]
    return edges[:, 0, 0] * edges[:, 1, 1] - edges[:, 0, 1] * edges[:, 1, 0]


@dataclass(frozen=True)
class Cut:
    vertices: np.ndarray
    faces: tuple
    parents: tuple
    seam: np.ndarray
    crossing_edges: np.ndarray
    crossing_t: np.ndarray
    original_vertices: int
    plane_x: float


def canonical_cut(mesh, plane_x, *, boundary_limit=1024):
    """Clip triangles by x=plane_x with edge-keyed intersections, without welding.

    Both halves share the exact same indexed affine subedges. The finite input
    and boundary caps are experiment budgets, not global river partitions.
    Refusal invalidates this local cut; callers keep other valid products.
    """
    if not isinstance(mesh, Mesh):
        raise TypeError("owned Mesh required")
    if type(boundary_limit) is not int or not 0 < boundary_limit <= 8192:
        raise ValueError("explicit bounded positive boundary budget required")
    if len(mesh.vertices) > 8192 or len(mesh.triangles) > 16384:
        raise ValueError("cut_input_budget")
    if not np.isfinite(plane_x) or not len(mesh.triangles):
        raise ValueError("finite plane and nonempty mesh required")
    original_area = signed_twice_area(mesh.vertices, mesh.triangles)
    if np.any(original_area == 0) or not np.isfinite(original_area).all():
        raise ValueError("degenerate_input")
    if np.any(original_area < 0):
        raise ValueError("clockwise_input")
    vertices = list(mesh.vertices.copy())
    crossings, interpolation, weights = {}, [], []
    faces, parents = ([], []), ([], [])

    def intersection(i, j):
        a, b = sorted((i, j))
        if vertices[a][0] == plane_x:
            return a
        if vertices[b][0] == plane_x:
            return b
        key = (a, b)
        if key not in crossings:
            if len(crossings) >= boundary_limit:
                raise ValueError("cut_boundary_budget")
            t = (plane_x - vertices[a][0]) / (vertices[b][0] - vertices[a][0])
            if not 0 < t < 1:
                raise ValueError("unrepresentable_cut")
            point = (1 - t) * vertices[a] + t * vertices[b]
            point[0] = plane_x
            crossings[key] = len(vertices)
            vertices.append(point)
            interpolation.append(key)
            weights.append(t)
        return crossings[key]

    for parent, triangle in enumerate(mesh.triangles):
        for side, sign in enumerate((-1, 1)):
            polygon = []
            for j, current in enumerate(triangle):
                previous = triangle[j - 1]
                inside = sign * (vertices[current][0] - plane_x) >= 0
                before = sign * (vertices[previous][0] - plane_x) >= 0
                if inside != before:
                    polygon.append(intersection(int(previous), int(current)))
                if inside:
                    polygon.append(int(current))
            polygon = list(dict.fromkeys(polygon))
            for j in range(1, len(polygon) - 1):
                faces[side].append([polygon[0], polygon[j], polygon[j + 1]])
                parents[side].append(parent)
    vertices = np.asarray(vertices)
    faces = tuple(np.asarray(f, dtype=np.uint32).reshape(-1, 3) for f in faces)
    parents = tuple(np.asarray(p, dtype=np.uint32) for p in parents)
    seam_sets = []
    for side in range(2):
        if not len(faces[side]):
            raise ValueError("empty_cut_half")
        area = signed_twice_area(vertices, faces[side])
        if np.any(area * np.sign(original_area[parents[side]]) <= 0):
            raise ValueError("degenerate_cut")
        edges = faces[side][:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
        at_plane = np.all(vertices[edges, 0] == plane_x, axis=1)
        seam_sets.append(np.unique(np.sort(edges[at_plane], axis=1), axis=0))
    if not np.array_equal(*seam_sets) or not len(seam_sets[0]):
        raise ValueError("nonconforming_cut")
    if len(np.unique(seam_sets[0])) > boundary_limit:
        raise ValueError("cut_boundary_budget")
    return Cut(
        vertices,
        faces,
        parents,
        seam_sets[0],
        np.asarray(interpolation, dtype=np.uint32).reshape(-1, 2),
        np.asarray(weights),
        len(mesh.vertices),
        float(plane_x),
    )


def cut_values(cut, vertices):
    """Apply the same affine cut to a second field on the original topology."""
    vertices = np.asarray(vertices)
    if vertices.shape != (cut.original_vertices, 3):
        raise ValueError("same original vertex table required")
    pair = vertices[cut.crossing_edges]
    t = cut.crossing_t[:, None]
    return np.concatenate((vertices, (1 - t) * pair[:, 0] + t * pair[:, 1]))


def local_piece(cut, side, body_anchor, anchor, *, vertices=None):
    ids = np.unique(cut.faces[side])
    canonical = cut.vertices if vertices is None else vertices
    mesh = Mesh(
        from_world(anchor, to_world(body_anchor, canonical[ids])),
        np.searchsorted(ids, cut.faces[side]).astype(np.uint32),
    )
    seam = np.searchsorted(ids, cut.seam).astype(np.uint32)
    if not np.array_equal(ids[seam], cut.seam):
        raise ValueError("missing_boundary_vertex")
    if np.any(signed_twice_area(mesh.vertices, mesh.triangles) <= 0):
        raise ValueError("local_projection_fold_or_roundoff")
    return mesh, ids, seam


def child_rows(rows, parents, body_anchor, anchor, original_faces):
    """Preserve all original facet constraints and the two explicit primary rows."""
    owner = rows[:, 0].astype(int)
    order = np.argsort(owner, kind="stable")
    offsets = np.r_[0, np.cumsum(np.bincount(owner, minlength=original_faces))]
    gathered, children, primary = [], [], []
    for child, parent in enumerate(parents):
        indices = order[offsets[parent] : offsets[parent + 1]]
        begin = len(gathered)
        primary.append(
            [
                begin + int(np.flatnonzero(indices == parent)[0]),
                begin + int(np.flatnonzero(indices == original_faces + parent)[0]),
            ]
        )
        gathered.extend(indices)
        children.extend([child] * len(indices))
    result = rows[gathered].copy()
    result[:, 0] = children
    result[:, 1:4] = result[:, 1:4] @ rotation(body_anchor, anchor)
    return result, np.asarray(primary), np.asarray(gathered)


def boundary_error(left, left_anchor, left_edges, right, right_anchor, right_edges):
    """Endpoint convexity bounds every point of the matching straight subedges."""
    if left_edges.shape != right_edges.shape or not len(left_edges):
        raise ValueError("matching nonempty canonical edge table required")
    a = to_world(left_anchor, left.vertices[left_edges])
    b = to_world(right_anchor, right.vertices[right_edges])
    samples = np.linspace(0, 1, 17)[None, :, None]
    pa = (1 - samples) * a[:, :1] + samples * a[:, 1:]
    pb = (1 - samples) * b[:, :1] + samples * b[:, 1:]
    return {
        "segments": len(a),
        "endpoint_max_m": float(np.linalg.norm(a - b, axis=2).max()),
        "whole_segment_sample_max_m": float(np.linalg.norm(pa - pb, axis=2).max()),
        "samples": int(np.prod(pa.shape[:2])),
    }
