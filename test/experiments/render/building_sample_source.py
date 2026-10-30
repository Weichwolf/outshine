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


def read_query(directory, source_directory, high_directory):
    """Exact archived depth-preserving512 source-mesh opponent, no new GPU run."""
    manifest = json.loads((directory / "manifest.json").read_text())
    source_path = source_directory / "manifest.json"
    high_path = high_directory / "manifest.json"
    source, high = (
        json.loads(source_path.read_text()),
        json.loads(high_path.read_text()),
    )
    path = directory / "captures.npz"
    if (
        manifest["version"] != 1
        or manifest["native_terminal_exit"] != 0
        or manifest["native_mode"] != "source-mesh, stored_samples=true"
        or manifest["source_fields_manifest_sha256"] != digest(source_path)
        or manifest["original_control_manifest_sha256"] != digest(high_path)
        or manifest["native_binary_sha256"] != source["native_binary_sha256"]
        or manifest["parent_atlas_sha256"] != source["parent_atlas_sha256"]
        or manifest["query"] != source["query"]
        or manifest["query"] != high["cases"][0]["query"]
        or manifest["raster"] != high["raster"]
        or manifest["crop"] != high["crop"]
        or manifest["query"]["filter_axis_samples"] != 64
        or path.stat().st_size != manifest["npz_bytes"]
        or digest(path) != manifest["npz_sha256"]
    ):
        raise ValueError(
            "Exact512 depth-preserving query/source/original binding required"
        )
    arrays = {}
    with np.load(path, allow_pickle=False) as saved:
        expected = {"linear", "depth", "normal", "ss4096"}
        bindings = list(manifest["native_buffers"].values())
        if (
            set(saved.files) != expected
            or {b["array"] for b in bindings} != expected
            or len(bindings) != 4
        ):
            raise ValueError("Complete native query inventory required")
        for binding in bindings:
            name = binding["array"]
            value = saved[name]
            channels = 1 if name == "depth" else 4
            w, h = np.asarray(manifest["crop"])[2:] - manifest["crop"][:2]
            if (
                value.dtype != np.dtype("<f4")
                or value.shape != (h, w, channels)
                or list(value.shape) != binding["shape"]
                or not np.isfinite(value).all()
                or hashlib.sha256(value.tobytes()).hexdigest() != binding["crop_sha256"]
                or full_buffer_digest(
                    value, binding["clear"], manifest["raster"], manifest["crop"]
                )
                != binding["full_sha256"]
                or binding["full_bytes"]
                != int(np.prod(manifest["raster"])) * channels * 4
            ):
                raise ValueError("Lossless full native query buffer changed: " + name)
            arrays[name] = value
    report = manifest["native_report"]["cases"]
    if len(report) != 1 or (
        report[0]["query"] != manifest["query"]["name"]
        or report[0]["triangles"] != 2 * source["covered_source_samples"]
        or report[0]["vertices"] != 4 * source["covered_source_samples"]
        or report[0]["images"] != 2
        or report[0]["image_storage_bytes"] != 512 * 512 * 32
    ):
        raise ValueError("Depth-preserving native source inventory changed")
    return manifest, arrays
