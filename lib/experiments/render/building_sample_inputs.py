"""Bound actual native point/sample readbacks and their orthographic capture grid."""

import hashlib
import json
import math

import numpy as np
from field_composition import opaque_points
from field_view_sampling import FieldGrid, ImageView


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def crop_view(manifest, query):
    low, high = np.asarray(manifest["crop"]).reshape(2, 2)
    if not np.array_equal(low + high, manifest["raster"]):
        raise ValueError(
            "This control requires a centred native crop with unchanged rays"
        )
    return ImageView(
        tuple(query["eye"]),
        tuple(map(tuple, query["basis"])),
        query["focal_px"],
        *map(int, high - low),
    )


def full_buffer_digest(value, clear, raster, crop):
    channels = value.shape[2]
    pixel = np.asarray(clear, dtype="<f4").tobytes()
    if len(pixel) != channels * 4:
        raise ValueError("Native clear value has the wrong channel count")
    row = pixel * int(raster[0])
    left, right = pixel * int(crop[0]), pixel * int(raster[0] - crop[2])
    checksum = hashlib.sha256()
    for y in range(int(raster[1])):
        if crop[1] <= y < crop[3]:
            checksum.update(left)
            checksum.update(value[y - crop[1]].tobytes())
            checksum.update(right)
        else:
            checksum.update(row)
    return checksum.hexdigest()


def readbacks(directory):
    path = directory / "captures.npz"
    manifest = json.loads((directory / "manifest.json").read_text())
    raster = np.asarray(manifest["raster"])
    crop = np.asarray(manifest["crop"])
    if (
        manifest["version"] != 1
        or raster.tolist() != [1280, 720]
        or crop.shape != (4,)
        or crop.dtype.kind not in "iu"
        or np.any(crop[:2] < 0)
        or np.any(crop[2:] > raster)
        or np.any(crop[2:] <= crop[:2])
        or path.stat().st_size != manifest["npz_bytes"]
        or digest(path) != manifest["npz_sha256"]
        or not 1 <= len(manifest["cases"]) <= 16
    ):
        raise ValueError("Bounded lossless native1280x720 readbacks required")
    arrays, names = {}, set()
    width, height = crop[2:] - crop[:2]
    with np.load(path, allow_pickle=False) as saved:
        for index, case in enumerate(manifest["cases"]):
            axis = case["query"]["filter_axis_samples"]
            if type(axis) is not int or not 1 <= axis <= 64:
                raise ValueError(
                    "Explicit bounded native reference sample axis required"
                )
            for representation in ("original", "samples"):
                for suffix in ("linear", "depth", "normal", f"ss{axis**2}"):
                    name = f"case{index}_{representation}_{suffix}"
                    names.add(name)
                    value = saved[name]
                    channels = 1 if suffix == "depth" else 4
                    if (
                        value.dtype != np.dtype("<f4")
                        or value.shape != (height, width, channels)
                        or not np.isfinite(value).all()
                        or name not in case["buffers"]
                    ):
                        raise ValueError(
                            "Complete finite Float32 native buffer required"
                        )
                    binding = case["buffers"][name]
                    if (
                        full_buffer_digest(value, binding["clear"], raster, crop)
                        != binding["full_sha256"]
                    ):
                        raise ValueError("Lossless full native buffer checksum changed")
                    arrays[name] = value
        if names != set(saved.files):
            raise ValueError("Native readback inventory changed")
    return manifest, arrays


def same_sources(first, second):
    for key in (
        "native_commit",
        "native_driver_sha256",
        "native_binary_sha256",
        "atlas_sha256",
        "original_geometry_files",
        "catalogue",
        "bounds",
        "raster",
        "crop",
        "lighting",
    ):
        if first[key] != second[key]:
            raise ValueError("Native source/capture binding changed: " + key)


def capture_fields(manifest, arrays):
    if len(manifest["cases"]) != 1:
        raise ValueError("One explicitly orthographic capture control required")
    case = manifest["cases"][0]
    camera = case["query"].get("capture", {})
    samples = case["draws"]["samples"]
    pixels = math.isqrt(samples["image_storage_bytes"] // 32)
    if (
        not camera.get("orthographic")
        or samples["images"] != 2
        or not 3 <= pixels <= 512
        or 32 * pixels**2 != samples["image_storage_bytes"]
        or case["query"]["filter_axis_samples"] != 1
    ):
        raise ValueError("Actual bounded two-Float32-image sample capture required")
    extent = camera["near_m"]
    raster = np.asarray(manifest["raster"])
    if (
        not math.isfinite(extent)
        or extent <= 0
        or not math.isclose(camera["far_m"], 5 * extent, rel_tol=1e-12)
        or [camera["width_px"], camera["height_px"]] != raster.tolist()
        or not np.allclose(
            2 * np.array([camera["x_mag_m"], camera["y_mag_m"]]) / raster,
            2 * extent / pixels,
            rtol=1e-12,
            atol=0,
        )
        or np.any((raster - pixels) % 2)
    ):
        raise ValueError("Native capture pitch/clip/padded-raster registration changed")
    forward = np.asarray(camera["forward"], float)
    centre = np.asarray(camera["eye_m"], float) + 3 * extent * forward
    grid = FieldGrid(tuple(centre), tuple(-forward), extent)
    if not np.allclose(
        grid.axes(), [camera["right"], camera["up"]], rtol=0, atol=1e-12
    ):
        raise ValueError("Canonical capture axes changed")
    source_origin = (raster - pixels) // 2
    crop = np.asarray(manifest["crop"])
    low, high = crop[:2] - source_origin, crop[2:] - source_origin
    if np.any(low < 0) or np.any(high > pixels):
        raise ValueError("Stored capture crop must lie inside the original sample grid")
    fields = {}
    for representation in ("original", "samples"):
        name = f"case0_{representation}_linear"
        if case["buffers"][name]["clear"] != [0, 0, 0, 0]:
            raise ValueError("Point-field clear value must be transparent black")
        field = np.zeros((pixels, pixels, 4), np.float32)
        field[low[1] : high[1], low[0] : high[0]] = arrays[name]
        opaque_points(field, 262144)
        fields[representation] = field
    return grid, fields
