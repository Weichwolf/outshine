"""Counterexample candidate: source-owned nodes do not ensure admissible faces.

The fixed half-tile lattice is a generation recipe, independent of camera LOD.
Mercator coordinates parameterize affine ECEF triangles. No fitted cell, pair
owner, native product or hydrological correctness is asserted here.
"""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.places.camera import ecef
from outshine_ref.places.sources import tile_geography, tile_tuple
from outshine_ref.water.frame import inverse_frame, tangent_directions
from physical_boundary import from_world
from water_cell_height import project
from water_contact_band import physical_audit
from water_grid_height import body_profile, normal_extension
from water_owner_inputs import fixed_owner_inputs

RECIPE = "water-source-owned-half-tile-nodes-v1"
SUBDIVISIONS = 2


@dataclass(frozen=True)
class Node:
    key: tuple
    state: str
    plan: object
    world: np.ndarray | None
    level_m: float | None
    courses: tuple
    report: dict


def cell_faces(cell):
    z, x, y = tile_tuple(cell)
    result = []
    for a in range(x * SUBDIVISIONS, (x + 1) * SUBDIVISIONS):
        for b in range(y * SUBDIVISIONS, (y + 1) * SUBDIVISIONS):
            for corners in (
                ((a, b), (a + 1, b + 1), (a + 1, b)),
                ((a, b), (a, b + 1), (a + 1, b + 1)),
            ):
                result.append(tuple((z, *corner) for corner in corners))
    return tuple(result)


def node_plan(key, receipts, epoch):
    if (
        not isinstance(key, tuple)
        or len(key) != 3
        or any(type(v) is not int for v in key)
    ):
        raise ValueError("integer global lattice node key required")
    z, x, y = key
    return fixed_owner_inputs(
        (z, x // SUBDIVISIONS, y // SUBDIVISIONS),
        receipts,
        epoch=epoch,
        model_revision=RECIPE,
    )


def evaluate_node(key, receipts, epoch, snapshots, data, course_order):
    plan = node_plan(key, receipts, epoch)
    if plan.state != "ready":
        return Node(key, plan.state, plan, None, None, (), {})
    z, x, y = key
    lat, lon = tile_geography([[x / SUBDIVISIONS, y / SUBDIVISIONS]], 1, (z, 0, 0))
    anchor = np.array([lon[0], lat[0]])
    polygons = [
        p.polygon
        for tile in plan.tiles
        for p in snapshots[tile].pieces
        if p.info["kind"] == "river"
        and p.info.get("bridge") is False
        and p.info.get("tunnel") is False
        and not p.info.get("crossing")
    ]
    projected = []
    for polygon in polygons:
        rings = [
            project(np.asarray(r.coords), z, anchor)[:, :2]
            for r in (polygon.exterior, *polygon.interiors)
        ]
        projected.append(shapely.Polygon(rings[0], rings[1:]))
    original = [i for tile in plan.tiles for i in course_order[tile]]
    scoped = dict(data, courses=data["courses"][original])
    try:
        profile, report = body_profile(scoped, shapely.union_all(projected), anchor)
        height, work = normal_extension(profile, np.zeros((1, 2)))
    except ValueError as error:
        return Node(
            key, "unresolved_recipe", plan, None, None, (), {"reason": str(error)}
        )
    courses = tuple(original[i] for i in report["selected_original_courses"])
    report = dict(
        report,
        target_work=work,
        examined_original_courses=len(original),
        examined_original_course_rows=original,
        examined_raw_polygon_parts=len(polygons),
        examined_raw_polygon_coordinates=int(
            shapely.get_num_coordinates(polygons).sum()
        ),
    )
    world = np.asarray(ecef(lat[0], lon[0], float(height[0])))
    world.setflags(write=False)
    return Node(key, "ready", plan, world, float(height[0]), courses, report)


def parameter_triangle(keys):
    return np.array([key[1:] for key in keys], dtype=float) / SUBDIVISIONS


def sample_face(keys, world, xy):
    """One globally keyed facet, with no consumer-dependent ENU lifting."""
    triangle = parameter_triangle(keys)
    xy = np.asarray(xy, dtype=float)
    if xy.ndim != 2 or xy.shape[1] != 2 or not np.isfinite(xy).all():
        raise ValueError("finite Mercator query pairs required")
    weights = np.linalg.solve((triangle[1:] - triangle[0]).T, (xy - triangle[0]).T).T
    if np.any(weights < -1e-12) or np.any(weights.sum(axis=1) > 1 + 1e-12):
        raise ValueError("query outside canonical facet")
    world = np.asarray(world, dtype=float)
    if world.shape != (3, 3) or not np.isfinite(world).all():
        raise ValueError("three finite ECEF node positions required")
    return world[0] + weights @ (world[1:] - world[0])


def point_audit(keys, world, anchor, rows, xy):
    sample = sample_face(keys, world, np.asarray(xy).reshape(1, 2))
    local = from_world(anchor, sample)
    triangle = from_world(anchor, world)
    normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
    _, _, up = inverse_frame(anchor, local)
    tangent = tangent_directions(np.repeat(up, len(rows), axis=0), rows[:, 1:4])
    grade = -(tangent @ normal) / float(up[0] @ normal)
    violation = max(
        float((rows[:, 4] - grade).max()), float((grade - rows[:, 5]).max()), 0.0
    )
    return {
        "directed_grades_m_per_m": grade.tolist(),
        "physical_grade_violation": violation,
        "scope": "single actual surface point, not a whole-face maximum",
    }, {
        "sample_parameter": np.asarray(xy),
        "sample_world": sample,
        "sample_grades": grade,
    }


def audit_face(keys, nodes, data, *, water_xy=None):
    selected = [nodes[key] for key in keys]
    if any(node.state != "ready" for node in selected):
        return {"state": "pending_nodes", "physical": None}, {}
    centre = parameter_triangle(keys).mean(axis=0)
    lat, lon = tile_geography([centre], 1, (keys[0][0], 0, 0))
    anchor = np.array([lon[0], lat[0]])
    world = np.array([node.world for node in selected])
    mesh = Mesh(from_world(anchor, world), np.array([[0, 1, 2]], np.uint32))
    source_indices = sorted({i for node in selected for i in node.courses})
    original_world = []
    for i in source_indices:
        course = data["courses"][i]
        first, count = int(course["first_point"]), int(course["point_count"])
        points = data["points_lat_lon"][first : first + count]
        levels = data["levels"][
            int(course["first_level"]) : int(course["first_level"]) + count
        ]
        original_world.extend(ecef(points[:, 0], points[:, 1], levels))
    source_up = from_world(anchor, np.asarray(original_world))[:, 2]
    bounds = np.array([source_up.min() - 20, source_up.max() + 20])
    rows = np.array([[0, 1, 0, 0, -0.01, 0.01], [0, 0, 1, 0, -0.01, 0.01]])
    physical = physical_audit(mesh, anchor, rows, bounds)
    sampled, sample_arrays = point_audit(
        keys, world, anchor, rows, centre if water_xy is None else water_xy
    )
    result = {
        "state": "admitted" if physical["admitted"] else "physical_rejection",
        "physical": physical,
        "sampled": sampled,
        "source_course_rows": source_indices,
        "bounds_scope": "union of the nodes' original admitted course endpoints, ENU Up +/-20m",
    }
    return result, {
        "world": world,
        "local": mesh.vertices,
        "anchor": anchor,
        "rows": rows,
        "bounds": bounds,
        **sample_arrays,
    }
