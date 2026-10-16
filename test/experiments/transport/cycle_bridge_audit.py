"""Independent whole-cap field, source-crossing, mouth and physical shell audit."""

import itertools

import numpy as np
import shapely
from foot_ramp_audit import shell_ownership
from outshine_ref.contracts import Mesh
from retained_surface import surface_domain
from surface_solid_checks import audit_closed


def reference(fixture):
    tri = np.asarray(fixture["field_vertices"])[np.asarray(fixture["field_faces"])[0]]
    normal = np.cross(tri[1] - tri[0], tri[2] - tri[0])
    gradient = -normal[:2] / normal[2]
    bridge = next(r for r in fixture["sources"] if r["role"] == "bridge")
    origin = np.asarray(bridge["coordinates"])[0]
    axis = np.asarray(bridge["coordinates"])[-1] - origin
    axis /= np.linalg.norm(axis)
    return tri[0], gradient, origin, axis


def whole_field(cap, table, ref, *, bottom=False, thickness=0.35, tolerance_m=1e-7):
    anchor, gradient, origin, axis = ref
    triangles = cap.vertices[cap.triangles]
    shapes = shapely.polygons(triangles[:, :, :2])
    coefficients = np.linalg.solve(
        np.dstack([triangles[:, :, :2], np.ones(triangles.shape[:2])]),
        triangles[:, :, 2, None],
    )[:, :, 0]
    xy = cap.vertices[:, :2]
    normal = np.array([-axis[1], axis[0]])
    ext = (xy - origin) @ normal
    error, covered = 0.0, 0.0
    for first, last in itertools.pairwise(table):
        local = np.array(
            [
                [first[0], ext.min() - 1],
                [last[0], ext.min() - 1],
                [last[0], ext.max() + 1],
                [first[0], ext.max() + 1],
            ]
        )
        region = shapely.Polygon(local @ np.array([axis, normal]) + origin)
        parts = shapely.intersection(shapes, region)
        for index in np.flatnonzero(shapely.area(parts) > 1e-14):
            points = shapely.get_coordinates(parts[index])
            stations = (points - origin) @ axis
            relative = np.interp(stations, table[:, 0], table[:, 1])
            if bottom:
                relative = np.maximum(relative - thickness, 0)
            target = anchor[2] + (points - anchor[:2]) @ gradient + relative
            actual = points @ coefficients[index, :2] + coefficients[index, 2]
            error = max(error, float(abs(target - actual).max()))
            covered += parts[index].area
    if error > tolerance_m or abs(covered - surface_domain(cap).area) > 1e-7:
        raise ValueError("whole bridge cap field differs or is incomplete")
    return error


def source_contacts(fixture, table, domain, ref):
    _anchor, _gradient, origin, axis = ref
    records = []
    for row in fixture["sources"]:
        if row["role"] in ("bridge", "approach"):
            continue
        shape = (
            shapely.LineString(row["coordinates"])
            .buffer(
                row["width_m"] / 2, quad_segs=2, cap_style="round", join_style="bevel"
            )
            .intersection(domain)
        )
        if row["role"] == "underpass":
            shape = shapely.union_all(
                [
                    shape,
                    shapely.from_wkb(
                        bytes.fromhex(row["actual_ground_traffic_wkb_hex"])
                    ),
                ]
            )
        if shape.area <= 1e-9:
            raise ValueError("source crossing/connection footprint absent")
        stations = (shapely.get_coordinates(shape) - origin) @ axis
        knots = table[
            (table[:, 0] > stations.min()) & (table[:, 0] < stations.max()), 0
        ]
        probes = np.r_[stations, knots]
        relative = np.interp(probes, table[:, 0], table[:, 1])
        if row["role"] == "ground_joint":
            error = float(abs(relative).max())
            if error > 1e-7:
                raise ValueError("whole chosen ground junction acquires a height step")
            records.append({"source_id": row["source_id"], "ground_step_m": error})
        else:
            clearance = float(relative.min() - fixture["recipe"]["deck_thickness_m"])
            if clearance < (
                fixture["recipe"]["nonmotorized_headroom_m"]
                + fixture["recipe"]["clearance_margin_m"]
                - 1e-7
            ):
                raise ValueError("whole nonmotorized underpass clearance violated")
            records.append({"source_id": row["source_id"], "clearance_m": clearance})
    return records


def expected_coverage(fixture, table, ref):
    _anchor, _gradient, origin, axis = ref
    normal = np.array([-axis[1], axis[0]])
    shapes = [
        shapely.LineString(r["coordinates"]).buffer(
            r["width_m"] / 2, cap_style="flat", join_style="mitre", mitre_limit=2
        )
        for r in fixture["sources"]
        if r["role"] in ("bridge", "approach")
    ]
    domain = shapely.union_all(shapes)
    extent = (shapely.get_coordinates(domain) - origin) @ normal
    pieces = []
    for a, b in itertools.pairwise(table):
        if max(a[1], b[1]) <= 1e-9:
            continue
        points = np.array(
            [
                [a[0], extent.min() - 1],
                [b[0], extent.min() - 1],
                [b[0], extent.max() + 1],
                [a[0], extent.max() + 1],
            ]
        )
        pieces.append(
            domain.intersection(
                shapely.Polygon(points @ np.array([axis, normal]) + origin)
            )
        )
    return domain, shapely.union_all(pieces)


def float32_caps(fixture, ribbon, table, ref):
    top, low = [
        Mesh(cap.vertices.astype(np.float32).astype(float), cap.triangles)
        for cap in (ribbon.upper, ribbon.lower)
    ]
    for cap in (top, low):
        points = cap.vertices[cap.triangles]
        normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        if np.any(normals[:, 2] <= 0):
            raise ValueError("Float32 bridge cap projection degenerates or reverses")
    points = top.vertices[top.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    grade = float(np.max(np.linalg.norm(normals[:, :2], axis=1) / normals[:, 2]))
    if grade > fixture["recipe"]["maximum_grade"] + 1e-8:
        raise ValueError("Float32 whole bridge grade exceeds the unchanged recipe")
    lower_error = whole_field(
        low,
        table,
        ref,
        bottom=True,
        thickness=fixture["recipe"]["deck_thickness_m"],
        tolerance_m=0.001,
    )
    return {
        "maximum_whole_face_grade": grade,
        "lower_field_error_m": lower_error,
        "upper_field_error_m": whole_field(top, table, ref, tolerance_m=0.001),
        "xy_drift_m": float(
            np.linalg.norm(
                top.vertices[:, :2] - ribbon.upper.vertices[:, :2], axis=1
            ).max()
        ),
        "relative_profile_lipschitz": float(
            np.max(abs(np.diff(table[:, 1]) / np.diff(table[:, 0])))
        ),
    }


def audit(fixture, ribbon, table, complete_domain):
    ref = reference(fixture)
    thickness = fixture["recipe"]["deck_thickness_m"]
    field_error = max(
        whole_field(ribbon.upper, table, ref),
        whole_field(ribbon.lower, table, ref, bottom=True, thickness=thickness),
    )
    expected_domain, expected_body = expected_coverage(fixture, table, ref)
    if (
        ribbon.shape.symmetric_difference(expected_body).area > 1e-7
        or complete_domain.symmetric_difference(expected_domain).area > 1e-7
    ):
        raise ValueError("bridge body lacks complete positive-height source coverage")
    for cap in (ribbon.upper, ribbon.lower):
        shapes = shapely.polygons(cap.vertices[cap.triangles, :2])
        if not shapely.coverage_is_valid(shapes) or (
            shapely.coverage_union_all(shapes).symmetric_difference(ribbon.shape).area
            > 1e-7
        ):
            raise ValueError("bridge cap coverage overlaps, cracks or differs")
    shell_ownership(ribbon)
    triangles = ribbon.upper.vertices[ribbon.upper.triangles]
    n = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    grade = float(np.max(np.linalg.norm(n[:, :2], axis=1) / n[:, 2]))
    if np.any(n[:, 2] <= 0) or grade > fixture["recipe"]["maximum_grade"] + 1e-8:
        raise ValueError("actual bridge cap grade/orientation invalid")
    volume = audit_closed(ribbon.body)
    height = ribbon.upper.vertices[:, 2] - ribbon.lower.vertices[:, 2]
    integral = float(np.sum(n[:, 2] / 2 * height[ribbon.upper.triangles].mean(axis=1)))
    if volume <= 0 or abs(volume - integral) > 1e-7:
        raise ValueError("bridge closed volume differs from its entire cap integral")
    p = ribbon.body.vertices[ribbon.body.triangles]
    q = ribbon.body.vertices.astype(np.float32).astype(float)[ribbon.body.triangles]
    a, b = (
        np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]),
        np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0]),
    )
    if np.any(np.einsum("ij,ij->i", a, b) <= 0):
        raise ValueError("bridge Float32 face collapses or inverts")
    return {
        "whole_field_error_m": field_error,
        "maximum_whole_face_grade": grade,
        "volume_m3": volume,
        "volume_error_m3": abs(volume - integral),
        "source_contacts": source_contacts(fixture, table, complete_domain, ref),
        "float32_faces_noncollapsed_noninverted": True,
        "float32": float32_caps(fixture, ribbon, table, ref),
    }
