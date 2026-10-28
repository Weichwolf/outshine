"""Bounded terrain collars around one immutable qualified road surface."""

from dataclasses import dataclass

import numpy as np
import shapely
from ground_refinement import refine_ground
from ground_surface import ground_layout
from outshine_ref.contracts import Mesh
from outshine_ref.transport.heights import _face_gradients


@dataclass(frozen=True)
class ContactLimits:
    margins_m: tuple = (5.0, 10.0, 15.0, 20.0)
    soil_grade: float = 2 / 3
    maximum_road_vertices: int = 512
    maximum_road_faces: int = 1024
    maximum_faces: int = 4096
    maximum_new_vertices: int = 128
    maximum_passes: int = 4
    maximum_quality_queries: int = 65536

    def __post_init__(self):
        if (
            not 1 <= len(self.margins_m) <= 8
            or not np.isfinite(self.margins_m).all()
            or not 0 < min(self.margins_m) <= max(self.margins_m) <= 64
            or any(a >= b for a, b in zip(self.margins_m, self.margins_m[1:]))
            or not np.isfinite(self.soil_grade)
            or not 0 < self.soil_grade <= 1
        ):
            raise ValueError(
                "Bounded ordered contact margins and positive soil grade required"
            )
        for cap in (
            self.maximum_road_vertices,
            self.maximum_road_faces,
            self.maximum_faces,
            self.maximum_passes,
            self.maximum_quality_queries,
        ):
            if type(cap) is not int or not 1 <= cap <= 1048576:
                raise ValueError("Positive bounded contact work capacities required")
        if (
            type(self.maximum_new_vertices) is not int
            or not 0 <= self.maximum_new_vertices <= 4096
        ):
            raise ValueError("Bounded terrain detail capacity required")


def maximum_grade(vertices, faces):
    gradients = _face_gradients(vertices[:, :2], faces, vertices[:, 2])
    return float(np.linalg.norm(gradients, axis=1).max(initial=0))


def corner_witness(road, corners, heights, grade):
    fixed = np.vstack((road.vertices, np.c_[corners, heights]))
    distance = np.linalg.norm(corners[:, None] - fixed[None, :, :2], axis=2)
    change = abs(heights[:, None] - fixed[None, :, 2])
    residual = change - grade * distance
    residual[(distance == 0) & (change == 0)] = -np.inf
    a, b = np.unravel_index(np.argmax(residual), residual.shape)
    return {
        "pairs": int(distance.size),
        "height_change_m": float(change[a, b]),
        "distance_m": float(distance[a, b]),
        "excess_m": float(residual[a, b]),
        "corner": int(a),
        "fixed_vertex": int(b),
    }


def candidate(
    road,
    road_dem_z,
    sample_z,
    margin,
    limits,
    method,
    road_grade,
    *,
    profile_guard=None,
):
    domain = shapely.box(
        *(road.vertices[:, :2].min(axis=0) - margin),
        *(road.vertices[:, :2].max(axis=0) + margin),
    )
    corners = shapely.get_coordinates(domain)[:-1]
    heights = np.asarray(sample_z(corners), dtype=float)
    if heights.shape != (4,) or not np.isfinite(heights).all():
        raise ValueError("missing_or_invalid_outer_dem")
    effective_grade = limits.soil_grade * 1.0000001
    witness = corner_witness(road, corners, heights, effective_grade)
    record = {"margin_m": margin, "corner_height_witness": witness}
    # On this convex patch, a uniform gradient bound implies a whole-segment
    # Lipschitz bound. Use it only when the immutable road meets the same bound.
    if (
        profile_guard is None
        and road_grade <= effective_grade
        and witness["excess_m"] > 0
    ):
        return None, record | {
            "status": "pending",
            "reason": "fixed_corner_lipschitz_conflict",
        }
    if profile_guard is not None:
        proof = profile_guard(corners, heights)
        record["whole_profile_preflight"] = proof
        if proof["state"] == "contradiction":
            return None, record | {
                "status": "pending",
                "reason": "fixed_whole_profile_conflict",
            }
    layout, work = ground_layout(
        road.vertices[:, :2],
        road.triangles,
        domain,
        method=method,
        max_faces=limits.maximum_faces,
    )
    mapping = layout["road_vertex_map"]
    xyz = np.empty((len(layout["xy"]), 3))
    xyz[:, :2] = layout["xy"]
    xyz[mapping] = road.vertices
    prior = np.empty(len(xyz))
    prior[mapping] = road_dem_z
    outer = np.setdiff1d(np.arange(len(xyz)), mapping)
    lookup = {tuple(p): float(z) for p, z in zip(corners, heights, strict=True)}
    if len(outer) != 4 or any(tuple(xyz[i, :2]) not in lookup for i in outer):
        raise ValueError("unexpected_unbound_terrain_vertex")
    for at in outer:
        xyz[at, 2] = prior[at] = lookup[tuple(xyz[at, :2])]
    soil_grade = maximum_grade(xyz, layout["faces"][len(road.triangles) :])
    record.update(work=work, maximum_soil_grade=soil_grade)
    if soil_grade > effective_grade:
        return None, record | {
            "status": "pending",
            "reason": "fixed_cell_grade_conflict",
        }
    return {
        "mesh": Mesh(xyz, layout["faces"]),
        "road_vertex_map": mapping,
        "domain": domain,
        "outer_vertex_ids": outer,
        "prior_z": prior,
    }, record | {"status": "candidate", "reason": ""}


def build_contact(
    road, road_dem_z, sample_z, *, limits=None, method="coverage", whole_profiles=False
):
    limits = ContactLimits() if limits is None else limits
    if not isinstance(road, Mesh) or not len(road.triangles):
        raise ValueError("One nonempty immutable road mesh required")
    if method not in ("coverage", "general"):
        raise ValueError("Known exact road-domain union procedure required")
    road_dem_z = np.asarray(road_dem_z, dtype=float)
    if road_dem_z.shape != (len(road.vertices),) or not np.isfinite(road_dem_z).all():
        raise ValueError(
            "One finite bound DEM observation per immutable road vertex required"
        )
    if (
        len(road.vertices) > limits.maximum_road_vertices
        or len(road.triangles) > limits.maximum_road_faces
        or len(road.triangles) > limits.maximum_faces
    ):
        return {
            "status": "pending",
            "reason": "road_contact_input_budget",
            "candidates": [],
        }
    road_grade = maximum_grade(road.vertices, road.triangles)
    profile_guard = None
    if whole_profiles:
        from ground_contact_profiles import WholeProfileGuard

        profile_guard = WholeProfileGuard(road, limits.soil_grade * 1.0000001)
    candidates, chosen = [], None
    for margin in limits.margins_m:
        try:
            value, record = candidate(
                road,
                road_dem_z,
                sample_z,
                margin,
                limits,
                method,
                road_grade,
                profile_guard=profile_guard,
            )
        except ValueError as error:
            value, record = (
                None,
                {"margin_m": margin, "status": "pending", "reason": str(error)},
            )
        candidates.append(record)
        if value is not None:
            chosen = value
            break
    if chosen is None:
        result = {
            "status": "pending",
            "reason": "no_qualified_terrain_contact_variant",
            "candidates": candidates,
        }
        if profile_guard is not None:
            result["profile_proof_work"] = profile_guard.work
        return result
    coarse = chosen["mesh"]
    mesh, roots, refinement = refine_ground(
        coarse,
        sample_z,
        protected_faces=len(road.triangles),
        grade=limits.soil_grade,
        method="midpoint",
        max_new_vertices=limits.maximum_new_vertices,
        max_faces=limits.maximum_faces,
        max_passes=limits.maximum_passes,
        max_quality_queries=limits.maximum_quality_queries,
    )
    mapping = chosen["road_vertex_map"]
    if not np.array_equal(mesh.vertices[mapping], road.vertices) or not np.array_equal(
        mesh.triangles[: len(road.triangles)], mapping[road.triangles]
    ):
        raise ValueError("terrain_contact_changed_immutable_road")
    result = chosen | {
        "status": "candidate",
        "reason": "",
        "mesh": mesh,
        "coarse_mesh": coarse,
        "refinement_roots": roots,
        "candidates": candidates,
        "refinement": refinement,
        "limits": limits,
        "road_physical_grade": road_grade,
        "scope": "Fixed finite road and chosen outer DEM chord profiles; bounded local soil construction/refinement only, no original DEM-grid/World/Source/vehicle/LOD/device admission.",
    }
    if profile_guard is not None:
        result["profile_proof_work"] = profile_guard.work
    return result
