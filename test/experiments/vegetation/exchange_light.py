"""Current beam visibility/material response remains outside the geometric operator."""

import numpy as np
from outshine_ref.vegetation.leaf_optics import bounded_optics


def basis_optics(basis, state):
    values = bounded_optics(
        np.array([state["leaf_rt"], [*state["ground_albedo"], 0, 0, 0]], float)
    )
    return values[(np.arange(len(basis["points"])) >= basis["leaf_nodes"]).astype(int)]


def direct_nodes(scene, basis, states, bias=2e-6, packet=8192):
    count = len(basis["points"])
    optics = {name: basis_optics(basis, state) for name, state in states.items()}
    output = {name: np.zeros((count * 2, 3)) for name in states}
    for first in range(0, count, packet):
        last = min(first + packet, count)
        points, normals = basis["points"][first:last], basis["normals"][first:last]
        incident = {}
        for name, state in states.items():
            optical = optics[name][first:last]
            result = np.zeros((len(points), 2, 3))
            for lamp in state["lights"]:
                direction, energy = (
                    np.asarray(lamp["direction"], float),
                    np.asarray(lamp["irradiance_rgb"], float),
                )
                if (
                    energy.shape != (3,)
                    or not np.isfinite(energy).all()
                    or np.any(energy < 0)
                ):
                    raise ValueError("finite nonnegative beam RGB required")
                key = tuple(direction)
                if key not in incident:
                    visible, work = scene.visibility(points, normals, direction, bias)
                    for k, v in work.items():
                        scene.shadow_work[k] = scene.shadow_work.get(k, 0) + v
                    cosine = normals @ direction
                    incident[key] = (
                        np.maximum(cosine, 0) * visible / np.pi,
                        np.maximum(-cosine, 0) * visible / np.pi,
                    )
                plus, minus = incident[key]
                r, t = optical[:, :3], optical[:, 3:]
                result[:, 0] += (r * plus[:, None] + t * minus[:, None]) * energy
                result[:, 1] += (t * plus[:, None] + r * minus[:, None]) * energy
            output[name][first * 2 : last * 2] = result.reshape(-1, 3)
    return output
