"""A finite actual cap/riser chain along an existing route; no walking-body model."""

from collections import defaultdict
from itertools import pairwise

import numpy as np
from outshine_ref.contracts import Mesh
from stair_section_points import SectionPoints


def _candidates(mesh, axis, method):
    work = {"input_faces": len(mesh.triangles), "broadphase_coordinate_bytes": 0}
    if method == "scan":
        ids = np.arange(len(mesh.triangles))
    else:
        xy = mesh.vertices[mesh.triangles, :2]
        low, high = xy.min(axis=1), xy.max(axis=1)
        selected = np.all(high >= axis.min(axis=0), axis=1) & np.all(
            low <= axis.max(axis=0), axis=1
        )
        ids = np.flatnonzero(selected)
        work["broadphase_coordinate_bytes"] = xy.nbytes + low.nbytes + high.nbytes
    work.update(
        candidate_faces=len(ids),
        selected_face_index_bytes=ids.nbytes,
        selected_face_copy_bytes=len(ids) * 3 * mesh.triangles.dtype.itemsize,
    )
    return ids, work


def _order(segments, points):
    adjacent = defaultdict(set)
    for a, b in segments:
        adjacent[a].add(b)
        adjacent[b].add(a)
    ends = [i for i, neighbors in adjacent.items() if len(neighbors) == 1]
    if len(ends) != 2 or any(len(neighbors) > 2 for neighbors in adjacent.values()):
        raise ValueError("route section is not one unbranched chain")
    current = min(ends, key=lambda i: (points.parameters[i], points.xyz[i][2]))
    previous, order = None, []
    while True:
        order.append(current)
        following = adjacent[current] - ({previous} if previous is not None else set())
        if not following:
            break
        previous, current = current, next(iter(following))
        if len(order) > len(adjacent):
            raise ValueError("route section contains a cycle")
    if len(order) != len(adjacent):
        raise ValueError("route section has disconnected components")
    if points.parameters[order[0]] != 0 or points.parameters[order[-1]] != 1:
        raise ValueError("route section does not reach both requested endpoints")
    return order


def trace_section(mesh, axis, *, method="bounds", max_faces=4096):
    if not isinstance(mesh, Mesh) or method not in ("scan", "bounds"):
        raise ValueError("owned Mesh and known section candidate method required")
    if type(max_faces) is not int or not 0 <= max_faces <= 4096:
        raise ValueError("bounded nonnegative route-section face cap required")
    if len(mesh.triangles) > max_faces:
        raise ValueError("route-section face budget exceeded before exact work")
    axis = np.asarray(axis, dtype=float)
    if (
        axis.shape != (2, 2)
        or not np.isfinite(axis).all()
        or np.array_equal(axis[0], axis[1])
        or abs(axis).max() > 1e7
        or abs(mesh.vertices).max(initial=0) > 1e7
    ):
        raise ValueError("finite distinct bounded local XY endpoints required")
    ids, work = _candidates(mesh, axis, method)
    faces = mesh.triangles[ids]
    used = np.unique(faces)
    points = SectionPoints(mesh.vertices, used, axis)
    segments = defaultdict(list)
    for face, face_id in zip(faces, ids, strict=True):
        segment = points.face(face)
        if segment is not None:
            segments[segment].append(int(face_id))
    order = _order(segments, points)
    xyz = np.array([[float(value) for value in points.xyz[i]] for i in order])
    parameters = np.array([float(points.parameters[i]) for i in order])
    arclength = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xyz, axis=0), axis=1))]
    provenance = [segments[tuple(sorted((a, b)))] for a, b in pairwise(order)]
    return {
        "xyz_m": xyz,
        "axis_parameter": parameters,
        "arclength_m": arclength,
        "segment_face_ids": provenance,
        "work": dict(
            work,
            exact_vertices=len(used),
            face_edge_visits=points.edge_queries,
            exact_edge_constructions=points.edge_constructions,
            exact_cut_points=len(points.xyz),
            output_points=len(order),
            output_array_bytes=xyz.nbytes + parameters.nbytes + arclength.nbytes,
            rounded_zero_segments=int(
                np.sum(np.all(np.diff(xyz, axis=0) == 0, axis=1))
            ),
            repeated_arclength_values=int(np.sum(np.diff(arclength) == 0)),
        ),
        "scope": "Exact binary-input plane/edge intersections and finite endpoint clipping; mesh identities define one cap/riser chain. Float64 output rounds the exact coordinates; consecutive rounded aliases remain distinct and are counted. Explicit face selection belongs to the caller. No foot volume, headroom, locomotion or outside-route admission.",
    }
