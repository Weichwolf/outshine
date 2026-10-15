"""Shared contact arrangement, exact selected affine heights and directed shell."""

import numpy as np
import shapely

from ...contracts import Mesh
from ...geometry import SurfaceSampler, conform_triangles
from ..surface import _sorted_boundary
from ._types import SupportVolume


def edges(mesh):
    values = mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    values = np.unique(np.sort(values, axis=1), axis=0)
    return shapely.linestrings(mesh.vertices[values, :2])


def arrangement(footprint, lower, upper, budget):
    lines = np.r_[edges(lower), edges(upper), [footprint.boundary]]
    noded = shapely.get_parts(shapely.union_all(lines))
    if len(noded) > budget.maximum_segments:
        raise ValueError("support contact arrangement segment budget")
    cells = shapely.get_parts(shapely.polygonize(noded))
    cells = cells[shapely.covers(footprint, shapely.point_on_surface(cells))]
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(cells))
    if not 0 < len(triangles) <= budget.maximum_contact_faces:
        raise ValueError("support contact arrangement face budget")
    xy, ids = np.unique(
        shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3].reshape(-1, 2),
        axis=0,
        return_inverse=True,
    )
    faces = ids.reshape(-1, 3)
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    cross = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    if np.any(cross == 0):
        raise ValueError("support contact degenerate projected face")
    faces[cross < 0] = faces[cross < 0, ::-1]
    faces, conformance = conform_triangles(
        xy,
        faces,
        max_faces=budget.maximum_contact_faces,
        max_candidates=budget.maximum_interface_candidates,
    )
    return xy, faces, {"noded_segments": len(noded), "conformance": conformance}


def heights(mesh, xy, budget):
    sampler = SurfaceSampler(mesh)
    points = np.empty(len(xy))
    remaining = budget.maximum_sample_candidates
    for begin in range(0, len(xy), 4096):
        stop = min(len(xy), begin + 4096)
        sampled = sampler.sample(
            xy[begin:stop], max_queries=4096, max_candidates=remaining
        )
        if any(sampled.reasons):
            reasons = sorted({r for r in sampled.reasons if r})
            raise ValueError("support contact unresolved: " + ", ".join(reasons))
        remaining -= sampled.report["candidates"]
        points[begin:stop] = sampled.points_enu_m[:, 2]
    return np.c_[xy, points]


def close(lower, upper, faces):
    count = len(lower)
    a, b = _sorted_boundary(faces).T
    walls = np.stack(
        (np.c_[a, a + count, b], np.c_[b, a + count, b + count]), axis=1
    ).reshape(-1, 3)
    mesh = Mesh(
        np.vstack((upper, lower)), np.vstack((faces, faces[:, ::-1] + count, walls))
    )
    triangles = mesh.vertices[mesh.triangles] - mesh.vertices[0]
    volume = float(
        np.einsum(
            "ij,ij->i", triangles[:, 0], np.cross(triangles[:, 1], triangles[:, 2])
        ).sum()
        / 6
    )
    if not np.isfinite(volume) or volume <= 0:
        raise ValueError("support closed oriented volume invalid")
    return SupportVolume(mesh, Mesh(lower, faces), Mesh(upper, faces)), volume
