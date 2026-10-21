"""Prepared straight-flight stations and shared indexed cuts, without XY lookup."""

import numpy as np
import shapely
from scipy import sparse


def _clear_cut(stations, required, direction):
    width = 2**-10
    lo, hi = sorted((required, required + direction * width))
    interior = stations[(stations > lo) & (stations < hi)]
    values = np.unique(np.r_[lo, interior, hi])
    index = int(np.argmax(np.diff(values)))
    return float((values[index] + values[index + 1]) / 2)


def _flight(plan, owner):
    row = plan["rows"][owner]
    origin = row["xy"][0]
    tangent = row["xy"][-1] - origin
    length = np.linalg.norm(tangent)
    if not np.isfinite(length) or length <= 0:
        raise ValueError("finite positive flight length required")
    tangent /= length
    normal = np.array([-tangent[1], tangent[0]])
    if np.max(abs((row["xy"] - origin) @ normal)) > 1e-8:
        raise ValueError("station loft requires a separately planned straight flight")
    stations = (plan["xy"] - origin) @ tangent
    nodes = {
        c["source_endpoint"]: i
        for i, c in enumerate(plan["construction_contacts"])
        if c.get("source_id") == row["source_id"]
    }
    vertices = np.unique(plan["parts"][owner])
    start = vertices[
        shapely.dwithin(
            plan["masks"][nodes[0]], shapely.points(plan["xy"][vertices]), 1e-8
        )
    ]
    end = vertices[
        shapely.dwithin(
            plan["masks"][nodes[-1]], shapely.points(plan["xy"][vertices]), 1e-8
        )
    ]
    if not len(start) or not len(end):
        raise ValueError("both prepared flight landings must own surface vertices")
    required_start, required_end = (
        float(stations[start].max()),
        float(stations[end].min()),
    )
    cut_start = _clear_cut(stations, required_start, 1)
    cut_end = _clear_cut(stations, required_end, -1)
    if not 0 < cut_start < cut_end < length:
        raise ValueError("straight flight has no positive remaining sloped run")
    return {
        "owner": owner,
        "source_id": row["source_id"],
        "origin_xy_m": origin.copy(),
        "tangent": tangent,
        "start_node": nodes[0],
        "end_node": nodes[-1],
        "start_cut_m": cut_start,
        "end_cut_m": cut_end,
        "extra_flat_m": [cut_start - required_start, required_end - cut_end],
        "stations_m": stations.tolist(),
    }


def _split(vertices, flights, faces, masks, flight, cut, max_vertices, max_faces):
    station = flights[flight]["stations_m"]
    cache, output, owners = {}, [], []

    def crossing(a, b):
        if station[a] == cut:
            return a
        if station[b] == cut:
            return b
        a, b = sorted((a, b))
        if (a, b) in cache:
            return cache[a, b]
        if len(vertices) >= max_vertices:
            raise ValueError("straight-flight vertex budget exhausted")
        fraction = (cut - station[a]) / (station[b] - station[a])
        point = vertices[a] + fraction * (vertices[b] - vertices[a])
        index = len(vertices)
        vertices.append(point)
        for current in flights:
            values = current["stations_m"]
            values.append(values[a] + fraction * (values[b] - values[a]))
        station[index] = cut
        cache[a, b] = index
        return index

    for face, mask in zip(faces, masks, strict=True):
        values = [station[i] for i in face]
        if max(values) <= cut or min(values) >= cut:
            polygons = [face]
        else:
            polygons = []
            for above in (False, True):
                polygon = []
                for a, b in zip(face, np.roll(face, -1), strict=True):
                    inside_a = station[a] >= cut if above else station[a] <= cut
                    inside_b = station[b] >= cut if above else station[b] <= cut
                    if inside_a and (not polygon or polygon[-1] != a):
                        polygon.append(a)
                    if inside_a != inside_b:
                        node = crossing(a, b)
                        if not polygon or polygon[-1] != node:
                            polygon.append(node)
                if polygon and polygon[0] == polygon[-1]:
                    polygon.pop()
                polygons.append(polygon)
        for polygon in polygons:
            for i in range(1, len(polygon) - 1):
                tri = [polygon[0], polygon[i], polygon[i + 1]]
                a, b, c = [vertices[v] for v in tri]
                area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
                if area == 0:
                    continue
                if area < 0:
                    tri.reverse()
                if len(output) >= max_faces:
                    raise ValueError("straight-flight face budget exhausted")
                output.append(tri)
                owners.append(mask)
    return output, owners, len(cache)


def prepare_stair_lofts(plan, *, max_vertices=4096, max_faces=8192):
    if len(plan["xy"]) > max_vertices or sum(map(len, plan["parts"])) > max_faces:
        raise ValueError("initial straight-flight geometry exceeds budget")
    flights = [_flight(plan, owner) for owner, _low, _high in plan["stairs"]]
    if not 1 <= len(flights) <= 16:
        raise ValueError("bounded nonempty straight-flight set required")
    canonical = {}
    for owner, faces in enumerate(plan["parts"]):
        for face in faces:
            key = tuple(sorted(map(int, face)))
            old = canonical.get(key, (list(map(int, face)), 0))
            canonical[key] = old[0], old[1] | (1 << owner)
    faces, masks = map(list, zip(*canonical.values(), strict=True))
    before = len(faces)
    vertices = [point.copy() for point in plan["xy"]]
    intersections, face_visits = 0, 0
    for i, flight in enumerate(flights):
        for cut in (flight["start_cut_m"], flight["end_cut_m"]):
            face_visits += len(faces)
            faces, masks, count = _split(
                vertices, flights, faces, masks, i, cut, max_vertices, max_faces
            )
            intersections += count
    xy, faces = np.array(vertices), np.array(faces, dtype=int)
    parts, globals_ = [], []
    for owner in range(len(plan["rows"])):
        indices = np.array(
            [i for i, mask in enumerate(masks) if mask & (1 << owner)], dtype=int
        )
        globals_.append(indices)
        parts.append(faces[indices])
    errors = []
    for part, domain in zip(parts, plan["prepared_domains"], strict=True):
        polygons = shapely.polygons(xy[part])
        error = shapely.union_all(polygons).symmetric_difference(domain).area
        if not shapely.coverage_is_valid(polygons) or error > 1e-7:
            raise ValueError("station refinement changed a prepared source coverage")
        errors.append(error)
    plan["xy"], plan["parts"], plan["part_global_faces"] = xy, parts, globals_
    plan["stair_lofts"] = flights
    plan["station_refinement"] = {
        "input_faces": before,
        "output_faces": len(faces),
        "edge_intersections": intersections,
        "face_visits": face_visits,
        "maximum_extra_flat_m": max(max(f["extra_flat_m"]) for f in flights),
        "source_coverage_errors_m2": errors,
    }


def _expressions(plan):
    count = len(plan["xy"])
    expressions = {}
    for flight in plan.get("stair_lofts", []):
        start, end = flight["start_cut_m"], flight["end_cut_m"]
        for vertex in np.unique(plan["parts"][flight["owner"]]):
            fraction = float(
                np.clip((flight["stations_m"][vertex] - start) / (end - start), 0, 1)
            )
            expression = (
                (count + 3 * flight["start_node"], 1 - fraction),
                (count + 3 * flight["end_node"], fraction),
            )
            if vertex in expressions and expressions[vertex] != expression:
                raise ValueError(
                    "overlapping flights need one explicit shared height owner"
                )
            expressions[int(vertex)] = expression
    return expressions


def add_loft_equalities(plan, system):
    for vertex, expression in _expressions(plan).items():
        columns = [vertex, *[i for i, _value in expression]]
        values = [1.0, *[-value for _i, value in expression]]
        system.equal(columns, np.array(values) * 1024, 0)


def height_projection(plan, variables):
    expressions = _expressions(plan)
    active = [i for i in range(variables) if i not in expressions]
    column = {value: i for i, value in enumerate(active)}
    rows, columns, data = [], [], []
    for row in range(variables):
        for old_column, value in expressions.get(row, ((row, 1.0),)):
            if value:
                rows.append(row)
                columns.append(column[old_column])
                data.append(value)
    return sparse.csc_matrix((data, (rows, columns)), shape=(variables, len(active)))
