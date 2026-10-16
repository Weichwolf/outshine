"""Bounded shared graminoid stand and its explicit neighbouring-root support."""

import hashlib
import json

import numpy as np
from grass_projection import guard_bounds
from ground_cover_scene import tuft_poses
from outshine_ref.vegetation import GroundCoverPlan
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh

LOWER = np.array((-0.5, -0.5))
UPPER = np.array((0.5, 0.5))
DIRECTIONS = {
    "top": np.array((0.0, 0.0, 1.0)),
    "east": np.array((np.sqrt(3) / 2, 0.0, 0.5)),
    "west": np.array((-np.sqrt(3) / 2, 0.0, 0.5)),
}


def build_stand(density=150):
    plan = GroundCoverPlan(113, density)
    forms = [graminoid_tuft_mesh(17 + i, segments=3) for i in range(8)]
    radius = (
        max(np.linalg.norm(m["vertices"][:, :2], axis=1).max() for m in forms) * 1.15
    )
    height = max(m["vertices"][:, 2].max() for m in forms) * 1.15
    lower, upper = np.array((-2.0, -2.0)), np.array((2.0, 2.0))
    for d in DIRECTIONS.values():
        needed_lower, needed_upper = guard_bounds(LOWER, UPPER, d, radius, height)
        if np.any(needed_lower < lower) or np.any(needed_upper > upper):
            raise ValueError(
                "saved canonical root support does not cover complete rays"
            )
    sites = plan.sites(lower, upper, max_sites=4096)
    if sites is None:
        raise ValueError("reference stand exceeds4096 root limit before meshing")
    poses = tuft_poses(sites)
    vertices, faces, face_root, offset = [], [], [], 0
    for i, form in enumerate(forms):
        indices = np.flatnonzero(poses["variants"] == i)
        v = (
            np.einsum("nij,vj->nvi", poses["frames"][indices], form["vertices"])
            * poses["scales"][indices, None, None]
        )
        v += poses["origins"][indices, None]
        f = form["faces"][None] + np.arange(len(indices))[:, None, None] * len(
            form["vertices"]
        )
        vertices.append(v.reshape(-1, 3))
        faces.append(f.reshape(-1, 3) + offset)
        face_root.append(np.repeat(indices, len(form["faces"])))
        offset += v.size // 3
    recipe = {
        "format": "grass-contact-stand-1",
        "root_plan": plan.recipe(),
        "form_seeds": list(range(17, 25)),
        "segments": 3,
        "root_bounds_m": [lower.tolist(), upper.tolist()],
        "maximum_radius_m": float(radius),
        "maximum_height_m": float(height),
        "scope": "Authored unidentified graminoid; no botanical or lighting admission",
    }
    identity = hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()
    return (
        recipe,
        identity,
        {
            "vertices": np.concatenate(vertices),
            "faces": np.concatenate(faces),
            "face_root": np.concatenate(face_root),
            **{f"site_{k}": v for k, v in sites.items()},
        },
    )


def require_support(recipe, lower, upper, direction):
    needed = guard_bounds(
        lower, upper, direction, recipe["maximum_radius_m"], recipe["maximum_height_m"]
    )
    saved = np.asarray(recipe["root_bounds_m"])
    if np.any(needed[0] < saved[0]) or np.any(needed[1] > saved[1]):
        raise ValueError("query requires missing neighbouring-root support")
