"""Offline checks of recipe reconstruction and actual triangulated surfaces."""

import numpy as np


def metadata_error(curve):
    points = curve.at(curve.middle[:, 0])
    centers = (points[:, 0] + points[:, -1]) / 2
    directions = (points[:, -1] - points[:, 0]) / (2 * curve.width)
    expected = np.c_[
        -np.sin(curve.middle[:, 4]),
        np.cos(curve.middle[:, 4]),
        -np.tan(curve.middle[:, 9]),
    ]
    return float(np.linalg.norm(centers - curve.middle[:, 1:4], axis=1).max()), float(
        np.linalg.norm(directions - expected, axis=1).max()
    )


def observed_error(curve, stations, vertices):
    vertices = np.asarray(vertices, dtype=float)
    fractions = np.array([0, 0.125, 0.25, 0.5, 0.75, 0.875, 1.0])
    at = np.unique(
        np.r_[
            (stations[:-1, None] + np.diff(stations)[:, None] * fractions).ravel(),
            curve.stations,
        ]
    )
    index = np.minimum(
        np.searchsorted(stations, at, side="right") - 1, len(stations) - 2
    )
    alpha = ((at - stations[index]) / (stations[index + 1] - stations[index]))[:, None]
    truth = curve.at(at)
    worst, witness = 0.0, None
    for band in [1] if curve.shoulder == 0 else range(3):
        a, b = vertices[index, band], vertices[index + 1, band]
        c, d = vertices[index, band + 1], vertices[index + 1, band + 1]
        for beta in (0, 0.25, 0.5, 0.75, 1.0):
            linear = np.where(
                alpha + beta <= 1,
                a + alpha * (b - a) + beta * (c - a),
                d + (1 - alpha) * (c - d) + (1 - beta) * (b - d),
            )
            actual = truth[:, band] * (1 - beta) + truth[:, band + 1] * beta
            error = np.linalg.norm(actual - linear, axis=1)
            i = int(np.argmax(error))
            if error[i] > worst:
                worst = float(error[i])
                witness = {
                    "station_m": float(at[i]),
                    "band": band,
                    "across_fraction": beta,
                    "model_enh_m": actual[i].tolist(),
                    "mesh_enh_m": linear[i].tolist(),
                }
    return worst, witness, len(at)


def native_positions(row):
    positions = np.array(row["upper_positions_m"], dtype=np.float32).reshape(-1, 4, 3)
    return positions[..., [0, 2, 1]] * np.array([1, -1, 1], dtype=np.float32)
