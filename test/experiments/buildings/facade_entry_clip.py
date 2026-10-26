"""Preserve sloped source wall triangles outside the one existing entrance cell."""

import numpy as np
import shapely
from facade_entry_geometry import basis_for


def remainder(mesh, selected, entry, *, excluded_columns=(), excluded_cells=()):
    frame = entry["frame"]
    basis = basis_for(frame)
    origin = np.r_[frame["a"], 0.0]
    scale = np.array([entry["cell_width_m"], entry["floor_m"]])
    low = entry["column"] * scale[0]
    cell = shapely.box(low, 0.0, low + scale[0], scale[1])
    if excluded_columns or excluded_cells:
        cell = shapely.union_all(
            [cell]
            + [
                shapely.box(c * scale[0], 0.0, (c + 1) * scale[0], scale[1])
                for c in excluded_columns
            ]
            + [
                shapely.box(
                    c * scale[0], r * scale[1], (c + 1) * scale[0], (r + 1) * scale[1]
                )
                for c, r in excluded_cells
            ]
        )
    indices = np.flatnonzero(selected)
    if len(indices) > 256:
        raise ValueError("Entrance clipping exceeds 256 source triangles")
    vertices, normals, uv, extents, materials = [], [], [], [], []
    intersections = []
    dropped = 0.0
    for index in indices:
        face = mesh["faces"][index]
        world = mesh["vertices"][face]
        local = (world - origin) @ basis.T
        triangle = shapely.Polygon(local[:, :2])
        cut = triangle.intersection(cell)
        intersections.append(cut)
        if cut.area == 0:
            vertices.extend(world)
            normals.extend(mesh["normals"][face])
            uv.extend(mesh["uv"][face])
            extents.extend(mesh["facade_extent"][face])
            materials.append(mesh["materials"][index])
            continue
        remaining = triangle.difference(cell)
        for polygon in shapely.get_parts(remaining):
            if polygon.geom_type != "Polygon" or polygon.is_empty:
                continue
            for t in shapely.get_parts(shapely.constrained_delaunay_triangles(polygon)):
                xy = np.asarray(shapely.orient_polygons(t).exterior.coords)[:3]
                coordinates = xy / scale
                d = coordinates[1:] - coordinates[0]
                determinant = abs(d[0, 0] * d[1, 1] - d[0, 1] * d[1, 0])
                if determinant <= 1e-12:
                    dropped += t.area
                    continue
                vertices.extend(np.c_[xy, np.zeros(3)] @ basis + origin)
                normals.extend(np.tile(basis[2], (3, 1)))
                uv.extend(coordinates)
                extents.extend(np.tile([frame["columns"], frame["rows"], 0.0], (3, 1)))
                materials.append(mesh["materials"][index])
    union = shapely.union_all(intersections)
    if (
        abs(union.area - cell.area) > 1e-7
        or abs(sum(p.area for p in intersections) - union.area) > 1e-7
    ):
        raise ValueError("Source triangles do not cover the entrance cell exactly once")
    if dropped > 1e-7:
        raise ValueError("Entrance clipping exceeds the existing area precision")
    points = np.asarray(vertices).reshape(-1, 3)
    return {
        "vertices": points,
        "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        "normals": np.asarray(normals).reshape(-1, 3),
        "uv": np.asarray(uv).reshape(-1, 2),
        "facade_extent": np.asarray(extents).reshape(-1, 3),
        "materials": np.asarray(materials, dtype=np.uint16),
    }, dropped
