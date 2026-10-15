"""Independent fixed-cloud-cell moments for the real opaque Scene/Depth captures."""

import json

import numpy as np
from cloud_field import Layer, density, segments
from cloud_moments import integrate_cells
from cloud_view_reference import physical_air


def reference(rays, medium, weather, warp=1):
    drift = weather[2, :3].astype(float)
    if drift[1] != 0:
        raise ValueError("this declared comparison has horizontal layer advection")
    layer = Layer(
        radius_m=float(weather[0, 0]),
        base_m=float(weather[0, 1]),
        thickness_m=float(weather[0, 2]),
        extinction_per_m=float(weather[0, 3]),
        cover=float(weather[1, 0]),
        wind_ms=float(np.linalg.norm(drift)),
        wind_from_deg=float(np.degrees(np.arctan2(-drift[0], drift[2]))),
    )
    origins, directions, query = rays[:, :3], rays[:, 3:6], rays[:, 10] * 1000
    _, extent = physical_air(rays, medium)
    bounds = segments(origins, directions, layer)
    begins, lengths, optical = [], [], []
    bound = np.ones(len(rays))
    for interval in range(2):
        start = bounds[:, interval, 0]
        end = np.maximum(start, np.minimum(bounds[:, interval, 1], extent))
        bound = np.maximum(bound, np.minimum(end, query))
        for cell in range(16):
            a, b = (cell / 16) ** warp, ((cell + 1) / 16) ** warp
            begin = start + (end - start) * a
            finish = start + (end - start) * b
            sample = origins + directions * (0.5 * (begin + finish))[:, None]
            length = np.clip(query - begin, 0, finish - begin)
            begins.append(begin)
            lengths.append(length)
            optical.append(density(sample, layer, 1.0) * length)
    weight, first = integrate_cells(
        np.stack(begins, axis=-1),
        np.stack(lengths, axis=-1),
        np.stack(optical, axis=-1),
    )
    return np.c_[weight, first], bound


def capture(metadata):
    data = json.loads(metadata.read_text())
    stem = str(metadata)[:-5]
    images = np.fromfile(stem + ".images.bin", "<f4").reshape(
        5, data["height"] * data["width"], 4
    )
    rays = np.fromfile(stem + ".rays.bin", "<f8").reshape(-1, 11)
    parameters = np.fromfile(stem + ".parameters.bin", "<f4")
    return data, images, rays, parameters[:20], parameters[20:].reshape(6, 4)


def judge(metadata, baseline_metadata):
    data, images, rays, medium, weather = capture(metadata)
    _, baseline, before_rays, before_medium, before_weather = capture(baseline_metadata)
    for before, after in (
        (baseline[:4], images[:4]),
        (before_rays, rays),
        (before_medium, medium),
        (before_weather, weather),
    ):
        np.testing.assert_array_equal(after.view(np.uint8), before.view(np.uint8))
    mask = images[2, :, 3] > 0
    expected, bound = reference(rays, medium, weather)
    expected[~mask] = 0
    actual = images[4, :, :2].astype(float)
    if not np.isfinite(actual).all() or np.any(actual < 0) or np.any(actual[:, 0] > 1):
        raise AssertionError("actual raster moments are invalid")
    error = np.abs(actual - expected)
    error[:, 1] /= bound
    if np.max(error) > 1e-4:
        raise AssertionError(
            f"actual raster raw moments fail the fixed dimensionless limit: {error.max()}"
        )
    if np.any(actual[~mask] != 0):
        raise AssertionError("already composed eye sky has no new front-cloud moments")
    np.testing.assert_array_equal(images[4, :, 2], baseline[4, :, 0])
    if weather[1, 0] > 0:
        np.testing.assert_array_equal(images[4, :, 3], baseline[4, :, 2])
    elif np.any(actual != 0):
        raise AssertionError("known-zero low cover stays zero weight/first moment")
    return {
        "case": data["name"],
        "pixels": len(rays),
        "max_m0_abs": float(error[:, 0].max()),
        "max_normalized_m1_abs": float(error[:, 1].max()),
        "max_m1_m_abs": float(np.max(np.abs(actual[:, 1] - expected[:, 1]))),
        "density_evaluations": int(images[4, :, 2].sum()),
        "radiance_transmittance_scene_depth_bit_identical": True,
    }
