"""Bounded local render cuts of one immutable functional water owner (#963).

This experiment keeps the owner's frame, height planes and normals. It does not
fit a local height field or define a native asset/GPU contract. Required contacts
come from the caller's explicit cohort; unknown outer cells stay unknown there.
"""

import hashlib
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from river_membership import face_membership
from water_field_difference import height
from water_shared_borders import adjacent_cells


@dataclass(frozen=True)
class RegionMesh:
    mesh: Mesh
    parent_faces: np.ndarray
    parent_normals: np.ndarray
    membership: object
    source_keys: tuple
    contacts: dict
    context: str
    report: dict


def boundary_edges(faces):
    edges = np.sort(
        np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1
    )
    unique, count = np.unique(edges, axis=0, return_counts=True)
    if np.any(count > 2):
        raise ValueError("nonmanifold local cut")
    return unique[count == 1]


def validate_sources(owner, polygons, source_keys, coordinate_limit):
    if not 0 < len(polygons) <= 1024 or len(polygons) != len(source_keys):
        raise ValueError(
            "bounded source polygons and corresponding identities required"
        )
    if (
        shapely.get_num_coordinates(polygons).sum() > coordinate_limit
        or any(
            p.geom_type not in ("Polygon", "MultiPolygon")
            or not p.is_valid
            or p.is_empty
            or not 0 < p.area < np.inf
            for p in polygons
        )
        or not np.isfinite(shapely.get_coordinates(polygons)).all()
    ):
        raise ValueError("bounded finite valid source polygons required")
    mask = shapely.union_all(polygons)
    outside = mask.difference(owner.mask).area
    # Same numerical coverage criterion as surface_mesh/compare_fields; no snap,
    # mask erosion, changed height/normal bound or exact-topology claim.
    if outside > max(1e-8, mask.area * 1e-12):
        raise ValueError("local sources extend outside the authoritative water mask")
    return mask, outside


def cut_faces(owner, mask, candidate_limit, face_limit, refine_interior):
    candidates = np.sort(owner.tree.query(mask))
    if len(candidates) > candidate_limit:
        raise ValueError("local root intersection budget exhausted")
    faces, parents = [], []
    coordinates = 0
    for parent in candidates:
        cut = mask.intersection(owner.polygons[parent])
        coordinates += int(shapely.get_num_coordinates(cut))
        if coordinates > 6 * face_limit:
            raise ValueError("local cut coordinate budget exhausted")
        for polygon in shapely.get_parts(cut):
            if polygon.geom_type != "Polygon" or polygon.area <= 0:
                continue
            # CDT is local to a single root facet; a triangle cannot bridge a crease.
            pieces = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
            if len(faces) + len(pieces) * (3 if refine_interior else 1) > face_limit:
                raise ValueError("local render face budget exhausted")
            for part in pieces:
                if part.area <= 0:
                    raise ValueError("degenerate local triangulation")
                xy = np.asarray(part.exterior.coords)[:3]
                triangles = [xy]
                if refine_interior:
                    middle = xy.mean(axis=0)
                    triangles = [
                        np.array([xy[i], xy[(i + 1) % 3], middle]) for i in range(3)
                    ]
                faces.extend(triangles)
                parents.extend([int(parent)] * len(triangles))
    if not faces:
        raise ValueError("local water has no covered positive-area render cut")
    return np.asarray(faces), np.asarray(parents, dtype=np.uint32), len(candidates)


def validate_contacts(owner, cell, required_edges, profiles):
    if (
        len(required_edges) > 4
        or len(set(required_edges)) != len(required_edges)
        or set(required_edges) != set(profiles)
    ):
        raise ValueError("missing or extra declared local contact")
    for edge in sorted(required_edges):
        if len(edge) != 4 or edge[1] not in (0, 1) or cell not in adjacent_cells(edge):
            raise ValueError("contact identity does not belong to this cell")
        profile = profiles[edge]
        if profile.context != owner.context:
            raise ValueError("stale or different functional owner contact")
        if (
            profile.points.ndim != 2
            or profile.points.shape[1] != 3
            or not 0 < len(profile.points) <= 16384
            or profile.segments.ndim != 2
            or profile.segments.shape[1] != 2
            or not 0 < len(profile.segments) <= 8192
            or profile.segments.dtype.kind not in "ui"
            or np.any(profile.segments >= len(profile.points))
            or np.any(profile.segments < 0)
            or not np.isfinite(profile.points).all()
        ):
            raise ValueError("bounded nonempty complete contact required")
        digest = hashlib.sha256(owner.context.encode())
        for array in (profile.points, profile.segments, profile.parent_faces):
            digest.update(np.ascontiguousarray(array).tobytes())
        if digest.hexdigest() != profile.report.get("profile_content_digest"):
            raise ValueError("contact content digest mismatch")


def bind_contacts(profiles, mesh, boundary):
    lookup = {tuple(point): index for index, point in enumerate(mesh.vertices[:, :2])}
    boundary = set(map(tuple, boundary))
    result = {}
    for edge, profile in sorted(profiles.items()):
        ids = [lookup.get(tuple(point)) for point in profile.points[:, :2]]
        if any(index is None for index in ids):
            raise ValueError("complete profile vertex absent from local cut")
        ids = np.asarray(ids, dtype=np.uint32)
        if not np.array_equal(mesh.vertices[ids], profile.points):
            raise ValueError("local contact height differs from the functional owner")
        segments = ids[profile.segments]
        if any(
            tuple(sorted(map(int, segment))) not in boundary for segment in segments
        ):
            raise ValueError("complete profile interval is not a local boundary edge")
        segments.setflags(write=False)
        result[edge] = segments
    return result


def validate_limits(cell, values):
    if (
        len(cell) != 3
        or any(type(n) is not int for n in cell)
        or not 0 <= cell[0] <= 30
        or any(not 0 <= n < 2 ** cell[0] for n in cell[1:])
        or any(
            type(value) is not int or not 1 <= value <= maximum
            for value, maximum in values
        )
    ):
        raise ValueError(
            "valid owned cell and bounded positive regional work limits required"
        )


def coverage(mesh, mask):
    polygons = shapely.polygons(mesh.vertices[mesh.triangles, :2])
    union = shapely.union_all(polygons)
    missing, outside = mask.difference(union).area, union.difference(mask).area
    overlap = float(shapely.area(polygons).sum() - union.area)
    if max(missing, outside, abs(overlap)) > max(1e-8, mask.area * 1e-12):
        raise ValueError("local render cut loses precise mask coverage or holes")
    return missing, outside, overlap


def cook_region(
    owner,
    polygons,
    *,
    cell,
    source_keys,
    required_edges,
    profiles,
    refine_interior=False,
    candidate_limit=16384,
    face_limit=16384,
    vertex_limit=8192,
    coordinate_limit=32768,
):
    """Produce an entire local cut, or refuse without mutating the owner.

    Bounds cap emitted numerical products and requested intersections. A GEOS
    query/overlay/CDT can allocate its workspace before these output checks; this
    is not a hard allocator or native memory bound. Source overlap CSR is retained.
    """
    limits = (
        (candidate_limit, 16384),
        (face_limit, 16384),
        (vertex_limit, 8192),
        (coordinate_limit, 32768),
    )
    validate_limits(cell, limits)
    if type(refine_interior) is not bool:
        raise ValueError("explicit boolean interior refinement required")
    polygons, source_keys, required_edges = (
        tuple(polygons),
        tuple(source_keys),
        tuple(required_edges),
    )
    validate_contacts(owner, cell, required_edges, profiles)
    mask, source_outside = validate_sources(
        owner, polygons, source_keys, coordinate_limit
    )
    xy_faces, parents, candidates = cut_faces(
        owner, mask, candidate_limit, face_limit, refine_interior
    )
    xy, indices = np.unique(xy_faces.reshape(-1, 2), axis=0, return_inverse=True)
    if len(xy) > vertex_limit:
        raise ValueError("local render vertex budget exhausted")
    faces = indices.reshape(-1, 3).astype(np.uint32)
    e = xy[faces[:, 1:]] - xy[faces[:, :1]]
    determinant = e[:, 0, 0] * e[:, 1, 1] - e[:, 0, 1] * e[:, 1, 0]
    if np.any(determinant == 0):
        raise ValueError("unrepresentable degenerate local triangle")
    faces[determinant < 0] = faces[determinant < 0][:, [0, 2, 1]]
    samples = owner.sampler.sample(xy, max_queries=8192, max_candidates=1000000)
    if any(samples.reasons):
        raise ValueError("local cut vertex lacks an authoritative height")
    mesh = Mesh(samples.points_enu_m, faces)
    expected = np.array(
        [
            height(owner.vertices, owner.gradient, int(p), mesh.vertices[f, :2])
            for p, f in zip(parents, faces, strict=True)
        ]
    )
    up_error = float(abs(expected - mesh.vertices[faces, 2]).max())
    if up_error > 1e-9:
        raise ValueError("local render facet crosses an authoritative crease")
    missing, outside, overlap = coverage(mesh, mask)
    contacts = bind_contacts(profiles, mesh, boundary_edges(faces))
    csr = face_membership(
        mesh.vertices, faces, polygons, source_coordinate_limit=coordinate_limit
    )
    if np.any(csr.status != 0):
        raise ValueError("local render cut lacks complete source overlap provenance")
    normals = np.c_[-owner.gradient[parents], np.ones(len(parents))]
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    for array in (parents, normals):
        array.setflags(write=False)
    digest = hashlib.sha256(owner.context.encode())
    for array in (mesh.vertices, mesh.triangles, parents):
        digest.update(array.tobytes())
    return RegionMesh(
        mesh,
        parents,
        normals,
        csr,
        source_keys,
        contacts,
        owner.context,
        {
            "cell": cell,
            "vertices": len(xy),
            "faces": len(faces),
            "root_candidates": candidates,
            "all_root_faces": len(owner.polygons),
            "source_coordinates": int(shapely.get_num_coordinates(polygons).sum()),
            "source_parts": len(polygons),
            "source_membership": csr.work,
            "sampling": dict(samples.report),
            "maximum_parent_plane_up_difference_m": up_error,
            "uncovered_area_m2": missing,
            "outside_area_m2": outside,
            "overlap_area_m2": overlap,
            "mask_area_m2": mask.area,
            "source_outside_owner_mask_area_m2": source_outside,
            "coverage_area_tolerance_m2": max(1e-8, mask.area * 1e-12),
            "required_contacts": len(contacts),
            "contact_segments": sum(len(a) for a in contacts.values()),
            "numeric_payload_bytes": mesh.vertices.nbytes
            + mesh.triangles.nbytes
            + parents.nbytes
            + normals.nbytes
            + csr.work["output_array_bytes"]
            + sum(a.nbytes for a in contacts.values()),
            "known_final_mesh_constructor_copied_bytes": mesh.vertices.nbytes
            + mesh.triangles.nbytes,
            "total_copied_bytes": None,
            "geos_workspace_peak_bytes": None,
            "content_digest": digest.hexdigest(),
            "scope": "common-frame root-height render cut with inherited root normals; no local physical fit, cross-cohort owner, native asset contract or DB-IO saving proved",
        },
    )
