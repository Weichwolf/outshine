"""Actual four-way input and one planar arrangement with separate overpass owners."""

import itertools
import json
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.geometry import conform_triangles
from shared_bridge_geometry import corridor
from transport_vertex_pool import welded_vertices

INPUT = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/transport/wien-junction-sources.json"
)


def inputs(path=INPUT):
    case = json.loads(path.read_text())
    lat, lon = case["reference_lat_lon"]
    scale = np.array([111320 * np.cos(np.deg2rad(lat)), 111132])
    rows = []
    for way in case["ways"]:
        xy = (np.array(way["points_lat_lon"])[:, ::-1] - [lon, lat]) * scale
        rows.append(
            {
                "source_id": way["source"]["feature"]["id"],
                "xy": xy,
                "width_m": way["width_m"],
                "gradient": way["declared_max_gradient"],
                "prior": np.array(way["source_ground_height_m"]),
                "original": way,
            }
        )
    if [r["source_id"] for r in rows] != [377113090, 97041640, 229812320, 1013938082]:
        raise ValueError("actual complete four-way native source input required")
    return case, rows


def landings(rows, domains, method, width_reach_m):
    nodes = [rows[0]["xy"][0], rows[0]["xy"][-1]]
    incident = [[0, 1, 3], [0, 2, 3]]
    masks = []
    for node, owners in zip(nodes, incident, strict=True):
        parts = [
            shapely.Point(node).buffer(
                max(rows[i]["width_m"] for i in owners) / 2, quad_segs=2
            )
        ]
        for i, j in itertools.combinations(owners, 2):
            pieces = shapely.get_parts(domains[i].intersection(domains[j]))
            parts.extend(p for p in pieces if p.distance(shapely.Point(node)) < 1e-8)
        reach = 4.0 if method == "fixed" else width_reach_m
        masks.append(shapely.union_all(parts).buffer(reach, quad_segs=2))
    return nodes, incident, masks


def cross_sections(row):
    xy, half = row["xy"], row["width_m"] * 2
    arc = np.r_[0, np.cumsum(np.linalg.norm(np.diff(xy, axis=0), axis=1))]
    station = np.unique(
        np.r_[
            arc,
            np.arange(0, min(30, arc[-1]), 2),
            np.arange(max(0, arc[-1] - 30), arc[-1], 2),
        ]
    )
    line = shapely.LineString(xy)
    result = []
    for s in station:
        point = np.array(line.interpolate(s).coords[0])
        a = np.array(line.interpolate(max(0, s - 1e-4)).coords[0])
        b = np.array(line.interpolate(min(line.length, s + 1e-4)).coords[0])
        t = (b - a) / np.linalg.norm(b - a)
        n = np.array([-t[1], t[0]])
        result.append(shapely.LineString([point - n * half, point + n * half]))
    return result


def arrangement(rows, domains, masks):
    lines = [p.boundary for p in domains + masks]
    for row in rows:
        lines.append(shapely.LineString(row["xy"]))
        lines.extend(cross_sections(row))
    noded = shapely.get_parts(shapely.union_all(lines))
    if len(noded) > 8192:
        raise ValueError("local stair arrangement segment budget")
    cells = shapely.get_parts(shapely.polygonize(noded))
    union = shapely.union_all(domains)
    cells = cells[shapely.covers(union, shapely.point_on_surface(cells))]
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(cells))
    raw = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    xy, inverse = np.unique(raw.reshape(-1, 2), axis=0, return_inverse=True)
    xy, labels, drift = welded_vertices(xy, 1e-8)
    faces = labels[inverse].reshape(-1, 3)
    points = xy[faces]
    signed = np.cross(
        np.c_[points[:, 1] - points[:, 0], np.zeros(len(faces))],
        np.c_[points[:, 2] - points[:, 0], np.zeros(len(faces))],
    )[:, 2]
    faces = faces[signed != 0]
    signed = signed[signed != 0]
    faces[signed < 0] = faces[signed < 0, ::-1]
    faces, _note = conform_triangles(xy, faces, max_faces=8192, max_candidates=65536)
    shapes = shapely.polygons(xy[faces])
    if (
        not shapely.coverage_is_valid(shapes)
        or shapely.union_all(shapes).symmetric_difference(union).area > 1e-7
    ):
        raise ValueError("actual stair arrangement changes source footprints")
    owners = np.array(
        [shapely.covers(p, shapely.point_on_surface(shapes)) for p in domains]
    ).T
    return (
        xy,
        faces,
        owners,
        {
            "faces": len(faces),
            "vertices": len(xy),
            "segments": len(noded),
            "weld_m": float(drift),
        },
    )


def layout(rows, method, *, width_reach_m=0.75):
    if method not in ("fixed", "width"):
        raise ValueError("known stair core method required")
    domains = [corridor(row) for row in rows]
    nodes, incident, masks = landings(rows, domains, method, width_reach_m)
    xy, faces, owners, note = arrangement(rows, domains, masks)
    # Vertex identity remains per source until a real endpoint landing joins it.
    keys = {(i, int(v)) for i in range(4) for v in np.unique(faces[owners[:, i]])}
    parent = {k: k for k in keys}

    def root(k):
        while parent[k] != k:
            parent[k] = parent[parent[k]]
            k = parent[k]
        return k

    for mask, sources in zip(masks, incident, strict=True):
        inside = np.flatnonzero(shapely.dwithin(mask, shapely.points(xy), 1e-8))
        for v in inside:
            hit = [i for i in sources if (i, v) in keys]
            for i in hit[1:]:
                parent[root((i, v))] = root((hit[0], v))
    roots = sorted({root(k) for k in keys})
    ids = {r: i for i, r in enumerate(roots)}
    bound = {k: ids[root(k)] for k in keys}
    vertices = np.array([xy[v] for _i, v in roots])
    parts = [
        np.array([[bound[(i, int(v))] for v in f] for f in faces[owners[:, i]]])
        for i in range(4)
    ]
    return {
        "xy": vertices,
        "parts": parts,
        "part_global_faces": [np.flatnonzero(owners[:, i]) for i in range(4)],
        "primary_source": [i for i, _v in roots],
        "rows": rows,
        "domains": domains,
        "nodes": nodes,
        "incident": incident,
        "masks": masks,
        "note": note,
    }
