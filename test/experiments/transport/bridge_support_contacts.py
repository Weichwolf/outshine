"""Whole finite contact prisms between explicitly chosen piecewise affine surfaces."""

import numpy as np
import shapely
from ground_surface_showcase import sample_vertices
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, conform_triangles, vertical_clearance
from outshine_ref.transport.surface import _sorted_boundary
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


class ContactSurface:
    def __init__(self, mesh, *, maximum=262144):
        if not 0 < len(mesh.triangles) <= maximum:
            raise ValueError("support contact surface face budget")
        self.mesh = mesh
        self.shapes = shapely.polygons(mesh.vertices[mesh.triangles, :2])
        self.tree = shapely.STRtree(self.shapes)

    def patch(self, footprint):
        hits = self.tree.query(footprint, predicate="intersects")
        if not 0 < len(hits) <= 2048:
            raise ValueError("support contact patch absent or face budget")
        pieces = shapely.intersection(self.shapes[hits], footprint)
        pieces = pieces[shapely.area(pieces) > 0]
        covered = shapely.union_all(pieces)
        if footprint.difference(covered).area > 1e-8:
            raise ValueError("support footprint outside selected contact")
        used, inverse = np.unique(self.mesh.triangles[hits], return_inverse=True)
        patch = Mesh(self.mesh.vertices[used], inverse.reshape(-1, 3))
        return patch, pieces, len(hits)


def contact_prism(footprint, lower, upper, *, minimum_height=2.0):
    if (
        footprint.geom_type != "Polygon"
        or not footprint.is_valid
        or footprint.area <= 0
        or shapely.get_num_coordinates(footprint) > 64
        or not np.isfinite(minimum_height)
        or minimum_height <= 0
    ):
        raise ValueError("bounded valid support polygon and positive height required")
    bottom, _bottom_pieces, low_count = lower.patch(footprint)
    top, _top_pieces, high_count = upper.patch(footprint)
    # Node original edges together once. Independently clipped face boundaries
    # introduced nearly coincident points at the same footprint edge in Vienna.
    lines = np.r_[
        contact_edges(bottom),
        contact_edges(top),
        [footprint.boundary],
    ]
    noded = shapely.get_parts(shapely.union_all(lines))
    if len(noded) > 8192:
        raise ValueError("support contact arrangement segment budget")
    cells = shapely.get_parts(shapely.polygonize(noded))
    cells = cells[shapely.covers(footprint, shapely.point_on_surface(cells))]
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(cells))
    if not 0 < len(triangles) <= 4096:
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
        xy, faces, max_faces=4096, max_candidates=65536
    )
    lower_xyz = np.c_[xy, sample_vertices(SurfaceSampler(bottom), xy)[:, 2]]
    upper_xyz = np.c_[xy, sample_vertices(SurfaceSampler(top), xy)[:, 2]]
    heights = upper_xyz[:, 2] - lower_xyz[:, 2]
    if not np.isfinite(heights).all() or heights.min() < minimum_height:
        raise ValueError("support insufficient whole contact height")
    count = len(xy)
    a, b = _sorted_boundary(faces).T
    walls = np.stack(
        (np.c_[a, a + count, b], np.c_[b, a + count, b + count]), axis=1
    ).reshape(-1, 3)
    result = Mesh(
        np.vstack((upper_xyz, lower_xyz)),
        np.vstack((faces, faces[:, ::-1] + count, walls)),
    )
    lower_contact, upper_contact = Mesh(lower_xyz, faces), Mesh(upper_xyz, faces)
    error = surface_domain(lower_contact).symmetric_difference(footprint).area
    if error > 1e-8 or audit_closed(result) <= 0:
        raise ValueError("support contact coverage or oriented volume failed")
    return (
        result,
        (lower_contact, upper_contact),
        {
            "minimum_height_m": float(heights.min()),
            "maximum_height_m": float(heights.max()),
            "contact_faces": len(faces),
            "source_face_candidates": low_count + high_count,
            "noded_segments": len(noded),
            "coverage_error_m2": error,
            "conformance": conformance,
            "volume_m3": audit_closed(result),
        },
    )


def contact_edges(mesh):
    edges = mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    return shapely.linestrings(mesh.vertices[edges, :2])


def replay_contact(actual, reference):
    patch, _pieces, _count = reference.patch(surface_domain(actual))
    forward = vertical_clearance(actual, patch, max_faces=4096, seconds=2)
    backward = vertical_clearance(patch, actual, max_faces=4096, seconds=2)
    if not all(
        row["valid"] and abs(row["minimum_clearance_m"]) <= 1e-7
        for row in (forward, backward)
    ):
        raise ValueError("support whole contact differs from selected surface")
    return max(abs(r["minimum_clearance_m"]) for r in (forward, backward))
