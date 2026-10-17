"""Independent float64 slab reference for the captured native opaque cloud-front frames."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_field import Layer, density, segments
from cloud_radiance import source


def physical_air(rays, medium):
    radius, cosine = rays[:, 6], rays[:, 7]
    projection = radius * cosine
    discriminant = projection**2 + float(medium[1]) ** 2 - radius**2
    root = np.sqrt(np.maximum(0.0, discriminant))
    begin = np.maximum(0.0, -projection - root) * 1000.0
    end = np.maximum(0.0, -projection + root) * 1000.0
    ground = projection**2 + float(medium[0]) ** 2 - radius**2
    hit = (cosine < 0.0) & (ground >= 0.0)
    ground_near = (-projection - np.sqrt(np.maximum(0.0, ground))) * 1000.0
    end = np.where(hit, np.minimum(end, ground_near), end)
    valid = (radius >= medium[0]) & (discriminant >= 0.0) & (end > begin)
    return np.where(valid, begin, 0.0), np.where(valid, end, 0.0)


def coefficients(rays, medium):
    cosine = rays[:, 9]
    rayleigh_phase = 3.0 * (1.0 + cosine**2) / (16.0 * np.pi)
    g = float(medium[14])
    mie_phase = (
        3.0
        * (1.0 - g * g)
        * (1.0 + cosine**2)
        / (8.0 * np.pi * (2.0 + g * g) * (1.0 + g * g - 2.0 * g * cosine) ** 1.5)
    )
    rayleigh = medium[4:7].astype(float)
    scatter, extinction = float(medium[7]), float(medium[11])
    sunlight = np.array([0.75, 0.5, 0.25])
    psi = np.array([0.015625, 0.03125, 0.0625])
    return (rayleigh + extinction) * 0.001, (
        sunlight * (rayleigh * rayleigh_phase[:, None] + scatter * mie_phase[:, None])
        + (rayleigh + scatter) * psi
    ) * 0.001


def transfer(
    rays, medium, weather, phase_s, view_samples=16, light_samples=6, warp=1, profile=0
):
    if medium[2] < 1e19 or medium[3] < 1e19 or np.any(medium[8:11] != 0.0):
        raise ValueError(
            "this reference requires the declared homogeneous, ozone-free control"
        )
    layer = Layer(
        radius_m=float(weather[0, 0]),
        base_m=float(weather[0, 1]),
        thickness_m=float(weather[0, 2]),
        extinction_per_m=float(weather[0, 3]),
        cover=float(weather[1, 0]),
    )
    if profile == 1:
        if warp != 1:
            raise ValueError(
                "the independent cumulus control requires the existing uniform cell partition"
            )
        from cloud_shape_transfer import transfer as shape_transfer

        return shape_transfer(
            rays,
            medium,
            layer,
            phase_s,
            "cell-smooth",
            view_samples=view_samples,
            light_samples=light_samples,
            query_m=rays[:, 10] * 1000.0,
        )
    if profile != 0:
        raise ValueError("the declared reference profile must be supported explicitly")
    origins, directions, queries = rays[:, :3], rays[:, 3:6], rays[:, 10] * 1000.0
    begin, end = physical_air(rays, medium)
    air_extinction, air_emission = coefficients(rays, medium)
    radiance, through = np.zeros((len(rays), 3)), np.ones((len(rays), 3))
    position = begin.copy()

    def integrate(width, extinction, emission):
        nonlocal radiance, through
        width = np.maximum(0.0, width)[:, None]
        tau = extinction * width
        local = np.divide(
            emission * -np.expm1(-tau),
            extinction,
            out=emission * width,
            where=extinction > 0.0,
        )
        radiance += through * local
        through *= np.exp(-tau)

    if layer.cover > 0.0:
        bounds = segments(origins, directions, layer)
        for interval in range(2):
            start = bounds[:, interval, 0]
            finish = np.minimum(bounds[:, interval, 1], end)
            active = finish > start
            start, finish = (
                np.where(active, start, position),
                np.where(active, finish, position),
            )
            integrate(
                np.minimum(queries, start) - position, air_extinction, air_emission
            )
            position = np.maximum(position, start)
            for tap in range(view_samples):
                a, b = (tap / view_samples) ** warp, ((tap + 1) / view_samples) ** warp
                cell_begin = start + (finish - start) * a
                cell_end = start + (finish - start) * b
                midpoint = (
                    origins + directions * (0.5 * (cell_begin + cell_end))[:, None]
                )
                rho = density(midpoint, layer, phase_s)
                cloud = rho[:, None] * source(
                    midpoint, directions, layer, phase_s, light_samples
                )
                width = np.minimum(queries, cell_end) - np.maximum(position, cell_begin)
                integrate(width, air_extinction + rho[:, None], air_emission + cloud)
                position = np.maximum(position, cell_end)
    integrate(np.minimum(queries, end) - position, air_extinction, air_emission)
    return np.c_[radiance, through]


def judge(metadata, view_samples=16, light_samples=6, warp=1):
    case = json.loads(metadata.read_text())
    stem = str(metadata)[:-5]
    width, height = case["width"], case["height"]
    pixels = width * height
    images = np.fromfile(stem + ".images.bin", dtype="<f4").reshape(5, pixels, 4)
    rays = np.fromfile(stem + ".rays.bin", dtype="<f8").reshape(pixels, 11)
    parameters = np.fromfile(stem + ".parameters.bin", dtype="<f4")
    medium, weather = parameters[:20], parameters[20:].reshape(-1, 4)
    mask = images[2, :, 3] > 0.0
    expected = transfer(
        rays[mask],
        medium,
        weather,
        case["phase_s"],
        view_samples,
        light_samples,
        warp,
        profile=case.get("cloud_profile", int(weather[1, 1])),
    )
    actual = np.c_[images[2, mask, :3], images[3, mask, :3]].astype(float)
    errors = np.abs(actual - expected)
    composed = images[0, mask, :3] * expected[:, 3:] + expected[:, :3]
    colour_error = np.abs(images[1, mask, :3] - composed)
    work = images[4, mask].astype(float)
    totals = {
        name: None if np.any(work[:, index] < 0.0) else float(work[:, index].sum())
        for index, name in enumerate(
            (
                "view_density_evaluations",
                "cloud_source_evaluations",
                "union_cells",
                "air_source_evaluations",
            )
        )
    }
    row_bytes = (width * 16 + 255) // 256 * 256
    limit = 1e-4
    passed = bool(
        np.all(np.isfinite(images))
        and np.max(errors) <= limit
        and np.max(colour_error) <= limit
    )
    return {
        "case": case["name"],
        "profile": [width, height],
        "receivers": int(mask.sum()),
        "max_radiance_error": float(np.max(errors[:, :3])),
        "max_transmittance_error": float(np.max(errors[:, 3:])),
        "max_composition_error": float(np.max(colour_error)),
        "limit": limit,
        "logical_work_per_frame": totals,
        "declared_capture_storage_bytes": {
            "texture_texels": 84 * pixels + 64,
            "readback_buffers": 5 * row_bytes * height,
            "readback_copy_per_frame": 5 * row_bytes * height,
            "capture_vectors_per_frame": 80 * pixels,
        },
        "passed": passed,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view-samples", type=int, default=16)
    parser.add_argument("--light-samples", type=int, default=6)
    parser.add_argument("--warp", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    rows = [
        judge(path, args.view_samples, args.light_samples, args.warp)
        for path in sorted(args.captures.glob("actual.*.json"))
    ]
    if not rows:
        parser.error("no actual native raster captures")
    args.output.write_text(json.dumps(rows, indent=2) + "\n")
    print(
        json.dumps(
            {"cases": len(rows), "failures": [row for row in rows if not row["passed"]]}
        )
    )
    return any(not row["passed"] for row in rows)


if __name__ == "__main__":
    raise SystemExit(main())
