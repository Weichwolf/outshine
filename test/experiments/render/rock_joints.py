"""Compare equal-input rock relief and filtered, warped joint bands."""

import json
from pathlib import Path
from time import perf_counter

import numpy as np
from ground_gradients import noise_gradient
from PIL import Image, ImageDraw
from rock_direction import BEDDING, FRACTURES

ROOT = Path(__file__).resolve().parents[3]
BED_DIRECTION = BEDDING[0]
JOINT_DIRECTION = FRACTURES[0]


def fade(footprint, wavelength):
    t = np.clip((footprint / wavelength - 0.25) / 0.55, 0, 1)
    return 1 - t * t * (3 - 2 * t)


def band(
    world, derivative, footprint, wavelength, direction, warp, warp_gradient, strength
):
    phase = world @ direction / wavelength + strength * warp
    gradient = direction / wavelength + strength * warp_gradient
    bound = (
        np.linalg.norm(direction) / wavelength
        + abs(strength) * 1.5 * np.sqrt(3) / derivative
    )
    variance = (footprint * bound) ** 2 / 6
    height = np.zeros(phase.shape)
    slope = np.zeros_like(phase)
    for harmonic, weight in ((1, 2 / 3), (2, 1 / 3)):
        angle = 2 * np.pi * harmonic * phase
        attenuation = np.exp(-2 * np.pi**2 * harmonic**2 * variance)
        height -= weight * attenuation * np.cos(angle)
        slope += weight * attenuation * 2 * np.pi * harmonic * np.sin(angle)
    return height, slope[..., None] * gradient


def cavity(value, gradient, footprint, wavelength):
    width = 0.07
    variance = (footprint * 1.5 * np.sqrt(3) / wavelength) ** 2 / 6
    square = width * width + variance
    pulse = width / np.sqrt(square) * np.exp(-value * value / (2 * square))
    visible = fade(footprint * 1.04, wavelength)
    return -pulse * visible, (pulse * value / square * visible)[..., None] * gradient


def field(world, footprint, recipe, mode):
    coarse, fine = recipe["detailScaleM"]
    macro_scale = coarse * 12
    macro, macro_gradient = noise_gradient(world / macro_scale)
    macro -= 0.5
    macro_gradient /= macro_scale
    macro_fade = fade(footprint, macro_scale)
    if mode == "bands":
        bed, bed_gradient = band(
            world,
            macro_scale,
            footprint,
            coarse,
            BED_DIRECTION,
            macro,
            macro_gradient,
            1.2,
        )
        joint, joint_gradient = band(
            world,
            macro_scale,
            footprint,
            coarse * 3,
            JOINT_DIRECTION,
            macro,
            macro_gradient,
            1.8,
        )
        grain, grain_gradient = noise_gradient(world @ FRACTURES.T / fine)
        grain = (grain - 0.5) * fade(footprint * 1.04, fine)
        grain_gradient = (
            (grain_gradient @ FRACTURES) / fine * fade(footprint * 1.04, fine)
        )
        amplitudes = (min(coarse * 0.04, 0.4), min(coarse * 0.08, 0.8))
        bump = min(macro_scale * 0.025, 3) * macro * macro_fade
        gradient = min(macro_scale * 0.025, 3) * macro_gradient * macro_fade
        bump += (
            amplitudes[0] * bed
            + amplitudes[1] * joint
            + recipe["heightAmplitudeM"] * grain
        )
        gradient += amplitudes[0] * bed_gradient + amplitudes[1] * joint_gradient
        gradient += recipe["heightAmplitudeM"] * grain_gradient
        colour = 1 + 0.6 * (
            0.4 * macro * macro_fade + 0.25 * bed + 0.25 * joint + 0.1 * grain
        )
    else:
        bump = np.zeros(world.shape[:-1])
        gradient = np.zeros_like(world)
        colour = np.ones(world.shape[:-1])
        scales = (macro_scale, coarse, fine)
        amplitudes = (
            min(macro_scale * 0.025, 3),
            min(coarse * 0.04, 0.4),
            recipe["heightAmplitudeM"],
        )
        for index, (scale, transform, amplitude, weight) in enumerate(
            zip(scales, (np.eye(3), BEDDING, FRACTURES), amplitudes, (0.4, 0.5, 0.1))
        ):
            if index == 0:
                value, slope = macro + 0.5, macro_gradient * scale
            else:
                value, slope = noise_gradient(world @ transform.T / scale)
            visible = fade(footprint * (1 if scale == macro_scale else 1.04), scale)
            raw_gradient = (slope @ transform) / scale
            if mode == "cavities" and index < 2:
                height, height_gradient = cavity(
                    value - 0.5, raw_gradient, footprint, scale
                )
                bump += amplitude * height
                gradient += amplitude * height_gradient
            else:
                bump += amplitude * (value - 0.5) * visible
                gradient += amplitude * raw_gradient * visible
            colour += 0.6 * weight * (value - 0.5) * visible
    return np.clip(colour, 0.72, 1.28), bump, gradient


def picture(colour, gradient, recipe):
    normal = np.stack((-gradient[..., 0], -gradient[..., 1], np.ones(colour.shape)), -1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    light = np.array((0.4, 0.6, 0.6))
    light /= np.linalg.norm(light)
    shade = 0.25 + 0.75 * np.clip(normal @ light, 0, 1)
    linear = np.array(recipe["albedo"]) * colour[..., None] * shade[..., None]
    srgb = np.where(
        linear <= 0.0031308, 12.92 * linear, 1.055 * linear ** (1 / 2.4) - 0.055
    )
    return Image.fromarray(np.uint8(np.clip(srgb, 0, 1) * 255))


def main():
    recipe = next(
        row
        for row in json.loads(
            (ROOT / "src/assets/world/ground-materials.json").read_text()
        )["classes"]
        if row["name"] == "rock"
    )
    x, y = np.meshgrid(np.arange(480) * 0.2, np.arange(360) * 0.2)
    world = np.stack((1000 + x, 200 + y, np.full_like(x, 350)), -1)
    out = ROOT / "build/experiments/render/rock-joints"
    out.mkdir(parents=True, exist_ok=True)
    canvas = Image.new("RGB", (1440, 390), (24, 24, 24))
    draw = ImageDraw.Draw(canvas)
    rows = []
    for column, mode in enumerate(("current", "bands", "cavities")):
        start = perf_counter()
        colour, _, gradient = field(world, 0.2, recipe, mode)
        elapsed = 1000 * (perf_counter() - start)
        canvas.paste(picture(colour, gradient, recipe), (column * 480, 30))
        draw.text((column * 480 + 10, 10), mode, fill="white")
        rows.append(
            {
                "mode": mode,
                "elapsed_ms": elapsed,
                "mean_albedo_scale": float(colour.mean()),
                "gradient_rms": float(np.sqrt(np.mean(gradient * gradient))),
            }
        )
    canvas.save(out / "cliff-comparison.png")
    probes = np.random.default_rng(28).uniform(-1000, 1000, (100, 3))
    for mode in ("current", "bands", "cavities"):
        _, _, gradient = field(probes, 0.2, recipe, mode)
        for axis in range(3):
            shift = np.eye(3)[axis] * 1e-4
            difference = (
                field(probes + shift, 0.2, recipe, mode)[1]
                - field(probes - shift, 0.2, recipe, mode)[1]
            ) / 2e-4
            assert np.max(abs(difference - gradient[:, axis])) < 1e-6
        colour, bump, gradient = field(probes, 1e6, recipe, mode)
        assert np.array_equal(colour, np.ones(100))
        assert np.array_equal(bump, np.zeros(100)) and np.array_equal(
            gradient, np.zeros((100, 3))
        )
    repeated = {}
    for mode in ("current", "bands", "cavities"):
        times = []
        for _ in range(5):
            start = perf_counter()
            field(world, 0.2, recipe, mode)
            times.append(1000 * (perf_counter() - start))
        repeated[mode] = {"samples": times, "median_ms": float(np.median(times))}
    report = {
        "cases": rows,
        "repeated_timings": repeated,
        "bounds": "analytic value-noise gradient <= 1.5 sqrt(3); fixed footprint differentiation",
        "filtering": "Gaussian local-linear phase model with conservative isotropic gradient bound; not exact nonlinear pixel integral",
        "scope": "Same metric cliff and material. Synthetic material preview, not a DEM/place/GPU proof.",
        "field_hashes": {"current": 24, "bands": 16, "cavities": 24},
        "additional_math": "bands: four Fourier terms; cavities: two broadened Gaussian profiles",
    }
    (out / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
