"""Indexed station clipping with separate geometric and height-edge ownership."""

import numpy as np


def _geometry(face, parent, planar):
    return parent, tuple(sorted(planar[v] for v in face))


def _clip_polygons(face, station, cut, crossing):
    values = [station[i] for i in face]
    if max(values) <= cut or min(values) >= cut:
        return [face]
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
    return polygons


def _conform_face(face, planar, split_edges, crossing):
    triangles = [list(face)]
    for a, b in zip(face, np.roll(face, -1), strict=True):
        if tuple(sorted((planar[a], planar[b]))) not in split_edges:
            continue
        node = crossing(a, b)
        owner = next(i for i, f in enumerate(triangles) if a in f and b in f)
        opposite = next(v for v in triangles[owner] if v not in (a, b))
        triangles[owner : owner + 1] = [[a, node, opposite], [node, b, opposite]]
    return triangles


def _split(
    vertices,
    flights,
    faces,
    masks,
    parents,
    planar,
    flight,
    cut,
    max_vertices,
    max_faces,
    local_owners,
):
    station = flights[flight]["stations_m"]
    frame = _frame(flights[flight])
    aligned = [f["stations_m"] for f in flights if _frame(f) == frame]
    cache, planar_cache, output, owners, lineage = {}, {}, [], [], []
    records = list(zip(faces, masks, parents, strict=True))
    selected = (
        {
            _geometry(face, parent, planar)
            for face, mask, parent in records
            if mask & local_owners
        }
        if local_owners is not None
        else None
    )

    def crossing(a, b):
        if station[a] == cut:
            return a
        if station[b] == cut:
            return b
        a, b = sorted((a, b), key=planar.__getitem__)
        if (a, b) in cache:
            return cache[a, b]
        if len(vertices) >= max_vertices:
            raise ValueError("straight-flight vertex budget exhausted")
        edge = planar[a], planar[b]
        shared = planar_cache.get(edge)
        index = len(vertices)
        if shared is None:
            fraction = (cut - station[a]) / (station[b] - station[a])
            vertices.append(vertices[a] + fraction * (vertices[b] - vertices[a]))
            for current in flights:
                values = current["stations_m"]
                values.append(values[a] + fraction * (values[b] - values[a]))
            planar.append(index)
            planar_cache[edge] = index
        else:
            vertices.append(vertices[shared].copy())
            for current in flights:
                current["stations_m"].append(current["stations_m"][shared])
            planar.append(planar[shared])
        for values in aligned:
            values[index] = cut
        cache[a, b] = index
        return index

    def emit(tri, mask, parent):
        a, b, c = [vertices[v] for v in tri]
        area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if area == 0:
            return
        if area < 0:
            tri.reverse()
        if len(output) >= max_faces:
            raise ValueError("straight-flight face budget exhausted")
        output.append(tri)
        owners.append(mask)
        lineage.append(parent)

    remaining = []
    for face, mask, parent in records:
        if selected is not None and _geometry(face, parent, planar) not in selected:
            remaining.append((face, mask, parent))
            continue
        polygons = _clip_polygons(face, station, cut, crossing)
        for polygon in polygons:
            for i in range(1, len(polygon) - 1):
                emit([polygon[0], polygon[i], polygon[i + 1]], mask, parent)
    for face, mask, parent in remaining:
        for tri in _conform_face(face, planar, planar_cache, crossing):
            emit(tri, mask, parent)
    return output, owners, lineage, len(cache), len(planar_cache)


def _frame(flight):
    return tuple(flight["origin_xy_m"]), tuple(flight["tangent"])
