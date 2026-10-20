"""Independent coverage checks and private, self-contained consumer inputs (#941)."""

import hashlib

import numpy as np
import shapely
from outshine_ref.water._fit import grade_envelopes
from physical_boundary import from_world, to_world
from river_membership import face_membership


def inherited_envelope(vertices, triangles, parents, parent_caps, rows, tangent):
    """Reuse each parent's original normal cap, including its constant direction.

    A ball intersected with the Up slab is convex. Containment of all three
    reconstructed child corners therefore contains the whole straight facet.
    The original cap bounds Up throughout this same ball/slab by the existing
    normal-derivative argument; no new centre projection changes the direction.
    """
    points = vertices[triangles]
    ball_excess = float(
        (
            np.linalg.norm(points - parent_caps["centre_enu_m"][parents, None], axis=2)
            - parent_caps["radius_m"][parents, None]
        ).max()
    )
    low, high = parent_caps["up_bounds_m"]
    box_excess = max(
        float(low - points[:, :, 2].min()), float(points[:, :, 2].max() - high), 0.0
    )
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    normals /= normals[:, 2, None]
    caps = {"up": parent_caps["up"][parents], "delta": parent_caps["delta"][parents]}
    envelope = grade_envelopes(normals, caps, rows[:, 0].astype(int), tangent)
    violation = max(
        float((rows[:, 4] - envelope[:, 0]).max()),
        float((envelope[:, 1] - rows[:, 5]).max()),
        0.0,
    )
    return {
        "parent_ball_excess_m": ball_excess,
        "parent_up_box_violation_m": box_excess,
        "whole_parent_envelope_violation": violation,
        "certified": max(ball_excess, box_excess, violation) <= 1e-9,
    }


def coverage(meshes, anchors, body_anchor, polygons, original_area):
    """Use reconstructed actual geometry, not inherited positive-area CSR claims."""
    triangles, memberships = [], []
    for mesh, anchor in zip(meshes, anchors, strict=True):
        returned = from_world(body_anchor, to_world(anchor, mesh.vertices))
        csr = face_membership(returned, mesh.triangles, polygons)
        memberships.append(csr)
        triangles.extend(shapely.polygons(returned[mesh.triangles, :2]))
    domain = shapely.union_all(triangles)
    source = shapely.union_all(polygons)
    return {
        "actual_body_xy_area_m2": float(domain.area),
        "area_difference_from_original_mesh_m2": float(domain.area - original_area),
        "outside_original_sources_m2": float(domain.difference(source).area),
        "uncovered_original_sources_m2": float(source.difference(domain).area),
        "source_union_area_m2": float(source.area),
        "original_source_holes": int(sum(len(p.interiors) for p in polygons)),
        "actual_domain_holes": int(
            sum(len(p.interiors) for p in shapely.get_parts(domain))
        ),
        "original_source_hole_areas_m2": [
            float(shapely.Polygon(r).area) for p in polygons for r in p.interiors
        ],
        "actual_domain_hole_areas_m2": [
            float(shapely.Polygon(r).area)
            for p in shapely.get_parts(domain)
            for r in p.interiors
        ],
        "boundary_hausdorff_from_source_union_m": float(
            shapely.hausdorff_distance(domain.boundary, source.boundary)
        ),
        "positive_source_ids": np.unique(
            np.concatenate([c.members for c in memberships])
        ).tolist(),
        "unmapped_faces": int(sum(np.count_nonzero(c.status) for c in memberships)),
        "memberships": int(sum(len(c.members) for c in memberships)),
        "intersection_work": int(
            sum(c.work["requested_intersections"] for c in memberships)
        ),
    }, memberships


def boundary_digest(ids, edges, ecef):
    """Content control for these private experiment inputs, not a native cache key."""
    h = hashlib.sha256()
    for array, dtype in ((ids, "<u4"), (edges, "<u4"), (ecef, "<f8")):
        h.update(np.asarray(array, dtype=dtype).tobytes())
    return h.hexdigest()


def store_piece(
    path,
    mesh,
    anchor,
    ids,
    edges,
    parents,
    canonical_ecef,
    canonical_ids,
    canonical_edges,
    csr=None,
    source_ids=None,
):
    """A consumer needs this single file; it never reopens or solves the body."""
    boundary_vertices = np.unique(edges)
    expected_ids = ids[boundary_vertices]
    if not np.array_equal(expected_ids, canonical_ids):
        raise ValueError("same ordered boundary ownership required")
    data = {
        "vertices": mesh.vertices,
        "triangles": mesh.triangles,
        "anchor_lon_lat": np.asarray(anchor),
        "original_vertex_ids": ids,
        "original_parent_faces": parents,
        "boundary_mesh_vertices": boundary_vertices,
        "boundary_ids": canonical_ids,
        "boundary_edges": canonical_edges,
        "boundary_ecef": canonical_ecef,
        "boundary_version": np.array(
            boundary_digest(canonical_ids, canonical_edges, canonical_ecef)
        ),
    }
    if csr is not None:
        data.update(
            source_offsets=csr.offsets,
            source_members=csr.members,
            original_source_ids=source_ids,
        )
    np.savez_compressed(path, **data)
    return {
        "file_bytes": path.stat().st_size,
        "uncompressed_array_bytes": sum(a.nbytes for a in data.values()),
        "boundary_array_bytes": sum(
            v.nbytes for k, v in data.items() if k.startswith("boundary_")
        ),
    }


def read_piece(path, *, expected_version=None):
    """Exercise one stored consumer in a fresh process, with no body input."""
    with np.load(path, allow_pickle=False) as data:
        ids, edges, canonical = (
            data[k] for k in ("boundary_ids", "boundary_edges", "boundary_ecef")
        )
        expected = str(data["boundary_version"])
        if boundary_digest(ids, edges, canonical) != expected:
            raise ValueError("boundary_version_mismatch")
        if expected_version is not None and expected_version != expected:
            raise ValueError("boundary_version_mismatch")
        local = data["vertices"][data["boundary_mesh_vertices"]]
        world = to_world(data["anchor_lon_lat"], local)
        return {
            "boundary_version": expected,
            "boundary_vertices": len(ids),
            "boundary_segments": len(edges),
            "ecef_roundtrip_max_m": float(
                np.linalg.norm(world - canonical, axis=1).max()
            ),
            "faces": len(data["triangles"]),
            "file_bytes": path.stat().st_size,
        }


if __name__ == "__main__":
    import argparse
    import json
    from pathlib import Path

    parser = argparse.ArgumentParser(
        description="Read one independent private water piece"
    )
    parser.add_argument("piece", type=Path)
    print(json.dumps(read_piece(parser.parse_args().piece)))
