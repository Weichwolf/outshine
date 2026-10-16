"""Independent whole affine field, zero-mouth ownership and positive shell checks."""

from collections import Counter

import numpy as np
import shapely
from bridge_support_contacts import replay_contact
from foot_ramp_geometry import FLARE_M, HEIGHT_M, LENGTH_M, RETURN_M, zones
from retained_surface import surface_domain
from sidewalk_geometry import joint_error
from surface_solid_checks import audit_closed


def affine_field(upper, row):
    origin, basis = np.array(row["origin"]), np.array(row["basis"])
    triangle = np.array(row["triangle"])
    normal = np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0])
    xy = upper.vertices[:, :2]
    ground_z = triangle[0, 2] - (xy - triangle[0, :2]) @ normal[:2] / normal[2]
    local = (xy - origin) @ basis
    p = local[upper.triangles]
    relative = (upper.vertices[:, 2] - ground_z)[upper.triangles]
    coefficients = np.linalg.solve(
        np.dstack([p, np.ones(p.shape[:2])]), relative[:, :, None]
    )[:, :, 0]
    shapes = shapely.polygons(p)
    half, end = row["half"], row["landing_end_m"]
    expected = [
        [0, HEIGHT_M / LENGTH_M, 0],
        [-HEIGHT_M / FLARE_M, 0, -HEIGHT_M * half / FLARE_M],
        [HEIGHT_M / FLARE_M, 0, -HEIGHT_M * half / FLARE_M],
        [0, 0, HEIGHT_M],
        [0, -HEIGHT_M / RETURN_M, HEIGHT_M * (end + RETURN_M) / RETURN_M],
    ]
    error, covered, volume = 0.0, 0.0, 0.0
    for zone, plane in zip(zones(half, end), expected, strict=True):
        intersections = shapely.intersection(shapes, zone)
        for index in np.flatnonzero(shapely.area(intersections) > 1e-15):
            shape = intersections[index]
            points = shapely.get_coordinates(shape)
            delta = coefficients[index] - plane
            error = max(error, float(np.max(abs(points @ delta[:2] + delta[2]))))
            covered += shape.area
            volume += shape.area * (
                np.array(shape.centroid.coords[0]) @ np.array(plane[:2]) + plane[2]
            )
    if error > 1e-7 or abs(covered - surface_domain(upper).area) > 1e-8:
        raise ValueError("ramp whole affine height field differs or incomplete")
    return error, volume


def keys(mesh, reverse=False):
    faces = mesh.vertices[mesh.triangles[:, ::-1] if reverse else mesh.triangles]
    return Counter(
        min(tuple(map(tuple, np.roll(p, k, axis=0))) for k in range(3)) for p in faces
    )


def shell_ownership(ramp):
    remaining = keys(ramp.body)
    for cap in [keys(ramp.upper), keys(ramp.lower, reverse=True)]:
        for face, count in cap.items():
            if remaining[face] != count:
                raise ValueError("ramp actual body/cap ownership differs")
            del remaining[face]
    boundary = surface_domain(ramp.upper).boundary
    for face in remaining:
        points = np.array(face)
        normal = np.cross(points[1] - points[0], points[2] - points[0])
        if abs(normal[2]) > 1e-8 * np.linalg.norm(normal) or any(
            boundary.distance(shapely.Point(p[:2])) > 1e-8 for p in points
        ):
            raise ValueError("ramp has nonboundary or nonvertical extra wall")
    if len(np.unique(ramp.body.vertices, axis=0)) != len(ramp.body.vertices):
        raise ValueError("ramp zero-height mouth vertices are not shared")


def audit(ramp, row, ground, traffic, world, obstacles):
    shape = row["shape"]
    if any(
        surface_domain(cap).symmetric_difference(shape).area > 1e-8
        for cap in [ramp.lower, ramp.upper]
    ):
        raise ValueError("ramp whole actual cap footprint mismatch")
    if shape.difference(world).area > 1e-8 or shape.intersection(obstacles).area > 1e-8:
        raise ValueError("ramp actual footprint violates world/structure exclusion")
    for cap in [ramp.lower, ramp.upper]:
        points = cap.vertices[cap.triangles]
        normal = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
        if np.any(normal[:, 2] <= 0):
            raise ValueError("ramp cap has degenerate or reversed height face")
    contact = replay_contact(ramp.lower, ground)
    field_error, expected_volume = affine_field(ramp.upper, row)
    shell_ownership(ramp)
    volume = audit_closed(ramp.body)
    if volume <= 0 or abs(volume - expected_volume) > 1e-7:
        raise ValueError("ramp actual closed volume differs from whole field integral")
    p = ramp.body.vertices[ramp.body.triangles]
    q = ramp.body.vertices.astype(np.float32).astype(float)[ramp.body.triangles]
    n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    cast = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
    if np.any(np.einsum("ij,ij->i", n, cast) <= 0):
        raise ValueError("ramp Float32 body face degenerates or inverts")
    p = ramp.upper.vertices[ramp.upper.triangles]
    normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    grade = float(np.max(np.linalg.norm(normals[:, :2], axis=1) / normals[:, 2]))
    if grade > 0.12 + 1e-7:
        raise ValueError("ramp actual whole-face grade exceeds recipe")
    origin, basis, half = np.array(row["origin"]), np.array(row["basis"]), row["half"]
    errors = []
    for t in [0, row["landing_end_m"] + RETURN_M]:
        mouth = shapely.LineString(np.array([[-half, t], [half, t]]) @ basis.T + origin)
        if (
            mouth.difference(traffic).length > 1e-7
            or mouth.difference(shape).length > 1e-7
        ):
            raise ValueError(
                "ramp actual zero mouth lacks whole original traffic contact"
            )
        error, _queries = joint_error(ramp.upper, ramp.lower, mouth)
        errors.append(error)
    return {
        "whole_ground_contact_error_m": contact,
        "whole_field_error_m": field_error,
        "volume_m3": volume,
        "volume_error_m3": abs(volume - expected_volume),
        "maximum_grade": grade,
        "maximum_mouth_step_m": max(errors),
    }
