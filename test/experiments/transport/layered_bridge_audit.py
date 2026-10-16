"""Whole-field/cross-section checks on actual saved facets, including Float32 facets."""

import itertools

import numpy as np
import shapely
from foot_ramp_audit import shell_ownership
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


def field_error(
    cap, table, problem, field, *, bottom=False, tolerance_m=1e-7, region=None
):
    points = cap.vertices[cap.triangles]
    normal = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if np.any(normal[:, 2] <= 0):
        raise ValueError("layered cap has a degenerate/reversed projected facet")
    gradients = -normal[:, :2] / normal[:, 2, None]
    shapes = shapely.polygons(points[:, :, :2])
    if region is not None:
        shapes = shapely.intersection(shapes, region)
    target_area = float(shapely.area(shapes).sum())
    origin, axis = problem["origin"], problem["axis"]
    cross = np.array([-axis[1], axis[0]])
    width = (cap.vertices[:, :2] - origin) @ cross
    error, area = 0.0, 0.0
    for tri in field.vertices[field.triangles]:
        region = shapely.Polygon(tri[:, :2])
        gradient = np.linalg.solve(tri[1:, :2] - tri[0, :2], tri[1:, 2] - tri[0, 2])
        for a, b in itertools.pairwise(table):
            local = np.array(
                [
                    [a[0], width.min() - 1],
                    [b[0], width.min() - 1],
                    [b[0], width.max() + 1],
                    [a[0], width.max() + 1],
                ]
            )
            strip = shapely.Polygon(local @ np.array([axis, cross]) + origin)
            pieces = shapely.intersection(shapes, region.intersection(strip))
            for i in np.flatnonzero(shapely.area(pieces) > 1e-13):
                xy = shapely.get_coordinates(pieces[i])
                relative = np.interp((xy - origin) @ axis, table[:, 0], table[:, 1])
                if bottom:
                    relative = np.maximum(
                        relative - problem["recipe"]["deck_thickness_m"], 0
                    )
                expected = tri[0, 2] + (xy - tri[0, :2]) @ gradient + relative
                actual = points[i, 0, 2] + (xy - points[i, 0, :2]) @ gradients[i]
                error = max(error, float(abs(actual - expected).max()))
                area += pieces[i].area
    if error > tolerance_m or abs(area - target_area) > 1e-7:
        raise ValueError(
            "layered whole affine field differs or has uncovered/duplicate area"
        )
    return error


def frame_proof(original, actual, problem):
    points = actual.vertices[actual.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if np.any(normals[:, 2] <= 0):
        raise ValueError("actual cap projection is degenerate/reversed")
    gradients = -normals[:, :2] / normals[:, 2, None]
    shapes = shapely.polygons(original.vertices[original.triangles, :2])
    covered = np.zeros(len(shapes), bool)
    worst = 0.0
    recipe = problem["recipe"]
    for row in problem["patches"]:
        region = shapely.from_wkb(bytes.fromhex(row["region_wkb_hex"]))
        selected = shapely.area(shapely.intersection(shapes, region)) > 1e-12
        if not np.any(selected):
            continue
        covered |= selected
        frame = np.array(row["frame"])
        frame[0] *= (recipe["long_grade"] - recipe["margin"]) / recipe["long_grade"]
        frame[1] *= (recipe["cross_grade"] - recipe["margin"]) / recipe["cross_grade"]
        worst = max(
            worst, float(np.linalg.norm(gradients[selected] @ frame.T, axis=1).max())
        )
    if not np.all(covered) or worst > 1 + 1e-7:
        raise ValueError(
            "actual whole source section grade/crossfall exceeds its recipe"
        )
    return worst


def expected_coverage(problem, table):
    axis, origin = problem["axis"], problem["origin"]
    normal = np.array([-axis[1], axis[0]])
    cross = (shapely.get_coordinates(problem["domain"]) - origin) @ normal
    pieces = []
    for a, b in itertools.pairwise(table):
        if max(a[1], b[1]) <= 1e-9:
            continue
        xy = np.array(
            [
                [a[0], cross.min() - 1],
                [b[0], cross.min() - 1],
                [b[0], cross.max() + 1],
                [a[0], cross.max() + 1],
            ]
        )
        pieces.append(
            problem["domain"].intersection(
                shapely.Polygon(xy @ np.array([axis, normal]) + origin)
            )
        )
    return shapely.union_all(pieces)


def constraint_proof(table, problem):
    if table[0, 0] > problem["stations"][0] or table[-1, 0] < problem["stations"][-1]:
        raise ValueError("saved profile table omits declared ground ports")
    worst = 0.0
    for row in problem["bounds"]:
        lo, hi = row["range_m"]
        points = np.r_[lo, hi, table[(table[:, 0] > lo) & (table[:, 0] < hi), 0]]
        heights = np.interp(points, table[:, 0], table[:, 1])
        error = (
            float(heights.max() - row["value_m"])
            if row["kind"] == "maximum"
            else float(row["value_m"] - heights.min())
        )
        worst = max(worst, error)
    if worst > 1e-7:
        raise ValueError("whole saved source contact/clearance profile bound violated")
    return worst


def audit(ribbon, table, problem, fixture):
    field = Mesh(np.array(fixture["field_vertices"]), np.array(fixture["field_faces"]))
    if ribbon.shape.symmetric_difference(expected_coverage(problem, table)).area > 1e-7:
        raise ValueError("layered body lacks whole positive-height source coverage")
    fields = [
        field_error(ribbon.upper, table, problem, field),
        field_error(ribbon.lower, table, problem, field, bottom=True),
    ]
    for cap in (ribbon.lower, ribbon.upper):
        polygons = shapely.polygons(cap.vertices[cap.triangles, :2])
        if (
            not shapely.coverage_is_valid(polygons)
            or surface_domain(cap).symmetric_difference(ribbon.shape).area > 1e-7
        ):
            raise ValueError("layered cap is not a conforming whole coverage")
    shell_ownership(ribbon)
    volume = audit_closed(ribbon.body)
    xyz = ribbon.upper.vertices[ribbon.upper.triangles]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    height = ribbon.upper.vertices[:, 2] - ribbon.lower.vertices[:, 2]
    integral = float(
        np.sum(normal[:, 2] / 2 * height[ribbon.upper.triangles].mean(axis=1))
    )
    if volume <= 0 or abs(volume - integral) > 1e-7:
        raise ValueError("layered shell volume differs from its cap integral")
    cast = Mesh(
        ribbon.upper.vertices.astype(np.float32).astype(float), ribbon.upper.triangles
    )
    low = Mesh(
        ribbon.lower.vertices.astype(np.float32).astype(float), ribbon.lower.triangles
    )
    p = ribbon.body.vertices[ribbon.body.triangles]
    q = ribbon.body.vertices.astype(np.float32).astype(float)[ribbon.body.triangles]
    a, b = (
        np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]),
        np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0]),
    )
    if np.any(np.einsum("ij,ij->i", a, b) <= 0):
        raise ValueError("Float32 layered body collapses or inverts")
    return {
        "volume_m3": volume,
        "volume_error_m3": abs(volume - integral),
        "whole_field_error_m": max(fields),
        "whole_section_ratio": frame_proof(ribbon.upper, ribbon.upper, problem),
        "float32_section_ratio": frame_proof(ribbon.upper, cast, problem),
        "float32_upper_field_error_m": field_error(
            cast, table, problem, field, tolerance_m=0.001
        ),
        "float32_lower_field_error_m": field_error(
            low, table, problem, field, bottom=True, tolerance_m=0.001
        ),
        "float32_xy_drift_m": float(
            np.linalg.norm(
                cast.vertices[:, :2] - ribbon.upper.vertices[:, :2], axis=1
            ).max()
        ),
        "whole_profile_bound_violation_m": constraint_proof(table, problem),
    }
