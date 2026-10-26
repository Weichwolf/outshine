"""Native-shaped source CSR and maximum-area representative, without vertex edits."""

from dataclasses import dataclass, fields

import numpy as np
import shapely
from water_exact_membership import ExactBudget, ExactBudgetExceeded, exact_overlap
from water_positive_membership import OverlapRelation, overlap_relation, prepare_source

NO_SOURCE = np.iinfo(np.uint32).max


@dataclass(frozen=True)
class BindingBudget:
    sources: int = 4096
    source_coordinates: int = 262144
    vertices: int = 8192
    faces: int = 16384
    intersections: int = 100000
    memberships: int = 100000
    face_work: int = 100000
    total_work: int = 10000000

    def __post_init__(self):
        maximum = (4096, 262144, 8192, 16384, 100000, 100000, 100000, 10000000)
        for field, cap in zip(fields(self), maximum, strict=True):
            value = getattr(self, field.name)
            if type(value) is not int or not 0 <= value <= cap:
                raise ValueError("finite explicit binding budget required")


@dataclass(frozen=True)
class SourceBinding:
    offsets: np.ndarray
    sources: np.ndarray
    representatives: np.ndarray
    states: tuple
    report: dict


def _inputs(vertices, faces, sources, source_ids, budget):
    vertices, faces, source_ids = (
        np.asarray(vertices),
        np.asarray(faces),
        np.asarray(source_ids),
    )
    sources = np.asarray(tuple(sources), dtype=object)
    if (
        vertices.ndim != 2
        or vertices.shape[1] != 2
        or len(vertices) > budget.vertices
        or not np.isfinite(vertices).all()
        or np.any(abs(vertices) > 1e8)
    ):
        raise ValueError("bounded finite original vertices required")
    if (
        faces.ndim != 2
        or faces.shape[1] != 3
        or not 0 < len(faces) <= budget.faces
        or faces.dtype.kind not in "ui"
        or np.any(faces < 0)
        or np.any(faces >= len(vertices))
    ):
        raise ValueError("bounded valid original faces required")
    if (
        not 0 < len(sources) <= budget.sources
        or source_ids.shape != (len(sources),)
        or source_ids.dtype.kind not in "ui"
        or np.any(source_ids < 0)
        or np.any(source_ids >= NO_SOURCE)
        or len(np.unique(source_ids)) != len(source_ids)
    ):
        raise ValueError(
            "bounded originals and unique exact uint32 source IDs required"
        )
    if shapely.get_num_coordinates(sources).sum() > budget.source_coordinates:
        raise ValueError("source-coordinate budget")
    if any(
        s.geom_type not in ("Polygon", "MultiPolygon") or s.is_empty or not s.is_valid
        for s in sources
    ):
        raise ValueError("valid original source polygons required")
    return vertices, faces, sources, source_ids


def _face(triangle, candidates, sources, prepared, method, work):
    if method == "areas":
        work.tick("area_calls")
        areas, _missing = exact_overlap(triangle, sources[candidates], work)
        pairs = [
            (int(i), area)
            for i, area in zip(candidates, areas, strict=True)
            if area > 0
        ]
        return (
            [i for i, _a in pairs],
            (max(pairs, key=lambda p: p[1])[0] if pairs else None),
            "areas",
        )
    relations = [
        (int(i), overlap_relation(triangle, prepared[i], work)) for i in candidates
    ]
    members = [i for i, relation in relations if relation != OverlapRelation.DISJOINT]
    if not members:
        return [], None, "empty"
    if len(members) == 1:
        return members, members[0], "single"
    full = [
        i for i, relation in relations if relation == OverlapRelation.COVERS_TRIANGLE
    ]
    if full:
        return members, full[0], "covered"
    work.tick("area_calls")
    areas, _missing = exact_overlap(triangle, sources[members], work)
    if any(area <= 0 for area in areas):
        raise ValueError("positive predicate and exact area disagree")
    return (
        members,
        members[max(range(len(areas)), key=areas.__getitem__)],
        "partial_areas",
    )


def bind_sources(
    vertices, faces, sources, source_ids, *, method="relations", budget=None
):
    """Return source CSR/representatives; incomplete faces never expose partial IDs.

    Representatives maximize exact original overlap area; ties use the smallest
    original source ID, independently of input order. Single positive owners and
    complete single-source coverage prove the winner without evaluating areas.
    No union-coverage admission, area weights or physical water fit is performed.
    """
    if budget is None:
        budget = BindingBudget()
    if method not in ("areas", "relations") or not isinstance(budget, BindingBudget):
        raise ValueError("known binding method and explicit budget required")
    vertices, faces, sources, source_ids = _inputs(
        vertices, faces, sources, source_ids, budget
    )
    prepared = [prepare_source(s) for s in sources] if method == "relations" else None
    tree = shapely.STRtree(sources)
    offsets = np.zeros(len(faces) + 1, dtype=np.uint32)
    representatives = np.full(len(faces), NO_SOURCE, dtype=np.uint32)
    committed, states, covered = [], [], set()
    intersections = operations = 0
    paths, counts = {}, {}
    for face, indices in enumerate(faces):
        work = ExactBudget(limit=min(budget.face_work, budget.total_work - operations))
        try:
            if operations == budget.total_work:
                raise ExactBudgetExceeded("predicate work budget")
            if intersections == budget.intersections:
                raise ExactBudgetExceeded("intersection budget")
            if len(committed) == budget.memberships:
                raise ExactBudgetExceeded("membership budget")
            triangle = vertices[indices]
            candidates = sorted(
                tree.query(shapely.Polygon(triangle)), key=lambda i: int(source_ids[i])
            )
            if len(candidates) > budget.intersections - intersections:
                raise ExactBudgetExceeded("intersection budget")
            intersections += len(candidates)
            members, owner, path = _face(
                triangle, candidates, sources, prepared, method, work
            )
            paths[path] = paths.get(path, 0) + 1
            if not members:
                states.append("no_positive_source")
            elif len(members) > budget.memberships - len(committed):
                raise ExactBudgetExceeded("membership budget")
            else:
                ids = [int(source_ids[i]) for i in members]
                committed.extend(ids)
                covered.update(ids)
                representatives[face] = source_ids[owner]
                states.append("complete")
        except (ValueError, ExactBudgetExceeded) as error:
            states.append(str(error))
        operations += work.operations
        for key, value in work.counts.items():
            counts[key] = counts.get(key, 0) + value
        offsets[face + 1] = len(committed)
    members = np.asarray(committed, dtype=np.uint32)
    for array in (offsets, members, representatives):
        array.setflags(write=False)
    unused = sorted(set(map(int, source_ids)) - covered)
    return SourceBinding(
        offsets,
        members,
        representatives,
        tuple(states),
        {
            "valid": all(s == "complete" for s in states) and not unused,
            "unused_source_ids": unused,
            "candidate_intersections": intersections,
            "counted_operations": operations,
            "work": counts,
            "paths": paths,
            "area_evaluations": counts.get("area_calls", 0),
            "owned_edge_array_bytes": sum(s.edges.nbytes for s in prepared)
            if prepared
            else 0,
            "output_array_bytes": offsets.nbytes
            + members.nbytes
            + representatives.nbytes,
            "scope": "Source IDs and exact maximum-area representative only, before native roundoff/component/height/asset admission. No area weights or union-coverage claim.",
        },
    )
