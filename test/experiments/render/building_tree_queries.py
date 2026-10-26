"""Declared diagnostic viewpoints, separate from reusable source/node products."""

import numpy as np


def queries(tree):
    centre = tree["node_bounds"][0].mean(0)
    result = []
    for azimuth, direction in enumerate(([0.45, 0.55, -0.7], [-0.65, 0.3, -0.65])):
        forward = np.array(direction)
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, [0, 0, 1])
        right /= np.linalg.norm(right)
        basis = np.array([right, np.cross(right, forward), forward])
        for distance in (1000, 16000, 64000):
            result.append(
                {
                    "name": f"az{azimuth}-{distance}",
                    "eye": (centre - forward * distance).tolist(),
                    "basis": basis.tolist(),
                    "focal_px": 900,
                    "resolution": [1280, 720],
                    "distance_m": distance,
                }
            )
    return result


def view(query):
    return (
        np.array(query["eye"]),
        np.array(query["basis"]),
        query["focal_px"],
        *query["resolution"],
    )
