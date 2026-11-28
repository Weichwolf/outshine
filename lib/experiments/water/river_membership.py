"""Sparse positive-area source provenance for a caller-selected water mesh (#760).

This records overlap, not full face coverage, hydrological compatibility or a
geodetic height proof. Original geometry/identity tables remain with the caller.
"""

from dataclasses import dataclass

import numpy as np
import shapely
from shapely import STRtree
from shapely.errors import GEOSException
from shapely.geometry import Polygon

NO_SOURCE = np.iinfo(np.uint32).max
STATUS = (
    "mapped",
    "no_positive_overlap",
    "intersection_budget",
    "membership_budget",
    "geometry_error",
)


@dataclass(frozen=True)
class Membership:
    offsets: np.ndarray
    members: np.ndarray
    representative: np.ndarray
    member_area_m2: np.ndarray
    uncovered_area_m2: np.ndarray
    status: np.ndarray
    work: dict


def _validated(
    vertices, faces, sources, intersection_limit, member_limit, source_coordinate_limit
):
    vertices, faces = np.asarray(vertices), np.asarray(faces)
    sources = np.asarray(tuple(sources), dtype=object)
    if any(
        type(n) is not int or not 0 <= n < NO_SOURCE
        for n in (intersection_limit, member_limit, source_coordinate_limit)
    ):
        raise ValueError("nonnegative uint32-sized work and reference budgets required")
    if shapely.get_num_coordinates(sources).sum() > source_coordinate_limit:
        raise ValueError("original coordinate budget exceeded")
    if (
        vertices.ndim != 2
        or vertices.shape[1] not in (2, 3)
        or not np.isfinite(vertices).all()
        or faces.ndim != 2
        or faces.shape[1] != 3
        or faces.dtype.kind not in "ui"
        or np.any(faces < 0)
        or np.any(faces >= len(vertices))
        or len(faces) > 16384
        or len(vertices) > 8192
        or len(sources) > 4096
    ):
        raise ValueError(
            "bounded finite vertices, valid triangle indices and source table required"
        )
    if (
        any(
            p.geom_type not in ("Polygon", "MultiPolygon")
            or p.is_empty
            or not p.is_valid
            or not np.isfinite(p.area)
            or p.area <= 0
            for p in sources
        )
        or not np.isfinite(shapely.get_coordinates(sources)).all()
    ):
        raise ValueError("finite valid positive-area original polygons required")
    return vertices, faces, sources


def face_membership(
    vertices,
    faces,
    sources,
    *,
    intersection_limit=100000,
    member_limit=100000,
    source_coordinate_limit=262144,
    indexed=True,
):
    """Map positive GEOS overlap to original-table indices, one face at a time.

    Budgets cap requested polygon intersections and committed CSR entries. A
    rejected face retains no partial membership; other faces may still fit.
    One STR query can enumerate up to all sources before the intersection check;
    query traversal/GEOS allocation costs are not claimed to obey that budget.
    """
    vertices, faces, sources = _validated(
        vertices,
        faces,
        sources,
        intersection_limit,
        member_limit,
        source_coordinate_limit,
    )
    tree = STRtree(sources) if indexed else None
    offsets = np.zeros(len(faces) + 1, dtype="<u4")
    representative = np.full(len(faces), NO_SOURCE, dtype="<u4")
    missing = np.full(len(faces), np.nan)
    status = np.zeros(len(faces), dtype=np.uint8)
    members, member_area = [], []
    candidates_total = intersections = positive_total = unions = 0
    maximum_candidates = 0
    for i, face in enumerate(faces):
        offsets[i + 1] = len(members)
        if len(sources) and intersections == intersection_limit:
            status[i] = 2
            continue
        if len(sources) and len(members) == member_limit:
            status[i] = 3
            continue
        triangle = Polygon(vertices[face, :2])
        if (
            not triangle.is_valid
            or not np.isfinite(triangle.area)
            or triangle.area <= 0
        ):
            status[i] = 4
            continue
        candidates = (
            np.sort(tree.query(triangle)) if indexed else np.arange(len(sources))
        )
        candidates_total += len(candidates)
        maximum_candidates = max(maximum_candidates, len(candidates))
        if intersections + len(candidates) > intersection_limit:
            status[i] = 2
            continue
        intersections += len(candidates)
        try:
            clipped = shapely.intersection(triangle, sources[candidates])
            areas = shapely.area(clipped)
            if not np.isfinite(areas).all():
                status[i] = 4
                continue
            positive = areas > 0
            count = int(positive.sum())
            positive_total += count
            if len(members) + count > member_limit:
                status[i] = 3
                continue
            if not count:
                status[i] = 1
                missing[i] = triangle.area
                continue
            union = shapely.union_all(clipped[positive])
            unions += 1
            missing[i] = triangle.difference(union).area
        except GEOSException:
            status[i] = 4
            continue
        selected = candidates[positive]
        representative[i] = selected[np.argmax(areas[positive])]
        members.extend(selected)
        member_area.extend(areas[positive])
        offsets[i + 1] = len(members)
    members = np.asarray(members, dtype="<u4")
    member_area = np.asarray(member_area, dtype=float)
    work = {
        "faces": len(faces),
        "sources": len(sources),
        "source_coordinates": int(shapely.get_num_coordinates(sources).sum()),
        "indexed": indexed,
        "all_pairs": len(faces) * len(sources),
        "enumerated_candidates": candidates_total,
        "maximum_candidates_per_face": maximum_candidates,
        "requested_intersections": intersections,
        "positive_intersections": positive_total,
        "union_difference_pairs": unions,
        "committed_members": len(members),
        "status_counts": {
            name: int((status == i).sum()) for i, name in enumerate(STATUS)
        },
        "output_array_bytes": sum(
            a.nbytes
            for a in (offsets, members, representative, member_area, missing, status)
        ),
        "geometry_workspace_bytes": None,
    }
    return Membership(
        offsets, members, representative, member_area, missing, status, work
    )
