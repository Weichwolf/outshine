"""Whole continuous actor tube through the actual prismatic entrance assembly."""

import math
from itertools import pairwise

import manifold3d as manifold
import numpy as np
import shapely
from structural_shell_geometry import native


def collision(parts):
    solids = [native({k: p[k] for k in ("vertices", "faces")}) for p in parts]
    result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError:
        raise ValueError("Entrance assembly is not a valid solid")
    return result


def section(solid, z):
    # Millimetre units retain the existing measured native section precision.
    polygons = [
        shapely.Polygon(np.asarray(ring) / 1000.0)
        for ring in solid.slice(z * 1000.0).to_polygons()
    ]
    result = shapely.Polygon()
    for p in sorted(polygons, key=lambda p: -p.area):
        result = result.symmetric_difference(p)
    return result


def clearance(entry, parts, *, floor_z_m=0.16, outside_m=0.8, inside_m=1.5):
    solid = collision(parts)
    mesh = solid.to_mesh64()
    vertices = np.asarray(mesh.vert_properties)[:, :3]
    triangles = vertices[np.asarray(mesh.tri_verts)]
    n = np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True)
    if not np.all((abs(n[:, 2]) < 1e-8) | (abs(abs(n[:, 2]) - 1) < 1e-8)):
        raise ValueError("Arrival-tube proof requires actual vertical prisms")
    centre, tangent = (
        np.asarray(entry["door"]["centre"]),
        np.asarray(entry["door"]["tangent"]),
    )
    normal = np.array([tangent[1], -tangent[0]])
    path = shapely.LineString([centre + normal * outside_m, centre - normal * inside_m])
    radius_factor = 1 / math.cos(math.pi / (4 * 32))
    torso = path.buffer(0.30 * radius_factor, quad_segs=32)
    feet = path.buffer(0.10 * radius_factor, quad_segs=32)
    scaled = solid.scale((1000.0,) * 3)
    floor = section(scaled, floor_z_m - 1e-7)
    if feet.difference(floor).area > 1e-8:
        raise ValueError("Actual arrival has missing foot support")
    tested = 0
    for lo, hi in pairwise(np.unique(vertices[:, 2])):
        low, high = max(float(lo), floor_z_m + 0.40), min(float(hi), floor_z_m + 1.90)
        if low >= high:
            continue
        if torso.intersection(section(scaled, (low + high) / 2)).area > 1e-8:
            raise ValueError("Actual arrival obstructs the continuous torso tube")
        tested += 1
    return {
        "path_length_m": path.length,
        "actor_radius_m": 0.30,
        "torso_top_m": 1.90,
        "critical_prism_slabs": tested,
        "foot_radius_m": 0.10,
        "scope": "Continuous straight arrival only on the actual prismatic mesh; conservative round tube, no upper routes/external terrain/native controller admission",
    }
