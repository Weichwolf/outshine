"""Linear single-scattering reference plus an explicitly approximate diffusion term."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_comparison import view_rays
from cloud_field import Layer, density, optical_depth, segments, sphere_roots
from cloud_moments import centroid_fraction
from PIL import Image

SUN = np.array([-0.6, 0.8, 0.0])
SUN_IRRADIANCE = np.array([1.0, 0.97, 0.92])
SKY_RADIANCE = np.array([0.10, 0.12, 0.16])
_GAUSS_NODES, _GAUSS_WEIGHTS = np.polynomial.legendre.leggauss(3)
GAUSS3 = ((_GAUSS_NODES + 1.0) * 0.5, _GAUSS_WEIGHTS * 0.5)


def source(position, direction, layer, seconds=0.0, light_samples=6, gauss3=False):
    if gauss3 and light_samples != 3:
        raise ValueError("Gauss3 requires exactly three light samples")
    sun_depth = optical_depth(
        position,
        SUN,
        layer,
        light_samples,
        seconds,
        quadrature=GAUSS3 if gauss3 else None,
    )
    cosine = np.sum(direction * SUN, axis=-1)
    g = 0.65
    square = 1.0 + g * g - 2.0 * g * cosine
    phase = (1.0 - g * g) / (4.0 * np.pi * square**1.5)
    direct = phase * np.exp(-sun_depth)
    multiple = 0.15 * np.exp(-0.25 * sun_depth) / (4.0 * np.pi)
    return (direct + multiple)[..., None] * SUN_IRRADIANCE + SKY_RADIANCE


def radiance(
    origin,
    direction,
    layer,
    maximum_m=np.inf,
    seconds=0.0,
    view_samples=16,
    light_samples=6,
    warp=1,
    source_centroid=False,
    light_gauss3=False,
):
    intervals = segments(origin, direction, layer)
    value = np.zeros(direction.shape)
    through = np.ones(direction.shape[:-1])
    for interval in range(2):
        begin = intervals[..., interval, 0]
        end = np.maximum(begin, np.minimum(intervals[..., interval, 1], maximum_m))
        if not np.any(end > begin):
            continue
        for tap in range(view_samples):
            start_fraction, end_fraction = (
                (tap / view_samples) ** warp,
                ((tap + 1) / view_samples) ** warp,
            )
            step_m = (end - begin) * (end_fraction - start_fraction)
            distance = begin + (start_fraction + end_fraction) * 0.5 * (end - begin)
            position = origin + distance[..., None] * direction
            extinction = density(position, layer, seconds)
            step_through = np.exp(-extinction * step_m)
            if source_centroid:
                fraction = centroid_fraction(extinction * step_m)
                distance = begin + (
                    start_fraction + fraction * (end_fraction - start_fraction)
                ) * (end - begin)
                position = origin + distance[..., None] * direction
            value += (through * (1.0 - step_through))[..., None] * source(
                position, direction, layer, seconds, light_samples, light_gauss3
            )
            through *= step_through
    return np.concatenate([value, through[..., None]], axis=-1)


def background(origin, directions, layer):
    blend = np.clip(directions[..., 1], 0.0, 1.0)
    sky = (1.0 - blend[..., None]) * np.array([0.55, 0.67, 0.80]) + blend[
        ..., None
    ] * np.array([0.18, 0.40, 0.85])
    sky += (np.sum(directions * SUN, axis=-1) > np.cos(0.0046542))[
        ..., None
    ] * np.array([5000.0, 4800.0, 4400.0])
    ground_near, _, ground_hit = sphere_roots(origin, directions, 0.0, layer.radius_m)
    ground = ground_hit & (ground_near > 0.0)
    sky = np.where(ground[..., None], np.array([0.04, 0.06, 0.025]), sky)
    return sky, np.where(ground, ground_near, np.inf)


def picture(width, height, layer, view_samples=16, light_samples=6, warp=1):
    origin = np.array([0.0, 1772.0, 0.0])
    directions = view_rays(width, height, 240.0, -0.5, 33.97)
    sky, maximum = background(origin, directions, layer)
    cloud = radiance(
        origin,
        directions,
        layer,
        maximum,
        view_samples=view_samples,
        light_samples=light_samples,
        warp=warp,
    )
    return np.concatenate(
        [cloud[..., :3] + cloud[..., 3:4] * sky, cloud[..., 3:4]], axis=-1
    )


def save_picture(linear, path):
    mapped = 1.0 - np.exp(-3.0 * np.maximum(linear[..., :3], 0.0))
    srgb = np.where(
        mapped <= 0.0031308, mapped * 12.92, 1.055 * mapped ** (1 / 2.4) - 0.055
    )
    Image.fromarray(np.uint8(np.clip(srgb * 255, 0, 255))).save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--width", type=int, default=240)
    parser.add_argument("--height", type=int, default=135)
    parser.add_argument("--cover", type=float, default=0.65)
    parser.add_argument("--compare", action="store_true")
    parser.add_argument("--view-samples", type=int, default=16)
    parser.add_argument("--light-samples", type=int, default=6)
    parser.add_argument("--warp", type=int, choices=[1, 2], default=1)
    args = parser.parse_args()
    if not (
        1 <= args.width <= 4096
        and 1 <= args.height <= 4096
        and 1 <= args.view_samples <= 4096
        and 1 <= args.light_samples <= 256
        and 0.0 <= args.cover <= 1.0
    ):
        parser.error(
            "require dimensions/view steps in [1,4096], light steps in [1,256], cover in [0,1]"
        )
    args.output.mkdir(parents=True, exist_ok=True)
    values = (
        np.fromfile(args.input, dtype=np.float32).reshape(args.height, args.width, 4)
        if args.input
        else picture(
            args.width,
            args.height,
            Layer(cover=args.cover),
            args.view_samples,
            args.light_samples,
            args.warp,
        )
    )
    assert np.isfinite(values).all() and np.all(values >= 0.0)
    assert np.all(values[..., 3] <= 1.0)
    save_picture(values, args.output / "cloud-picture.png")
    report = {
        "width": args.width,
        "height": args.height,
        "cover": args.cover,
        "source": "Native GLSL replay" if args.input else "NumPy float64",
        "view_samples": args.view_samples,
        "light_samples": args.light_samples,
        "warp": args.warp,
        "scope": "Declared source light and spherical ground; no clear-air, terrain, client, or GPU-budget proof.",
    }
    if args.compare:
        reference = picture(
            args.width,
            args.height,
            Layer(cover=args.cover),
            args.view_samples,
            args.light_samples,
            args.warp,
        )
        error = np.abs(values - reference)
        report.update(
            max_linear_abs=float(error.max()),
            p99_linear_abs=float(np.quantile(error, 0.99)),
        )
        assert error.max() < 0.0001
    (args.output / "radiance-report.json").write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report))


if __name__ == "__main__":
    main()
