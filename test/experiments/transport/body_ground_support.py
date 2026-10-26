"""Nearest support on one explicitly selected actual body underside, not ground inference."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler


def _check_cancel(cancelled):
    if cancelled is not None and cancelled():
        raise ValueError("body-ground support cancelled")


def _topology(sampler, max_pairs, cancelled):
    polygons, triangles = sampler._polygons, sampler.mesh.triangles
    if not bool(shapely.coverage_is_valid(polygons)):
        raise ValueError("nonoverlapping conforming projected body coverage required")
    used, begin = 0, 0
    while begin < len(polygons):
        _check_cancel(cancelled)
        count = min(128, (max_pairs - used) // len(polygons), len(polygons) - begin)
        if not count:
            raise ValueError("body qualification candidate budget exhausted")
        pairs = sampler._tree.query(
            polygons[begin : begin + count], predicate="intersects"
        )
        used += pairs.shape[1]
        pairs[0] += begin
        for first, second in pairs.T:
            if first >= second:
                continue
            common = np.intersect1d(triangles[first], triangles[second])
            contact = shapely.intersection(polygons[first], polygons[second])
            if not len(common) or (contact.length > 0 and len(common) < 2):
                raise ValueError(
                    "projected touching sheets require actual indexed continuity"
                )
        begin += count
    return used


@dataclass(frozen=True, slots=True)
class GroundSupport:
    sampler: SurfaceSampler
    source_vertex_ids: np.ndarray
    source_masks: np.ndarray
    contact_key: int
    strategy: str
    boundary_faces: object
    boundary_geometries: object
    boundary_tree: object
    domain: object
    preparation: object


def _snapshot(body, max_faces):
    vertices, faces, masks = (
        body[k] for k in ("body_vertices", "bottom_faces", "face_sources")
    )
    if (
        not all(isinstance(v, np.ndarray) for v in (vertices, faces, masks))
        or vertices.ndim != 2
        or vertices.shape[1:] != (3,)
        or vertices.dtype != np.float64
        or faces.ndim != 2
        or faces.shape[1:] != (3,)
        or faces.dtype.kind not in "iu"
        or not 0 < len(faces) <= max_faces
        or faces.min() < 0
        or faces.max() >= len(vertices)
        or masks.shape != (len(faces),)
        or masks.dtype.kind not in "iu"
        or np.any(masks <= 0)
    ):
        raise ValueError("bounded binary64 underside and indexed source masks required")
    source_vertices, inverse = np.unique(faces, return_inverse=True)
    local_vertices = vertices[source_vertices]
    sampler = SurfaceSampler(
        Mesh(local_vertices, inverse.reshape(faces.shape)), max_faces=max_faces
    )
    source_masks = np.array(masks, dtype=np.uint64, copy=True)
    source_masks.setflags(write=False)
    source_vertices.setflags(write=False)
    return sampler, source_vertices, source_masks, local_vertices.nbytes


def prepare(
    body,
    contact_key,
    *,
    strategy="index",
    max_faces=8192,
    max_pairs=262144,
    cancelled=None,
):
    """Snapshot the selected single-valued underside; caller declares its ground group."""
    _check_cancel(cancelled)
    if strategy not in ("scan", "index", "boundary"):
        raise ValueError("known support strategy required")
    if type(contact_key) is not int or not 0 < contact_key < 2**64:
        raise ValueError("explicit nonzero UInt64 ground contact key required")
    if any(type(v) is not int or v < 0 for v in (max_faces, max_pairs)):
        raise ValueError("nonnegative body qualification budgets required")
    sampler, source_vertices, source_masks, gather_bytes = _snapshot(body, max_faces)
    tri = sampler.mesh.vertices[sampler.mesh.triangles]
    a, b = tri[:, 1, :2] - tri[:, 0, :2], tri[:, 2, :2] - tri[:, 0, :2]
    if np.any(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] <= 0):
        raise ValueError("positive indexed projected underside caps required")
    used = _topology(sampler, max_pairs, cancelled)
    boundary_faces = boundary_geometries = boundary_tree = domain = None
    extra_bytes = boundary_count = 0
    if strategy == "boundary":
        all_edges = np.sort(
            sampler.mesh.triangles[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1
        )
        edges, first, counts = np.unique(
            all_edges, axis=0, return_index=True, return_counts=True
        )
        if np.any(counts > 2):
            raise ValueError("nonmanifold cap edges cannot define a body boundary")
        edges = edges[counts == 1]
        if not len(edges):
            raise ValueError("a finite projected field requires boundary edges")
        boundary_faces = (first[counts == 1] // 3).astype(np.uint32)
        boundary_faces.setflags(write=False)
        boundary_count = len(edges)
        xy = sampler.mesh.vertices[edges, :2]
        boundary_geometries = shapely.linestrings(xy)
        boundary_geometries.setflags(write=False)
        boundary_tree = shapely.STRtree(boundary_geometries)
        domain = shapely.coverage_union_all(sampler._polygons)
        shapely.prepare(domain)
        extra_bytes = edges.nbytes + boundary_faces.nbytes + xy.nbytes
    _check_cancel(cancelled)
    return GroundSupport(
        sampler,
        source_vertices,
        source_masks,
        contact_key,
        strategy,
        boundary_faces,
        boundary_geometries,
        boundary_tree,
        domain,
        MappingProxyType(
            {
                "faces": len(sampler.mesh.triangles),
                "selected_vertices": len(source_vertices),
                "source_vertex_map_bytes": source_vertices.nbytes,
                "selected_vertex_gather_bytes": gather_bytes,
                "qualification_candidate_pairs": used,
                "boundary_edges": boundary_count,
                "mesh_snapshot_bytes": sampler.mesh.vertices.nbytes
                + sampler.mesh.triangles.nbytes,
                "source_mask_copy_bytes": source_masks.nbytes,
                "selected_boundary_working_array_bytes": extra_bytes,
                "retained_boundary_face_map_bytes": 0
                if boundary_faces is None
                else boundary_faces.nbytes,
            }
        ),
    )


def _tree_pairs(tree, points, geometry_count, max_pairs, cancelled):
    pairs, distances, used, begin = [], [], 0, 0
    while begin < len(points):
        _check_cancel(cancelled)
        count = min(256, (max_pairs - used) // geometry_count, len(points) - begin)
        if not count:
            raise ValueError("nearest support candidate budget exhausted")
        found, distance = tree.query_nearest(
            points[begin : begin + count], all_matches=True, return_distance=True
        )
        found[0] += begin
        pairs.append(found)
        distances.append(distance)
        used += len(distance)
        begin += count
    if not pairs:
        return np.empty((2, 0), dtype=np.int64), np.empty(0)
    return np.concatenate(pairs, axis=1), np.concatenate(distances)


def _nearest(model, xy, points, max_pairs, cancelled):
    sampler = model.sampler
    if model.strategy == "scan":
        distances = shapely.distance(points[:, None], sampler._polygons[None, :])
        best = distances.min(axis=1)
        rows, faces = np.nonzero(distances == best[:, None])
        pairs = np.vstack([rows, faces])
        return pairs, best[rows], sampler._polygons[faces], distances.nbytes
    if model.strategy == "index":
        pairs, distances = _tree_pairs(
            sampler._tree, points, len(sampler.mesh.triangles), max_pairs, cancelled
        )
        return pairs, distances, sampler._polygons[pairs[1]], 0
    inside = shapely.covers(model.domain, points)
    groups, remaining = [], max_pairs
    for selected, tree, geometries, owners in (
        (np.flatnonzero(inside), sampler._tree, sampler._polygons, None),
        (
            np.flatnonzero(~inside),
            model.boundary_tree,
            model.boundary_geometries,
            model.boundary_faces,
        ),
    ):
        pair, distances = _tree_pairs(
            tree, points[selected], len(geometries), remaining, cancelled
        )
        remaining -= len(distances)
        shapes = geometries[pair[1]]
        if owners is not None:
            pair[1] = owners[pair[1]]
        pair[0] = selected[pair[0]]
        groups.append((pair, distances, shapes))
    return (
        np.concatenate([g[0] for g in groups], axis=1),
        np.concatenate([g[1] for g in groups]),
        np.concatenate([g[2] for g in groups]),
        0,
    )


def sample(model, xy, *, max_queries=4096, max_pairs=262144, cancelled=None):
    """Return explicit unresolved values for ambiguous heights, never a zero-height miss."""
    _check_cancel(cancelled)
    if any(type(v) is not int or v < 0 for v in (max_queries, max_pairs)):
        raise ValueError("nonnegative support query budgets required")
    if len(xy) > max_queries:
        raise ValueError("support query count exceeds budget before conversion")
    xy = np.asarray(xy, dtype=float)
    if xy.ndim != 2 or xy.shape[1:] != (2,) or not np.isfinite(xy).all():
        raise ValueError("finite Nx2 support query points required")
    count, faces_count = len(xy), len(model.sampler.mesh.triangles)
    if count * faces_count > max_pairs:
        raise ValueError("conservative support face/query budget exhausted")
    result = {
        "height_m": np.full(count, np.nan),
        "foot_xy_m": np.full((count, 2), np.nan),
        "distance_m": np.full(count, np.nan),
        "body_cap_face_ids": np.full(count, -1, dtype=np.int64),
        "source_masks": np.zeros(count, dtype=np.uint64),
        "valid": np.zeros(count, dtype=bool),
    }
    reasons = np.full(count, "ambiguous_nearest_height", dtype="U40")
    emitted = matrix_bytes = candidates = 0
    if count:
        points = shapely.points(xy)
        pairs, distances, geometries, matrix_bytes = _nearest(
            model, xy, points, max_pairs, cancelled
        )
        emitted = pairs.shape[1]
        if emitted > max_pairs:
            raise ValueError("nearest witness count exceeds support budget")
        foot_lines = shapely.shortest_line(points[pairs[0]], geometries)
        foot = shapely.get_coordinates(foot_lines).reshape(-1, 2, 2)[:, 1, :]
        support = model.sampler.sample(
            foot, max_queries=max_pairs, max_candidates=max_pairs
        )
        _check_cancel(cancelled)
        if "candidate_budget_exhausted" in support.reasons:
            raise ValueError("surface witness candidate budget exhausted")
        order = np.lexsort((pairs[1], pairs[0]))
        pairs, distances, foot = pairs[:, order], distances[order], foot[order]
        heights, owners = support.points_enu_m[order, 2], support.face_indices[order]
        starts = np.r_[0, np.flatnonzero(np.diff(pairs[0])) + 1]
        rows = pairs[0, starts]
        ok = (np.minimum.reduceat(owners, starts) >= 0) & (
            np.maximum.reduceat(heights, starts) - np.minimum.reduceat(heights, starts)
            <= model.sampler.height_tolerance_m
        )
        first, valid_rows = starts[ok], rows[ok]
        result["height_m"][valid_rows] = heights[first]
        result["foot_xy_m"][valid_rows] = foot[first]
        result["body_cap_face_ids"][valid_rows] = owners[first]
        result["source_masks"][valid_rows] = model.source_masks[owners[first]]
        result["distance_m"][rows] = distances[starts]
        result["valid"][valid_rows] = True
        reasons[valid_rows] = ""
        candidates = support.report["candidates"]
    result.update(
        reasons=tuple(map(str, reasons)),
        contact_key=model.contact_key,
        work={
            "queries": count,
            "reference_face_query_pairs": count * faces_count,
            "emitted_nearest_witnesses": emitted,
            "surface_sampler_candidates": candidates,
            "materialized_distance_matrix_bytes": matrix_bytes,
            "selected_result_array_bytes": sum(v.nbytes for v in result.values()),
            "scope": "STRtree internal node visits/distance predicates and all GEOS/NumPy allocations are not counted.",
        },
    )
    return result
