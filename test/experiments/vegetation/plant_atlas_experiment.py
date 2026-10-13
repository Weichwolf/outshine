"""Check and measure whole-plant aggregation before drawing a cached forest."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from plant_atlas import bake_chart, bake_product, load_source
from textured_raster import chart_fragments, composite


def compositor_checks():
    random = np.random.default_rng(9)
    pixel = random.integers(0, 30, 3000)
    depth = random.random(len(pixel))
    alpha = random.choice((0, 0.1, 0.5, 0.9, 1), len(pixel))
    samples = random.random((len(pixel), 8)) * alpha[:, None]
    samples[:, 3] = alpha
    expected = np.zeros((36, 8))
    for target in range(36):
        transmission = 1
        for index in np.flatnonzero(pixel == target)[
            np.argsort(-depth[pixel == target])
        ]:
            expected[target] += transmission * samples[index]
            transmission *= 1 - samples[index, 3]
    np.testing.assert_allclose(
        composite(pixel, depth, samples, 6).reshape(36, 8), expected, atol=1e-10
    )
    np.testing.assert_array_equal(
        composite(np.empty(0, int), np.empty(0), np.empty((0, 8)), 2),
        np.zeros((2, 2, 8)),
    )
    vertices = np.array(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)), float)
    mesh = {
        "vertices": np.concatenate((vertices, vertices + (0, 0, 1))),
        "faces": np.array(((0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7))),
        "normals": np.tile((0, 0, 1), (12, 1)),
        "uv": np.array(
            ((0, 0), (0.5, 0), (0.5, 1), (0, 1), (0.5, 0), (1, 0), (1, 1), (0.5, 1))
        ),
    }
    colour = np.zeros((4, 8, 4))
    colour[:, :4] = (0, 0, 0.5, 0.5)
    colour[:, 4:] = (0.5, 0, 0, 0.5)
    normal = np.broadcast_to((0.25, 0.25, 0.5, 0.5), colour.shape).copy()
    pieces = chart_fragments(mesh, colour, normal, np.eye(3), (-1, -1), (1, 1), 4)
    assert len(pieces[0]) == 32
    actual = composite(*pieces, 4)
    np.testing.assert_allclose(
        actual,
        np.broadcast_to((0.5, 0, 0.25, 0.75, 0, 0, 0.75, 0.75), actual.shape),
        atol=1e-12,
    )
    return {
        "independent_depth_composition": True,
        "empty_projection": True,
        "quad_diagonal_not_double_counted": True,
        "transparent_layers_retain_rear_colour": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source", type=Path, default=Path("build/experiments/vegetation/selection")
    )
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/vegetation/plant-atlas")
    )
    parser.add_argument("--probe", action="store_true")
    parser.add_argument("--grid", type=int, default=8, choices=(1, 4, 8, 16))
    args = parser.parse_args()
    checks = compositor_checks()
    began = time.perf_counter()
    source = load_source(args.source)
    load_ms = (time.perf_counter() - began) * 1000
    if args.probe:
        began = time.perf_counter()
        chart, leaf_weight = bake_chart(source, (0.3, -0.9, 0.3), 128)
        assert np.all(np.isfinite(chart.colour)) and np.all(np.isfinite(chart.normal))
        assert np.all(leaf_weight <= chart.colour[..., 3] + 1e-10)
        report = {
            "projection_ms": (time.perf_counter() - began) * 1000,
            "alpha_sum": float(chart.colour[..., 3].sum()),
        }
    else:
        report = bake_product(source, args.output, grid=args.grid)
    result = report | {
        "checks": checks,
        "source_load_ms": load_ms,
        "peak_rss_mib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", str(Path(__file__).parent)],
                text=True,
            ).strip()
        ),
    }
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / ("probe.json" if args.probe else "plant-atlas.json")).write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
