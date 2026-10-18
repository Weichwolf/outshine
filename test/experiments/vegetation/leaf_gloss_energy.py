"""Bounded hemisphere quadrature of the existing single-scatter specular kernel."""

import argparse
import json
import resource
import subprocess
import time
from math import fsum
from pathlib import Path

import numpy as np
from leaf_gloss import LeafGloss, specular_response
from outshine_ref.vegetation.quadrature import quadrature


def hemisphere(order):
    if type(order) is not int or order not in (32, 64, 128, 256):
        raise ValueError("bounded explicit angular quadrature required")
    x, w = quadrature(order)
    mu = (x + 1) / 2
    phi = (np.arange(4 * order) + 0.5) * 2 * np.pi / (4 * order)
    radius = np.sqrt(1 - mu * mu)
    directions = np.stack(
        np.broadcast_arrays(
            radius[:, None] * np.cos(phi), radius[:, None] * np.sin(phi), mu[:, None]
        ),
        axis=-1,
    ).reshape(-1, 3)
    weights = (
        np.broadcast_to((w / 2 * (2 * np.pi / len(phi)))[:, None], (order, len(phi)))
        .ravel()
        .copy()
    )
    return directions, weights


def specular_energy(roughness, view_cosine, directions, weights):
    if not np.isfinite(view_cosine) or not 0.025 <= view_cosine <= 1:
        raise ValueError("declared positive angular view domain required")
    view = np.array([np.sqrt(1 - view_cosine**2), 0, view_cosine])
    normal = np.broadcast_to([0.0, 0, 1], directions.shape)
    response = specular_response(normal, roughness, view, directions)
    return fsum(weights * response)


def direct_energy(optical, model, view_cosine, order=64):
    directions, weights = hemisphere(order)
    directions = np.r_[directions, directions * [1, 1, -1]]
    weights = np.r_[weights, weights]
    view = np.array([np.sqrt(1 - view_cosine**2), 0, view_cosine])
    normal = np.broadcast_to([0.0, 0, 1], directions.shape)
    optical = np.broadcast_to(optical, (len(directions), 6))
    values = model(optical, normal, view, directions)
    return np.array([fsum(weights * values[:, i]) for i in range(3)])


def run(output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    rows = []
    for order in (64, 128, 256):
        directions, weights = hemisphere(order)
        for roughness in (0.2, 0.3, 0.35, 0.7, 1.0):
            for mu in (1.0, 0.9, 0.7, 0.4, 0.2, 0.1, 0.025):
                energy = specular_energy(roughness, mu, directions, weights)
                rows.append(
                    {
                        "order": order,
                        "roughness": roughness,
                        "view_cosine": mu,
                        "specular_energy": energy,
                        "area_white_furnace_p012": 0.88 + 0.12 * energy,
                        "additive_white_furnace_p012": 1 + 0.12 * energy,
                    }
                )
    direct = []
    for optical in ([0.5] * 6, [0.08, 0.22, 0.025, 0.035, 0.12, 0.018]):
        for mode in ("area", "additive"):
            model = LeafGloss(0.35, 0.12, mode)
            actual = direct_energy(optical, model, 0.4)
            directions, weights = hemisphere(64)
            specular = specular_energy(model.roughness, 0.4, directions, weights)
            sheet = np.array(optical[:3]) + optical[3:]
            expected = (0.88 if mode == "area" else 1) * sheet + 0.12 * specular
            np.testing.assert_allclose(actual, expected, rtol=0, atol=2e-14)
            direct.append(
                {
                    "optical": optical,
                    "model": model.recipe(),
                    "energy": actual.tolist(),
                    "formula_max_delta": float(abs(actual - expected).max()),
                }
            )
    usage = resource.getrusage(resource.RUSAGE_SELF)
    (output / "report.json").write_text(
        json.dumps(
            {
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "rows": rows,
                "direct_controls": direct,
                "work": {
                    "specular_samples": sum(4 * row["order"] ** 2 for row in rows),
                    "maximum_hemisphere_points": 4 * 256**2,
                    "largest_direction_weight_bytes": (4 * 256**2) * 4 * 8,
                    "wall_s": time.perf_counter() - began,
                    "process_peak_rss_bytes_linux": usage.ru_maxrss * 1024,
                },
                "scope": "finite uniform-hemisphere quadrature and formal convex-mixture law; no coating, calibrated whole-leaf R/T, GGX multiple-scatter compensation, native/IBL or device parity",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.output)
