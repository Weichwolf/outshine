"""Independent physical-boundary controls for actual native cloud/air sky captures."""

import json

import numpy as np
from cloud_view_reference import physical_air, transfer


def read(metadata):
    case = json.loads(metadata.read_text())
    stem = str(metadata)[:-5]
    count = case["width"] * case["height"]
    images = np.fromfile(stem + ".images.bin", "<f4").reshape(5, count, 4)
    rays = np.fromfile(stem + ".rays.bin", "<f8").reshape(count, 11)
    parameters = np.fromfile(stem + ".parameters.bin", "<f4")
    return case, images, rays, parameters[:20], parameters[20:].reshape(6, 4)


def judge(metadata):
    case, images, rays, medium, weather = read(metadata)
    if not case["boundary_sky"]:
        raise ValueError(
            "a boundary-source capture must explicitly declare that contract"
        )
    active = np.linalg.norm(rays[:, 3:6], axis=1) > 0
    sky = active & (images[2, :, 3] == 0)
    begin, end = physical_air(rays[active], medium)
    queries = rays[active].copy()
    selected_sky = sky[active]
    queries[selected_sky, 10] = end[selected_sky] * 0.001
    endpoint_error_m = np.abs(
        images[3, sky, 3].astype(float) * 1000 - end[selected_sky]
    )
    expected = transfer(queries, medium, weather, case["phase_s"])
    actual = np.c_[images[2, active, :3], images[3, active, :3]].astype(float)
    error = np.abs(expected - actual)
    composed = images[0, active, :3] * expected[:, 3:] + expected[:, :3]
    composition_error = np.abs(images[1, active, :3] - composed)
    np.testing.assert_array_equal(
        images[1, active, 3].view(np.uint32), images[0, active, 3].view(np.uint32)
    )
    # Existing receiver reconstruction retains its separate2mm check in the host.
    # A full planetary endpoint also includes FP32 radius/ray angular quantisation.
    if not np.any(sky) or np.max(endpoint_error_m) > 1.0:
        raise AssertionError(
            "the independently intersected physical sky boundary disagrees"
        )
    if np.max(error) > 1e-4 or np.max(composition_error) > 1e-4:
        raise AssertionError(
            f"{case['name']}: transfer={np.max(error)} composition={np.max(composition_error)}"
        )
    if not np.all(np.isfinite(images)):
        raise AssertionError("all native fields must remain finite")
    work = images[4, active].astype(float)
    counters = {
        name: None if np.any(work[:, index] < 0) else int(work[:, index].sum())
        for index, name in enumerate(
            ("density_evaluations", "cloud_sources", "joint_cells", "air_sources")
        )
    }
    return {
        "case": case["name"],
        "profile": [case["width"], case["height"]],
        "sky_pixels": int(np.sum(sky)),
        "surface_pixels": int(np.sum(active & ~sky)),
        "max_endpoint_error_m": float(np.max(endpoint_error_m)),
        "max_radiance_error": float(np.max(error[:, :3])),
        "max_transmittance_error": float(np.max(error[:, 3:])),
        "max_composition_error": float(np.max(composition_error)),
        "vacuum_sky_pixels": int(np.sum(end[selected_sky] == begin[selected_sky])),
        "work": counters,
    }


def cross_cases(folder, legacy=None):
    def capture(name):
        return read(folder / f"actual.{name}.json")

    _, base, _, _, _ = capture("sky-only")
    for name in ("sky-near", "sky-far"):
        _, images, _, _, _ = capture(name)
        np.testing.assert_array_equal(images.view(np.uint32), base.view(np.uint32))
    _, black, _, _, _ = capture("sky-black")
    _, bright, _, _, _ = capture("sky-bright")
    np.testing.assert_array_equal(black[2:].view(np.uint32), bright[2:].view(np.uint32))
    change = (bright[0, :, :3] - black[0, :, :3]) * black[3, :, :3]
    boundary_error = np.max(np.abs(bright[1, :, :3] - black[1, :, :3] - change))
    if boundary_error > 2e-7:
        raise AssertionError(
            "boundary radiance must enter once through joint transmittance"
        )
    for name in ("space-outward", "space-miss"):
        _, images, _, _, _ = capture(name)
        np.testing.assert_array_equal(
            images[1].view(np.uint32), images[0].view(np.uint32)
        )
        np.testing.assert_array_equal(images[2], 0)
        np.testing.assert_array_equal(images[3, :, :3], 1)
        np.testing.assert_array_equal(images[3, :, 3], 0)
    opaque_pixels = 0
    for metadata in sorted(legacy.glob("actual.*.json")) if legacy is not None else ():
        _, old, _, _, _ = read(metadata)
        _, current, _, _, _ = read(folder / metadata.name)
        mask = old[2, :, 3] > 0
        np.testing.assert_array_equal(
            old[:, mask].view(np.uint32), current[:, mask].view(np.uint32)
        )
        opaque_pixels += int(np.sum(mask))
    _, cloudy, _, _, _ = capture("declared-perspective")
    _, clear, _, _, _ = capture("captured-clear")
    sky = cloudy[2, :, 3] == 0
    cloud_change = float(np.max(np.abs(cloudy[1, sky, :3] - clear[1, sky, :3])))
    _, wind, _, _, _ = capture("wind-advance")
    wind_change = float(np.max(np.abs(cloudy[1, sky, :3] - wind[1, sky, :3])))
    if (
        cloud_change <= 1e-3
        or wind_change <= 1e-6
        or (legacy is not None and opaque_pixels == 0)
    ):
        raise AssertionError(
            "clouds, wind and original actual opaque controls must be exercised"
        )
    return {
        "far_planes_m": [50, 6000, 600000],
        "far_plane_raw_identity": True,
        "vacuum_raw_identity": True,
        "opaque_raw_identical_pixels": opaque_pixels,
        "max_boundary_linearity_error": float(boundary_error),
        "visible_cloud_rgb_change": cloud_change,
        "wind_step_rgb_change": wind_change,
    }
