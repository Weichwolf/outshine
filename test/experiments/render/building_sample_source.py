"""Load the exact native512 capture-lit operands, preserving their old provenance."""

import hashlib
import json

import numpy as np
from building_sample_inputs import crop_view, digest, full_buffer_digest
from field_view_sampling import FieldGrid


def read_source(directory, high_directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    high = json.loads((high_directory / "manifest.json").read_text())
    geometry = directory.parent / "original-geometry/manifest.json"
    path = directory / "samples.npz"
    if (
        manifest["version"] != 1
        or manifest["native_terminal_exit"] != 0
        or manifest["original_geometry_manifest_sha256"] != digest(geometry)
        or manifest["original_control_manifest_sha256"]
        != digest(high_directory / "manifest.json")
        or path.stat().st_size != manifest["npz_bytes"]
        or digest(path) != manifest["npz_sha256"]
        or manifest["query"] != high["cases"][0]["query"]
    ):
        raise ValueError("Exact original512 capture/query binding required")
    arrays = {}
    with np.load(path, allow_pickle=False) as saved:
        if set(saved.files) != set(manifest["arrays"]):
            raise ValueError("Complete native source/prefix inventory required")
        for name, binding in manifest["arrays"].items():
            value = saved[name]
            if (
                str(value.dtype) != binding["dtype"]
                or list(value.shape) != binding["shape"]
                or not np.isfinite(value).all()
                or hashlib.sha256(value.tobytes()).hexdigest() != binding["sha256"]
            ):
                raise ValueError("Native source operand changed: " + name)
            arrays[name] = value
    if (
        arrays["source_lit_rgba"].shape != (512, 512, 4)
        or arrays["stored_field_bytes"].nbytes != manifest["stored_field_bytes"]
        or manifest["stored_field_sha256"]
        != manifest["arrays"]["stored_field_bytes"]["sha256"]
    ):
        raise ValueError("Bounded old512 stored-field provenance required")
    for binding in manifest["native_buffers"].values():
        value = arrays[binding["array"]]
        checksum = (
            full_buffer_digest(
                value, binding["clear"], binding["raster"], binding["crop"]
            )
            if "crop" in binding
            else hashlib.sha256(value.tobytes()).hexdigest()
        )
        if checksum != binding["sha256"]:
            raise ValueError("Full native source/prefix buffer changed")
    camera = manifest["capture"]
    grid = FieldGrid(
        tuple(manifest["atlas_centre_m"]),
        tuple(manifest["toward_eye"]),
        manifest["atlas_half_extent_m"],
    )
    if (
        not camera["orthographic"]
        or [camera["width_px"], camera["height_px"]] != [512, 512]
        or not np.allclose(
            [camera["near_m"], camera["far_m"], camera["x_mag_m"], camera["y_mag_m"]],
            grid.half_extent_m * np.array([1, 5, 1, 1]),
            rtol=1e-12,
            atol=0,
        )
        or not np.allclose(
            camera["eye_m"],
            np.asarray(grid.centre)
            + 3 * grid.half_extent_m * np.asarray(grid.toward_eye),
            rtol=0,
            atol=1e-9,
        )
        or not np.allclose(grid.axes(), [camera["right"], camera["up"]], atol=1e-12)
        or not np.allclose(camera["forward"], -np.asarray(grid.toward_eye), atol=1e-12)
    ):
        raise ValueError("Exact native capture placement required")
    hits = arrays["source_depth"] > 0
    if (
        int(hits.sum()) != manifest["covered_source_samples"]
        or not np.array_equal(arrays["source_lit_rgba"][:, :, 3], hits)
        or not np.array_equal(arrays["source_surface"] > 0, hits)
    ):
        raise ValueError("Native point coverage and source-lit coverage differ")
    return manifest, arrays, grid, crop_view(high, manifest["query"])
