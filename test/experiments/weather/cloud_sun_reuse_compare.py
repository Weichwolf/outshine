"""Compare solar-map interpolation and retain low-sun source outliers (#353)."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_field import Layer, optical_depth
from cloud_sun_reuse import SolarLevel, column_points, sun_frame

ROOT = Path(__file__).resolve().parents[3]


def receivers():
    generator = np.random.default_rng(320)
    xz = generator.uniform(-19800, 19800, (8192, 2))
    height = generator.uniform(20, 2200, len(xz))
    radius = Layer().radius_m
    horizontal2 = np.sum(xz * xz, axis=1)
    root = np.sqrt((radius + height) ** 2 - horizontal2)
    y = (height * (2 * radius + height) - horizontal2) / (root + radius)
    return np.c_[xz[:, 0], y, xz[:, 1]]


def uniform_depth(layer, sun, u, v, bounds, spacing):
    low, high = bounds
    step = np.array([spacing * sun[1], spacing])
    dimensions = np.ceil((high - low) / step).astype(int) + 1
    axis = [low[k] + np.arange(dimensions[k]) * step[k] for k in (0, 1)]
    uv = np.stack(np.meshgrid(*axis, indexing="xy"), axis=-1).reshape(-1, 2)
    depth = np.empty(len(uv))
    for first in range(0, len(uv), 32768):
        points = column_points(uv[first : first + 32768], sun, u, v, layer)
        depth[first : first + len(points)] = optical_depth(
            points, sun, layer, 6, 37, maximum_m=1e8
        )
    return depth.reshape(dimensions[1], dimensions[0]), step


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    args.output.mkdir(parents=True, exist_ok=True)
    points = receivers()
    layer = Layer(cover=0.65)
    values = {"receivers": points}
    reports, refinement = [], []
    for cosine in (0.8, 0.2, 0.02):
        sun, u, v = sun_frame([-np.sqrt(1 - cosine * cosine), cosine, 0])
        uv = np.c_[points @ u, points @ v]
        bounds = (uv.min(axis=0) - 500, uv.max(axis=0) + 500)
        columns = column_points(uv, sun, u, v, layer)
        source = {
            count: np.exp(-optical_depth(points, sun, layer, count, 37, maximum_m=1e8))
            for count in (6, 64, 128, 256)
        }
        canonical = np.exp(-optical_depth(columns, sun, layer, 6, 37, maximum_m=1e8))
        invariance = float(np.max(abs(canonical - source[6])))
        if invariance > 1e-9:
            raise RuntimeError("full-column invariance was violated")
        worst = int(np.argmax(abs(source[6] - source[256])))
        refinement.append(
            {
                "sun_cosine": cosine,
                "invariance_max_abs": invariance,
                "T6_T256_max_abs": float(abs(source[6] - source[256]).max()),
                "T64_T128_max_abs": float(abs(source[64] - source[128]).max()),
                "T128_T256_max_abs": float(abs(source[128] - source[256]).max()),
                "worst_point_m": points[worst].tolist(),
                "worst_values": {str(n): float(source[n][worst]) for n in source},
            }
        )
        values.update({f"{cosine}/{count}": value for count, value in source.items()})
        for spacing in (400, 200, 100, 50):
            depth, step = uniform_depth(layer, sun, u, v, bounds, spacing)
            for kind in ("tau", "T"):
                for dtype in ("f4", "f2"):
                    stored = (depth if kind == "tau" else np.exp(-depth)).astype(dtype)
                    level = SolarLevel(bounds[0], step, stored)
                    interpolated, _ = level.sample(uv)
                    actual = np.exp(-interpolated) if kind == "tau" else interpolated
                    error = abs(actual - source[6])
                    reports.append(
                        {
                            "sun_cosine": cosine,
                            "spacing_m": spacing,
                            "kind": kind,
                            "dtype": dtype,
                            "bytes": stored.nbytes,
                            "map_density_build_upper": 6 * stored.size,
                            "T_abs_max": float(error.max()),
                            "T_abs_p99": float(np.quantile(error, 0.99)),
                        }
                    )
        print(refinement[-1], flush=True)
    np.savez(args.output / "inputs-and-source-values.npz", **values)
    (args.output / "report.json").write_text(
        json.dumps(
            {
                "interpolation": reports,
                "source_refinement": refinement,
                "scope": "Finite cloud-only source cohort; no general physical convergence or runtime profile.",
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
