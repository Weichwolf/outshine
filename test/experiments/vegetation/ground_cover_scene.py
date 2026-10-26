"""Prepare independent tuft transforms, retaining each original root identity."""

import numpy as np
from outshine_ref.vegetation import GroundCoverPlan
from outshine_ref.vegetation.graminoid import graminoid_tuft_mesh


def tuft_poses(sites):
    marks = sites["marks"]
    angle = 2 * np.pi * marks[:, 0]
    cosine, sine = np.cos(angle), np.sin(angle)
    frames = np.zeros((len(angle), 3, 3))
    frames[:, 0, 0] = frames[:, 1, 1] = cosine
    frames[:, 1, 0], frames[:, 0, 1] = sine, -sine
    frames[:, 2, 2] = 1
    return {
        "ids": sites["ids"],
        "origins": np.c_[sites["xy"], np.zeros(len(angle))],
        "frames": frames,
        "scales": 0.85 + 0.3 * marks[:, 1],
        "variants": (marks[:, 2] * 8).astype(np.uint8),
    }


def tuft_geometry(poses, segments=3):
    """Physical coverage reference only; the actual renderer shares prototypes."""
    vertices, faces, offset = [], [], 0
    for index in range(8):
        selected = poses["variants"] == index
        mesh = graminoid_tuft_mesh(17 + index, segments=segments)
        origins, frames = poses["origins"][selected], poses["frames"][selected]
        scales = poses["scales"][selected]
        v = np.einsum("nij,vj->nvi", frames, mesh["vertices"]) * scales[:, None, None]
        v += origins[:, None]
        f = mesh["faces"][None] + np.arange(len(origins))[:, None, None] * len(
            mesh["vertices"]
        )
        vertices.append(v.reshape(-1, 3))
        faces.append(f.reshape(-1, 3) + offset)
        offset += v.size // 3
    return np.concatenate(vertices), np.concatenate(faces)


def ground_population(name, recipe):
    half = recipe["size_m"] / 2
    plan = GroundCoverPlan(recipe["seed"], recipe["grass_density_m2"])
    sites = plan.sites((-half, -half), (half, half))
    if sites is None:
        raise ValueError("whole grass study exceeds the explicit cell/root budget")
    poses = tuft_poses(sites)
    result = []
    for index in range(8):
        selected = poses["variants"] == index
        if not selected.any():
            continue
        scales = np.repeat(poses["scales"][selected, None], 3, axis=1)
        if name == "Steppe":
            scales *= (0.8, 0.8, 1.65)
        if name == "Heide":
            scales *= (0.7, 0.7, 0.6)
        result.append(
            (
                f"grass-{index}",
                poses["origins"][selected],
                poses["frames"][selected],
                scales,
            )
        )
    return result
