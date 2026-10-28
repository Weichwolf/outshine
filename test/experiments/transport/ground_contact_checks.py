"""Whole actual shared-edge/outer-profile and fixed-road terrain qualification."""

from collections import defaultdict

import numpy as np
import shapely


def edge_owners(faces):
    result = defaultdict(list)
    for owner, face in enumerate(faces):
        for a, b in zip(face, np.roll(face, -1), strict=True):
            result[tuple(sorted((int(a), int(b))))].append(owner)
    return result


def qualify_contact(road, result):
    mesh, mapping = result["mesh"], result["road_vertex_map"]
    count = len(road.triangles)
    if not np.array_equal(mesh.vertices[mapping], road.vertices) or not np.array_equal(
        mesh.triangles[:count], mapping[road.triangles]
    ):
        raise ValueError("contact_road_geometry_changed")
    xyz = mesh.vertices[mesh.triangles]
    normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    if not np.isfinite(xyz).all() or np.any(normal[:, 2] <= 0):
        raise ValueError("contact_mesh_orientation")
    triangles = shapely.polygons(xyz[:, :, :2])
    if not shapely.coverage_is_valid(triangles):
        raise ValueError("contact_mesh_nonconforming")
    domain = shapely.coverage_union_all(triangles)
    if not domain.equals(result["domain"]):
        raise ValueError("contact_mesh_incomplete_domain")
    if len(np.unique(mesh.vertices[:, :2], axis=0)) != len(mesh.vertices):
        raise ValueError("contact_mesh_duplicate_xy_nodes")
    edges = edge_owners(mesh.triangles)
    if (
        any(len(v) not in (1, 2) for v in edges.values())
        or len(mesh.vertices) - len(edges) + len(mesh.triangles) != 1
    ):
        raise ValueError("contact_mesh_nonmanifold_or_euler")
    original = edge_owners(road.triangles)
    contacts = []
    for (a, b), owners in original.items():
        if len(owners) != 1:
            continue
        key = tuple(sorted((int(mapping[a]), int(mapping[b]))))
        neighbours = edges.get(key, ())
        if len(neighbours) != 2 or sum(n < count for n in neighbours) != 1:
            raise ValueError("contact_whole_road_boundary_missing_soil")
        contacts.append(key)
    outer = np.array([e for e, owners in edges.items() if len(owners) == 1])
    lines = shapely.linestrings(mesh.vertices[outer, :2])
    if not np.all(shapely.covered_by(lines, result["domain"].boundary)):
        raise ValueError("contact_outer_boundary_changed")
    retained = result["outer_vertex_ids"]
    if not np.array_equal(
        mesh.vertices[retained], result["coarse_mesh"].vertices[retained]
    ):
        raise ValueError("contact_outer_xyz_changed")
    if not set(np.unique(outer)) <= set(retained):
        raise ValueError("contact_outer_profile_repartitioned")
    if "rim_profile" in result:
        corners = result["rim_profile"]["corner_vertex_ids"]
        x0, y0, x1, y1 = result["domain"].bounds
        expected = {(x0, y0), (x1, y0), (x1, y1), (x0, y1)}
        if result["rim_profile"]["recipe"].get("boundary", "axis") == "oriented":
            rectangle = np.asarray(result["rim_profile"].get("rectangle_corners_xy"))
            if rectangle.shape != (4, 2) or not np.isfinite(rectangle).all():
                raise ValueError("contact_oriented_rectangle_corners_missing")
            expected = set(map(tuple, rectangle))
        if (
            len(corners) != 4
            or set(map(tuple, mesh.vertices[corners, :2])) != expected
            or not np.array_equal(mesh.vertices[corners, 2], result["prior_z"][corners])
        ):
            raise ValueError("contact_original_dem_corner_changed")
    gradient = -normal[:, :2] / normal[:, 2, None]
    soil = np.linalg.norm(gradient[count:], axis=1)
    maximum = float(soil.max(initial=0))
    if maximum > result["limits"].soil_grade * 1.0000001:
        raise ValueError("contact_soil_grade_exceeded")
    prior_gap = abs(result["prior_z"][mapping] - road.vertices[:, 2])
    rim = np.unique(np.array(contacts))
    inverse = {int(v): i for i, v in enumerate(mapping)}
    rim_prior_gap = [
        abs(result["prior_z"][i] - road.vertices[inverse[int(i)], 2]) for i in rim
    ]
    scope = "Exact old road XYZ/faces and shared whole index edges, strict conforming full-domain triangle coverage and unchanged four outer DEM chord profiles; C0 terrain contact, not C1 terrain, original DEM-grid/World/Source/device qualification."
    if "rim_profile" in result:
        scope = "Exact old road XYZ/faces, whole shared index edges, strict conforming complete domain and unchanged fitted whole rim/four exact DEM corners; intermediate rim observations explicitly adjusted under the soil limit. Local C0 only, no native DEM-grid/World/Source/device qualification."
    return {
        "road_vertices_exact": len(road.vertices),
        "road_faces_exact": count,
        "whole_road_soil_edges": len(contacts),
        "outer_edges": len(outer),
        "outer_xyz_exact": len(retained),
        "domain_area_m2": float(domain.area),
        "vertices": len(mesh.vertices),
        "faces": len(mesh.triangles),
        "maximum_soil_grade": maximum,
        "maximum_before_road_dem_gap_m": float(prior_gap.max()),
        "maximum_before_rim_dem_gap_m": float(max(rim_prior_gap)),
        "whole_shared_edge_height_gap_m": 0.0,
        "scope": scope,
    }
