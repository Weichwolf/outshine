"""Compare conservative and transformed rock footprints against pixel integration."""

import argparse
import hashlib
import json
import resource
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from ground_gradients import noise_gradient
from rock_direction import BEDDING, FRACTURES, value

ROOT = Path(__file__).resolve().parents[3]
WIDTH = 0.07
RGB_CONVERGENCE = 1e-5
GRADIENT_CONVERGENCE = 1e-4


def recipe():
    materials = json.loads(
        (ROOT / "src/assets/world/ground-materials.json").read_text()
    )
    templates = json.loads((ROOT / "src/assets/world/vegetation.json").read_text())
    template = next(x for x in templates["templates"] if x["name"] == "rock_crevice")
    material = next(
        x for x in materials["classes"] if x["name"] == template["ground"]["class"]
    )
    return material, template["ground"]["contrast"]


def fade(across):
    t = np.clip((across - 0.25) / 0.55, 0, 1)
    return 1 - t * t * (3 - 2 * t)


def field(world, dx, dy, material, contrast, mode):
    """Existing point definition; the candidate changes footprint mapping only."""
    coarse, fine = material["detailScaleM"]
    scales = [coarse * 12, coarse, fine]
    amplitudes = [
        min(scales[0] * 0.025, 3),
        min(coarse * 0.04, 0.4),
        material["heightAmplitudeM"],
    ]
    matrices = [np.eye(3), BEDDING, FRACTURES]
    world_size = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    values, gradients, hashes = [], [], 0
    for index, (matrix, scale, amplitude) in enumerate(
        zip(matrices, scales, amplitudes, strict=True)
    ):
        if mode == "point":
            across = np.zeros(len(world))
        elif mode == "legacy":
            across = world_size * (1 if index == 0 else 1.04) / scale
        else:
            across = (
                np.maximum(
                    np.linalg.norm(dx @ matrix.T, axis=-1),
                    np.linalg.norm(dy @ matrix.T, axis=-1),
                )
                / scale
            )
        visibility = fade(across)
        active = visibility > 0
        value = np.zeros(len(world))
        gradient = np.zeros_like(world)
        if active.any():
            n, g = noise_gradient(world[active] @ matrix.T / scale)
            value[active] = n - 0.5
            gradient[active] = g @ matrix / scale
            hashes += int(active.sum()) * 8
        values.append(value * visibility)
        if index < 2:
            variance = 1.125 * across * across
            if mode in ("linear", "production"):
                phase_dx = np.sum(gradient * dx, axis=-1)
                phase_dy = np.sum(gradient * dy, axis=-1)
                variance = (phase_dx * phase_dx + phase_dy * phase_dy) / 12
            square = WIDTH * WIDTH + variance
            pulse = WIDTH / np.sqrt(square) * np.exp(-value * value / (2 * square))
            gradient *= (amplitude * pulse * value / square * visibility)[:, None]
        else:
            gradient *= (amplitude * visibility)[:, None]
        gradients.append(gradient)
    macro, coarse_value, fine_value = values
    albedo = np.clip(
        1 + contrast * (0.4 * macro + 0.5 * coarse_value + 0.1 * fine_value), 0.72, 1.28
    )
    roughness = 0.14 * macro + 0.06 * coarse_value
    return np.column_stack([albedo, roughness, sum(gradients)]), hashes


def integrate(at, dx, dy, material, contrast, side):
    nodes = (np.arange(side) + 0.5) / side - 0.5
    x, y = np.meshgrid(nodes, nodes)
    points = at + x.ravel()[:, None] * dx + y.ravel()[:, None] * dy
    values, hashes = field(
        points,
        np.zeros_like(points),
        np.zeros_like(points),
        material,
        contrast,
        "point",
    )
    return values.mean(axis=0), hashes


def reference(at, dx, dy, material, contrast):
    previous, work = integrate(at, dx, dy, material, contrast, 32)
    colour = np.max(material["albedo"])
    for side in (64, 128, 256, 512):
        current, hashes = integrate(at, dx, dy, material, contrast, side)
        work += hashes
        rgb_change = abs(current[0] - previous[0]) * colour
        gradient_change = np.linalg.norm(current[2:] - previous[2:])
        if rgb_change <= RGB_CONVERGENCE and gradient_change <= GRADIENT_CONVERGENCE:
            return current, {
                "side": side,
                "rgb_change": float(rgb_change),
                "gradient_change": float(gradient_change),
                "point_hashes": work,
            }
        previous = current
    raise AssertionError(
        ("pixel reference did not converge", at, dx, dy, rgb_change, gradient_change)
    )


def normal(gradient, dx, dy):
    n = np.cross(dx, dy)
    n /= np.linalg.norm(n)
    tangent = gradient - np.sum(gradient * n, axis=-1, keepdims=True) * n
    result = n - tangent
    return result / np.linalg.norm(result, axis=-1, keepdims=True)


def cases():
    axes = {
        "floor_z": ([0, 0, 1], [1, 0, 0]),
        "cliff_y": ([0, 1, 0], [0, 0, 1]),
        "shear": ([0, 0.12, 1], [1, 0.25, 0.35]),
    }
    origins = np.random.default_rng(370).uniform(-120, 120, (4, 3))
    for orientation, (u, v) in axes.items():
        for width in (0.02, 0.3, 2, 8):
            dx = np.array(u) * width
            dy = np.array(v) * 0.03
            for index, at in enumerate(origins):
                yield f"{orientation}-{width}-p{index}", at, dx, dy
    material, _ = recipe()
    coarse = material["detailScaleM"][0]
    cells = np.random.default_rng(371).integers(-5, 6, (256, 3))
    cells = cells[np.argsort(abs(value(cells) - 0.5))[:2]]
    for label, transform in (
        ("macro", np.eye(3) / (coarse * 12)),
        ("bedding", BEDDING / coarse),
    ):
        inverse = np.linalg.inv(transform)
        dx = inverse @ np.array([0.4, 0, 0])
        dy = inverse @ np.array([0, 0.05, 0])
        for index, cell in enumerate(cells):
            for offset in np.linspace(-0.15, 0.15, 9):
                at = inverse @ (cell + np.array([offset, 0, 0]))
                yield f"critical-{label}-p{index}-{offset:.4f}", at, dx, dy
        yield f"swapped-{label}", at, dy, dx


def controls(material, contrast):
    rng = np.random.default_rng(13)
    at, dx, dy = rng.normal(size=(3, 32, 3))
    point, _ = field(
        at, np.zeros_like(at), np.zeros_like(at), material, contrast, "point"
    )
    for mode in ("legacy", "mapped", "linear"):
        tiny, _ = field(at, dx * 1e-7, dy * 1e-7, material, contrast, mode)
        np.testing.assert_allclose(tiny, point, rtol=0, atol=1e-8)
        distant, work = field(at, dx * 1e6, dy * 1e6, material, contrast, mode)
        np.testing.assert_array_equal(distant, np.tile([1, 0, 0, 0, 0], (len(at), 1)))
        assert work == 0
    for matrix in (BEDDING, FRACTURES):
        mapped = np.maximum(
            np.linalg.norm(dx @ matrix.T, axis=-1),
            np.linalg.norm(dy @ matrix.T, axis=-1),
        )
        bound = 1.04 * np.maximum(
            np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1)
        )
        assert np.all(mapped <= bound)
    return 8


def chart(output, rows):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(12, 5), constrained_layout=True)
    for axis, key, title in zip(
        axes,
        ("rgb_error", "normal_error_deg"),
        ("Nominal RGB error", "Mean-gradient normal error (degrees)"),
        strict=True,
    ):
        axis.plot([r["legacy"][key] for r in rows], label="World footprint ×1.04")
        axis.plot([r["mapped"][key] for r in rows], label="Transformed bound")
        axis.plot([r["linear"][key] for r in rows], label="Local phase Gaussian")
        axis.set_title(title)
        axis.set_xlabel("Same metric pixel cases")
        axis.legend()
    figure.suptitle(
        "Existing rock recipe / numerical point-material pixel integral\nCPU component comparison; not BRDF, native or Place proof"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) and not output.is_relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    material, contrast = recipe()
    checked = controls(material, contrast)
    started = time.perf_counter()
    rows = []
    for name, at, dx, dy in cases():
        expected, convergence = reference(at, dx, dy, material, contrast)
        row = {
            "case": name,
            "at": at.tolist(),
            "dx": dx.tolist(),
            "dy": dy.tolist(),
            "reference": convergence,
        }
        for mode in ("legacy", "mapped", "linear"):
            actual, hashes = field(
                at[None], dx[None], dy[None], material, contrast, mode
            )
            delta = actual[0] - expected
            a, b = (
                normal(actual[:, 2:], dx, dy)[0],
                normal(expected[2:][None], dx, dy)[0],
            )
            row[mode] = {
                "rgb_error": float(abs(delta[0]) * np.max(material["albedo"])),
                "roughness_error": float(abs(delta[1])),
                "normal_error_deg": float(np.degrees(np.arccos(np.clip(a @ b, -1, 1)))),
                "corner_hashes": hashes,
            }
        rows.append(row)
        print(name, row["legacy"]["rgb_error"], row["mapped"]["rgb_error"], flush=True)
    report = {
        "controls": checked,
        "material": material["name"],
        "cases": rows,
        "seconds": time.perf_counter() - started,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "scope": "CPU point-material integration. Normal metric resolves the mean gradient; not integrated BRDF or normal-variance proof.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    chart(output, rows)


if __name__ == "__main__":
    main()
