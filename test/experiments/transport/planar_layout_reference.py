"""Actual shared MVT layout, isolated repairs and independent edge diagnostics."""

import copy
from unittest.mock import patch

import numpy as np
import shapely
import stair_junction_layout as layout
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_layout import prepare_profiles
from outshine_ref.geometry import conform_triangles
from transport_vertex_pool import welded_vertices


def unchanged_vertices(_rows, xy):
    return xy, np.arange(len(xy)), 0.0, {}


def unchanged_faces(_xy, faces, **_kwargs):
    return faces, {}


def original_layout():
    captured = {}
    arrangement = layout.arrangement

    def record(rows, domains, masks, **kwargs):
        captured["args"] = (
            copy.deepcopy(rows),
            domains.copy(),
            masks.copy(),
            kwargs.copy(),
        )
        result = arrangement(rows, domains, masks, **kwargs)
        captured["reference"] = copy.deepcopy(result)
        return result

    case, sources = inputs()
    with patch.object(layout, "arrangement", record):
        prepare_profiles(case, sources, plan_connections(sources))
    rows, domains, masks, kwargs = captured["args"]
    with (
        patch.object(layout, "arrangement_vertices", unchanged_vertices),
        patch.object(layout, "conform_triangles", unchanged_faces),
    ):
        raw = arrangement(copy.deepcopy(rows), domains, masks, **kwargs)
    return captured["reference"], raw, captured["args"]


def nonzero_faces(points, faces):
    triangle = points[faces]
    a, b = triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]
    signed = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    faces = faces[signed != 0].copy()
    faces[signed[signed != 0] < 0] = faces[signed[signed != 0] < 0, ::-1]
    return faces


def repair(points, faces):
    points, remap, drift = welded_vertices(points, 1e-8)
    faces = nonzero_faces(points, remap[faces])
    faces, note = conform_triangles(points, faces, max_faces=8192, max_candidates=65536)
    return points, nonzero_faces(points, faces), remap, drift, note


def hanging_pairs(points, faces):
    edges = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
    edges = np.unique(np.sort(edges, axis=1), axis=0)
    epsilon = 64 * np.finfo(float).eps * max(1.0, float(abs(points).max(initial=0)))
    tree = shapely.STRtree(shapely.linestrings(points[edges]))
    pairs = tree.query(shapely.points(points), predicate="dwithin", distance=epsilon)
    hanging = []
    for vertex, edge in pairs.T:
        a, b = edges[edge]
        if vertex in (a, b):
            continue
        delta = points[b] - points[a]
        square = delta @ delta
        if square > 0 and 0 < ((points[vertex] - points[a]) @ delta / square) < 1:
            hanging.append((int(vertex), int(edge)))
    return np.asarray(hanging, dtype=int).reshape(-1, 2)


def ablation(args):
    rows, domains, masks, kwargs = args
    results = []
    for weld, conform in [(True, True), (False, False), (True, False), (False, True)]:
        vertices = layout.arrangement_vertices if weld else unchanged_vertices
        triangles = layout.conform_triangles if conform else unchanged_faces
        row = {"weld": weld, "conform": conform}
        with (
            patch.object(layout, "arrangement_vertices", vertices),
            patch.object(layout, "conform_triangles", triangles),
        ):
            try:
                xy, faces, _owners, note = layout.arrangement(
                    copy.deepcopy(rows), domains, masks, **kwargs
                )
                row.update(
                    status="complete",
                    vertices=len(xy),
                    faces=len(faces),
                    hanging_pairs=len(hanging_pairs(xy, faces)),
                    note=note,
                )
            except ValueError as error:
                row.update(status="rejected", reason=str(error))
        results.append(row)
    return results
