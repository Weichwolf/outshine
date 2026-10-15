"""Expose source-quadrature error, including low-sun counterexamples."""

import argparse
import json
from dataclasses import replace
from pathlib import Path

import numpy as np
from cloud_field import Layer, density, optical_depth
from cloud_radiance import GAUSS3


def error(actual, reference):
    delta = np.abs(actual - reference)
    return {
        "rmse": float(np.sqrt(np.mean(delta * delta))),
        "p99_abs": float(np.quantile(delta, 0.99)),
        "max_abs": float(delta.max()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for degree in range(6):
        assert abs(np.dot(GAUSS3[1], GAUSS3[0] ** degree) - 1.0 / (degree + 1)) < 1e-15
    generator = np.random.default_rng(264)
    layer = replace(Layer(), cover=0.65)
    positions = generator.uniform(
        [-50000, layer.base_m, -50000],
        [50000, layer.base_m + layer.thickness_m, 50000],
        (4096, 3),
    )
    height = positions[:, 1].copy()
    horizontal2 = positions[:, 0] ** 2 + positions[:, 2] ** 2
    root = np.sqrt((layer.radius_m + height) ** 2 - horizontal2)
    positions[:, 1] = (height * (2 * layer.radius_m + height) - horizontal2) / (
        root + layer.radius_m
    )
    positions = positions[density(positions, layer, 37.0) > 0]
    values = {"positions": positions}
    report = []
    for cosine in (0.8, 0.2, 0.02, -0.05):
        sun = np.array([-np.sqrt(1.0 - cosine * cosine), cosine, 0.0])
        rows = {}
        for name, count, quadrature in [
            ("midpoint6", 6, None),
            ("gauss3", 3, GAUSS3),
            ("midpoint64", 64, None),
            ("midpoint128", 128, None),
        ]:
            depth = optical_depth(
                positions, sun, layer, count, 37.0, quadrature=quadrature
            )
            rows[name] = depth
            values[f"sun{cosine}/{name}"] = depth
        reference = rows["midpoint128"]
        result = {
            "sun_cosine": cosine,
            "source_points": len(positions),
            "refinement_depth": error(rows["midpoint64"], reference),
            "methods": {
                name: {
                    "optical_depth": error(rows[name], reference),
                    "cloud_transmittance_only": error(
                        np.exp(-rows[name]), np.exp(-reference)
                    ),
                }
                for name in ("midpoint6", "gauss3")
            },
        }
        report.append(result)
        print(json.dumps(result), flush=True)
    np.savez(args.output / "source-values.npz", **values)
    (args.output / "results.json").write_text(
        json.dumps(
            {
                "cases": report,
                "scope": "Declared procedural density and cloud-only transmittance refinement; planet occlusion is excluded, including negative sun cosines. Low-sun references are not assumed converged. No direct visible-sun, GPU/image/budget claim.",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
