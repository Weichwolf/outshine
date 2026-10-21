"""Retriangulate one source/stair-level/loft-plane region at a time."""

from collections import defaultdict

import numpy as np
import shapely
from stair_band_rings import clean_ring


def _groups(points, faces, owners, steps, flights):
    bits = {1 << owner: flight for owner, flight in flights.items()}
    selected = np.array(
        [
            int(o) in bits and (steps is None or steps[i] >= 0)
            for i, o in enumerate(owners)
        ]
    )
    groups = defaultdict(list)
    for i in np.flatnonzero(selected):
        flight = bits[int(owners[i])]
        station = (points[faces[i], :2] - flight["origin_xy_m"]) @ flight["tangent"]
        start, end = flight["start_cut_m"], flight["end_cut_m"]
        mean = float(station.mean())
        region = 0 if mean <= start else (2 if mean >= end else 1)
        if (
            (region == 0 and station.max() > start + 1e-8)
            or (region == 2 and station.min() < end - 1e-8)
            or (
                region == 1
                and (station.min() < start - 1e-8 or station.max() > end + 1e-8)
            )
        ):
            raise ValueError("prepared band face crosses a loft plane boundary")
        groups[int(owners[i]), None if steps is None else int(steps[i]), region].append(
            int(i)
        )
    return selected, groups


def _plane_residual(points, vertices, flight, recipe, direction):
    station = (points[vertices, :2] - flight["origin_xy_m"]) @ flight["tangent"]
    start, end = flight["start_cut_m"], flight["end_cut_m"]
    ratio = np.clip((station - start) / (end - start), 0, 1)
    first, last = (
        (recipe["low"], recipe["high"])
        if direction > 0
        else (recipe["high"], recipe["low"])
    )
    expected = first + ratio * (last - first)
    residual = float(abs(points[vertices, 2] - expected).max())
    if residual > 1e-8:
        raise ValueError("band remeshing requires its declared flat/linear/flat field")
    return residual


def _triangulate(shape, protected, lookup):
    polygons, visits, removed, displacement, area_error = [], 0, 0, 0.0, 0.0
    for polygon in shapely.get_parts(shape):
        if polygon.geom_type != "Polygon":
            raise ValueError("stair band union must consist of polygons")
        rings = []
        for ring in [polygon.exterior, *polygon.interiors]:
            simplified, note = clean_ring(list(ring.coords), protected)
            visits += note["ring_visits"]
            removed += note["removed_ring_vertices"]
            displacement = max(displacement, note["maximum_chain_vertex_deviation_m"])
            rings.append(simplified)
        result = shapely.Polygon(rings[0], rings[1:])
        if not result.is_valid or len(result.interiors) != len(polygon.interiors):
            raise ValueError("band simplification changed polygon validity or holes")
        error = result.symmetric_difference(polygon).area
        area_error += error
        if area_error > 1e-7:
            raise ValueError("band simplification changed its prepared coverage")
        polygons.extend(
            shapely.get_parts(shapely.constrained_delaunay_triangles(result))
        )
    faces = []
    for polygon in polygons:
        xy = np.array(polygon.exterior.coords[:-1])
        if len(xy) != 3:
            raise ValueError("band triangulation must return indexed triangles")
        try:
            face = [lookup[tuple(point)] for point in xy]
        except KeyError as error:
            raise ValueError(
                "band triangulation introduced an unowned vertex"
            ) from error
        a, b = xy[1] - xy[0], xy[2] - xy[0]
        signed = a[0] * b[1] - a[1] * b[0]
        if signed == 0:
            raise ValueError("band triangulation returned a zero projected face")
        faces.append(face if signed > 0 else face[::-1])
    return faces, {
        "ring_visits": visits,
        "removed_ring_vertices": removed,
        "maximum_chain_vertex_deviation_m": displacement,
        "group_area_error_m2": area_error,
    }


def remesh(points, faces, owners, steps, levels, note, plan):
    if not 1 <= len(points) <= 65536 or not 1 <= len(faces) <= 8192:
        raise ValueError("bounded indexed cap required for band remeshing")
    if not np.isfinite(points).all() or points.shape[1:] != (3,):
        raise ValueError("finite reference vertices required for band remeshing")
    if (
        faces.ndim != 2
        or faces.shape[1:] != (3,)
        or faces.dtype.kind not in "iu"
        or faces.min() < 0
        or faces.max() >= len(points)
        or owners.shape != (len(faces),)
        or steps.shape != owners.shape
        or owners.dtype.kind not in "iu"
        or steps.dtype.kind not in "iu"
    ):
        raise ValueError("indexed source and step ownership must match the cap")
    flights = {flight["owner"]: flight for flight in plan["stair_lofts"]}
    recipes = {recipe["owner"]: recipe for recipe in note["stairs"]}
    if not flights or flights.keys() != recipes.keys():
        raise ValueError("every stair band needs one explicit station loft")
    selected, groups = _groups(points, faces, owners, steps, flights)
    protected_ids = set(np.unique(faces[~selected]).tolist())
    protected_xy = {tuple(points[v, :2]) for v in protected_ids}
    output, masks, bands = (
        faces[~selected].tolist(),
        owners[~selected].tolist(),
        steps[~selected].tolist(),
    )
    work = {
        "groups": len(groups),
        "input_faces": len(faces),
        "ring_visits": 0,
        "removed_ring_vertices": 0,
        "maximum_chain_vertex_deviation_m": 0.0,
        "group_area_error_m2": 0.0,
        "maximum_loft_height_residual_m": 0.0,
    }
    for (mask, step, _region), indices in groups.items():
        owner = mask.bit_length() - 1
        vertices = np.unique(faces[indices])
        recipe = recipes[owner]
        offset = recipe["level_offset"]
        if not offset <= step <= offset + recipe["risers"]:
            raise ValueError("remeshed band level belongs to another stair source")
        lower = levels[step - 1] if step > offset else levels[step]
        if (
            points[vertices, 2].min() < lower - 1e-8
            or points[vertices, 2].max() > levels[step] + 1e-8
        ):
            raise ValueError("reference band leaves its owned height interval")
        residual = _plane_residual(
            points,
            vertices,
            flights[owner],
            recipes[owner],
            plan["stair_directions"][owner],
        )
        work["maximum_loft_height_residual_m"] = max(
            work["maximum_loft_height_residual_m"], residual
        )
        lookup = {tuple(points[v, :2]): int(v) for v in vertices}
        if len(lookup) != len(vertices):
            raise ValueError("one stair band cannot own ambiguous XY identities")
        shape = shapely.union_all(shapely.polygons(points[faces[indices], :2]))
        rebuilt, measured = _triangulate(shape, protected_xy, lookup)
        if not (set(vertices.tolist()) & protected_ids) <= set(
            np.array(rebuilt).ravel().tolist()
        ):
            raise ValueError("band remeshing lost a shared indexed vertex")
        if len(output) + len(rebuilt) > 8192:
            raise ValueError("remeshed tread face budget")
        output.extend(rebuilt)
        masks.extend([mask] * len(rebuilt))
        bands.extend([step] * len(rebuilt))
        for key, value in measured.items():
            work[key] = (
                max(work[key], value)
                if key.startswith("maximum")
                else work[key] + value
            )
    output = np.array(output, dtype=int)
    retained, inverse = np.unique(output, return_inverse=True)
    work["output_faces"] = len(output)
    work["input_vertices"] = len(points)
    work["output_vertices"] = len(retained)
    retained_points = points[retained]
    work["selected_array_copy_bytes"] = int(retained_points.nbytes + inverse.nbytes)
    work["copy_scope"] = (
        "Final selected vertex and face arrays only; not all GEOS/NumPy allocations."
    )
    return (
        retained_points,
        inverse.reshape(-1, 3),
        np.array(masks),
        np.array(bands),
        levels,
        dict(note, band_remesh=work),
    )
