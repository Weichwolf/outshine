"""Diagnostic water cuts in cell-relative Mercator coordinates (#963).

This is a bounded triangle soup, not a welded render asset or a native schema.
The caller supplies immutable, physically admitted ECEF roots. Missing roots
refuse the entire cell. Source membership uses projected Mercator square metres,
not surface area on Earth. No local height fit, snapping or normal reconstruction.
"""

import hashlib
from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.places.sources import tile_tuple
from river_membership import face_membership
from water_cell_mask import local_sources, root_normal
from water_nodal_height import cell_faces, parameter_triangle, sample_face


@dataclass(frozen=True)
class ParameterCut:
    cell: tuple
    context: str
    source_keys: tuple
    parameter: np.ndarray
    world: np.ndarray
    parents: np.ndarray
    normals: np.ndarray
    membership: object
    report: dict


def cut_cell(
    cell,
    polygons,
    source_keys,
    facet,
    *,
    context,
    face_limit=2048,
    coordinate_limit=32768,
    refine_interior=False,
):
    """facet(keys) -> (state, ECEF vertices); only 'admitted' roots are usable.

    Input/output counts and requested intersections are bounded. GEOS workspace,
    callback work and allocator peaks are measured by the driver, not bounded by
    the face count. No partial output is published on a missing/invalid root.
    """
    cell = tile_tuple(cell)
    if not isinstance(context, str) or not context:
        raise ValueError("immutable parent context required")
    if (
        type(face_limit) is not int
        or not 0 < face_limit <= 2048
        or type(coordinate_limit) is not int
        or not 0 < coordinate_limit <= 32768
    ):
        raise ValueError("bounded integer work limits required")
    local, mask = local_sources(cell, polygons, source_keys, coordinate_limit)
    parameter, world, parents, normals = [], [], [], []
    work = {
        "root_intersections": 0,
        "root_requests": 0,
        "cut_coordinates": 0,
        "maximum_root_plane_residual_m": 0.0,
    }
    for index, keys in enumerate(cell_faces(cell)):
        triangle = parameter_triangle(keys) - cell[1:]
        cut = shapely.Polygon(triangle).intersection(mask)
        work["root_intersections"] += 1
        if cut.area <= 0:
            continue
        work["root_requests"] += 1
        state, root = facet(keys)
        if state != "admitted":
            return None, dict(work, state=state, refused_root=keys)
        root, normal = root_normal(root)
        work["cut_coordinates"] += int(shapely.get_num_coordinates(cut))
        if work["cut_coordinates"] > coordinate_limit:
            raise ValueError("cut coordinate budget exhausted")
        for polygon in shapely.get_parts(cut):
            if polygon.geom_type != "Polygon" or polygon.area <= 0:
                continue
            pieces = shapely.get_parts(shapely.constrained_delaunay_triangles(polygon))
            if (
                len(parameter) + len(pieces) * (3 if refine_interior else 1)
                > face_limit
            ):
                raise ValueError("fine face budget exhausted")
            for part in pieces:
                xy = np.asarray(part.exterior.coords)[:3]
                a, b = xy[1:] - xy[0]
                signed_area = a[0] * b[1] - a[1] * b[0]
                if signed_area == 0:
                    raise ValueError("degenerate fine triangle")
                if signed_area > 0:
                    xy = xy[[0, 2, 1]]
                children = [xy]
                if refine_interior:
                    middle = xy.mean(axis=0)
                    children = [
                        np.array([xy[i], xy[(i + 1) % 3], middle]) for i in range(3)
                    ]
                for child in children:
                    xyz = sample_face(keys, root, child, origin=cell[1:])
                    work["maximum_root_plane_residual_m"] = max(
                        work["maximum_root_plane_residual_m"],
                        float(np.abs((xyz - root[0]) @ normal).max()),
                    )
                    parameter.append(child)
                    world.append(xyz)
                    parents.append(index)
                    normals.append(normal)
    if not parameter:
        raise ValueError("positive covered water area required")
    parameter, world = np.asarray(parameter), np.asarray(world)
    parents, normals = np.asarray(parents, dtype=np.uint32), np.asarray(normals)
    membership, coverage = audit_cut(cell, parameter, local, mask)
    report = dict(
        work,
        state="diagnostic_cut",
        faces=len(parameter),
        unshared_vertices=3 * len(parameter),
        coverage=coverage,
        mask_digest=hashlib.sha256(shapely.normalize(mask).wkb).hexdigest(),
        membership=membership.work,
        numerical_array_bytes=sum(
            a.nbytes for a in (parameter, world, parents, normals)
        )
        + membership.work["output_array_bytes"],
        known_array_materialization_bytes=sum(
            a.nbytes for a in (parameter, world, parents, normals)
        ),
        membership_parameter_copy_bytes=parameter.nbytes,
        membership_index_materialization_bytes=3 * len(parameter) * 4,
        geometry_workspace_bytes=None,
    )
    return ParameterCut(
        cell,
        context,
        tuple(source_keys),
        parameter,
        world,
        parents,
        normals,
        membership,
        report,
    ), report


def audit_cut(cell, triangles, local, mask):
    scale = 2 * np.pi * 6378137 / 2 ** cell[0]
    faces = np.arange(3 * len(triangles), dtype=np.uint32).reshape(-1, 3)
    scaled = [
        shapely.affinity.scale(p, xfact=scale, yfact=scale, origin=(0, 0))
        for p in local
    ]
    membership = face_membership(triangles.reshape(-1, 2) * scale, faces, scaled)
    if np.any(membership.status):
        raise ValueError("incomplete fine source membership")
    pieces = shapely.polygons(triangles)
    union = shapely.union_all(pieces)
    coverage = {
        "area_units": "projected Mercator square metres, not Earth surface area",
        "missing": mask.difference(union).area * scale**2,
        "outside": union.difference(mask).area * scale**2,
        "overlap": max(0.0, float(shapely.area(pieces).sum() - union.area)) * scale**2,
        "source_uncovered": float(membership.uncovered_area_m2.max()),
        "tolerance": max(1e-8, mask.area * scale**2 * 1e-12),
    }
    if any(
        coverage[key] > coverage["tolerance"]
        for key in ("missing", "outside", "overlap", "source_uncovered")
    ):
        raise ValueError("fine mask coverage failed")
    return membership, coverage
