"""Independent whole-body, source-coverage, profile and lower-world admission."""

import numpy as np
import shapely
from elevated_rail_joints import inspect as inspect_joints
from elevated_rail_plane import slope_rows
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from shared_bridge_geometry import corridor
from stair_network_geometry import vertex_links
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed


def inspect(body, top_faces, rows, groups, thickness, along, cross):
    n = len(body.vertices) // 2
    if not 0 < n <= 4096 or len(body.vertices) != 2 * n or len(top_faces) > 8192:
        raise ValueError("Bounded paired deck-body layout required")
    if not np.array_equal(body.triangles[: len(top_faces)], top_faces):
        raise ValueError("Actual deck body lost its upper faces")
    if not np.array_equal(body.vertices[n:], body.vertices[:n] - [0, 0, thickness]):
        raise ValueError("Actual deck bottom differs from declared thickness")
    axes = np.vstack([np.diff(r["xy"], axis=0) for r in rows])
    directions, limits = slope_rows(axes, along, cross)
    original_domains = [
        corridor({k: v for k, v in r.items() if k != "source_grid_m"}) for r in rows
    ]
    proofs = []
    reference = body.vertices[body.triangles]
    reference_normals = np.cross(
        reference[:, 1] - reference[:, 0], reference[:, 2] - reference[:, 0]
    )
    for cast in (False, True):
        vertices = body.vertices.astype("f4").astype(float) if cast else body.vertices
        actual = Mesh(vertices, body.triangles)
        xyz = vertices[body.triangles]
        normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        if np.any(np.einsum("ij,ij->i", reference_normals, normals) <= 0):
            raise ValueError("Actual deck body face collapsed or inverted")
        volume = audit_closed(actual)
        if volume <= 0:
            raise ValueError("Positive closed deck volume required")
        topology = vertex_links(vertices, body.triangles)
        shape = shapely.union_all(shapely.polygons(vertices[top_faces, :2]))
        if not all(shape.covers(domain) for domain in original_domains):
            raise ValueError("Actual deck lost whole original source width")
        joint_proof = inspect_joints(rows, shape)
        gradient = normal_gradients(vertices[:n, :2], top_faces, vertices[:n, 2])
        normalized = abs(gradient @ directions.T) / limits
        if normalized.max() > 1:
            raise ValueError(
                f"Actual deck longitudinal/cross profile exceeded: normalized={float(normalized.max()):.9g}, float32={cast}, face={int(np.argmax(normalized) // len(directions))}"
            )
        underside = Mesh(vertices[n:], top_faces)
        clearances = []
        for name, lower, required in groups:
            low = (
                Mesh(lower.vertices.astype("f4").astype(float), lower.triangles)
                if cast
                else lower
            )
            proof = vertical_clearance(low, underside, seconds=2, include_maximum=True)
            if proof["valid"]:
                if proof["minimum_clearance_m"] < required:
                    raise ValueError("Actual whole deck clearance failed: " + name)
            elif proof["reason"] != "no_positive_area_overlap":
                raise ValueError(
                    "Actual whole deck clearance unresolved: " + str(proof)
                )
            clearances.append({"lower": name, "required_m": required, **proof})
        proofs.append(
            {
                "float32": cast,
                "volume_m3": volume,
                "maximum_normalized_profile": float(normalized.max()),
                "whole_source_widths": len(rows),
                "joint_profiles": joint_proof,
                "clearances": clearances,
                **topology,
            }
        )
    return proofs
