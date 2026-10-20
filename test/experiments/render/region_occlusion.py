"""Strict depth-separated extended projections; opaque regional visibility probe."""

import itertools

import numpy as np
import shapely
from beam_intervals import Beams, add, divide, exact, mul, sub
from beam_patch_fusion import Patches
from current_visibility import mesh_boxes


def project(points, eyes, plane):
    """Outward projection intervals for represented world points and fixed eyes."""
    points, eyes = np.asarray(points, float), np.asarray(eyes, float)
    p, e = points[None], eyes[:, None]
    denominator = sub(exact(p[:, :, 2]), exact(e[:, :, 2]))
    if denominator[0].min() <= 0:
        raise ValueError("extended projection crosses observer depth")
    ratio = divide(sub(exact(plane), exact(e[:, :, 2])), denominator)
    result = add(
        exact(e[:, :, :2]),
        mul(
            tuple(r[:, :, None] for r in ratio),
            sub(exact(p[:, :, :2]), exact(e[:, :, :2])),
        ),
    )
    if not all(np.isfinite(r).all() for r in result):
        raise ValueError("nonfinite extended projection")
    return result


def inner_projection(polygon, eyes, plane, guard):
    low, high = project(polygon, eyes, plane)
    middle = low / 2 + high / 2
    # Convex projected patches: vertex perturbation bounds Hausdorff distance.
    radius = np.linalg.norm(np.maximum(middle - low, high - middle), axis=2).max()
    if radius > guard / 8:
        return shapely.Polygon()
    result = None
    for xy in middle:
        p = shapely.Polygon(xy).buffer(-guard)
        result = p if result is None else shapely.intersection(result, p)
        if result.is_empty:
            break
    return result


def outer_projection(points, eyes, plane, guard):
    low, high = project(points, eyes, plane)
    choices = np.array(list(itertools.product((0, 1), repeat=2)))
    boxes = np.where(choices[:, None, None, :], high, low).reshape(-1, 2)
    return shapely.MultiPoint(boxes).convex_hull.buffer(guard)


def compile_projection(
    source, observer_box, *, certified_opaque=False, tight_hulls=False, plane=None
):
    if certified_opaque is not True:
        raise ValueError("explicit opaque regional control required")
    beams = Beams(source["positions"], source["indices"], observer_box)
    patches = Patches(source, beams, with_polygons=True)
    vertices = source["positions"].astype(float) @ beams.basis.T
    sources, owners = np.unique(source["binding"][:, 1], return_inverse=True)
    low, high = mesh_boxes(vertices, source["indices"], owners, len(sources))
    if plane is None:
        levels = np.unique((low[:, 2] + high[:, 2]) / 2)
        middle = max(1, len(levels) // 2)
        plane = float((levels[middle - 1] + levels[min(middle, len(levels) - 1)]) / 2)
    if not np.isfinite(plane) or plane <= beams.observer[1, 2]:
        raise ValueError("projection plane must be beyond the complete observer region")
    eyes = np.unique(
        np.array(list(itertools.product(*zip(*beams.observer, strict=True)))), axis=0
    )
    all_q = project(np.concatenate((low, high)), eyes, plane)
    magnitude = max(1.0, *(abs(q).max() for q in all_q))
    guard = 2048 * np.finfo(float).eps * magnitude
    triangles = vertices[source["indices"]]
    front = beams.numerator[1] < 0
    before = triangles[:, :, 2].max(1) < plane - guard
    pair_before = patches.polygons[:, :, 2].max(1) < plane - guard
    paired = np.zeros(len(triangles), bool)
    paired[patches.pairs[pair_before].ravel()] = True
    polygons = list(patches.polygons[pair_before]) + list(
        triangles[front & before & ~paired]
    )
    projected = [inner_projection(p, eyes, plane, guard) for p in polygons]
    fused = shapely.union_all(projected)
    candidates = low[:, 2] > plane + guard
    hidden = np.zeros(len(sources), bool)
    hidden_bounds = np.zeros_like(hidden)
    cube = np.array(list(itertools.product((0, 1), repeat=3)))
    query_vertices = 0
    for i in np.flatnonzero(candidates):
        bounds_query = outer_projection(
            low[i] + cube * (high[i] - low[i]), eyes, plane, guard
        )
        hidden_bounds[i] = (
            fused.covers(bounds_query) and fused.boundary.distance(bounds_query) > guard
        )
        if tight_hulls:
            used = np.unique(source["indices"][owners == i])
            points = vertices[used]
        else:
            points = None
        query = (
            outer_projection(points, eyes, plane, guard)
            if tight_hulls
            else bounds_query
        )
        query_vertices += len(shapely.get_coordinates(query))
        # Positive clearance rejects ambiguous GEOS boundary decisions.
        hidden[i] = fused.covers(query) and fused.boundary.distance(query) > guard
    return {
        "sources": sources,
        "hidden": hidden,
        "hidden_bounds": hidden_bounds,
        "basis": beams.basis,
        "plane": plane,
        "guard_projected_m": guard,
        "fused": fused,
        "observer_box": np.asarray(observer_box, float),
        "source_bounds": np.stack((low, high), axis=1),
        "work": {
            "input_sources": len(sources),
            "input_triangles": len(triangles),
            "certified_patch_pairs": len(patches.pairs),
            "selected_polygons": len(polygons),
            "nonempty_projected_polygons": sum(not p.is_empty for p in projected),
            "fused_vertices": len(shapely.get_coordinates(fused)),
            "queried_sources": int(candidates.sum()),
            "query_polygon_vertices": query_vertices,
            "hidden_sources": int(hidden.sum()),
            "aabb_hidden_sources": int(hidden_bounds.sum()),
            "hidden_triangles": int(
                np.isin(source["binding"][:, 1], sources[hidden]).sum()
            ),
            "beam_interval_bytes": beams.prepared_array_bytes,
            "patch_polygon_bytes": patches.polygons.nbytes,
        },
        "scope": "strict depth-separated opaque projection; numeric GEOS guard, not a universal finite-precision proof, native or world admission",
    }
