"""Export native, camera-bound field/background inputs; all private work stays in temp."""

import argparse
import hashlib
import json
import sqlite3
import subprocess
import tempfile
import time
from contextlib import closing
from pathlib import Path

import numpy as np
from field_background_inputs import (
    ATTRIBUTES,
    DATA,
    digest,
    original_inputs,
    placements,
    source_bounds,
    stage_mesh,
)


def native(binary, directory, mode):
    command = [str(binary), str(directory), mode]
    started = time.perf_counter()
    with (directory / "native.log").open("w") as log:
        process = subprocess.run(
            command, check=False, timeout=120, stdout=log, stderr=subprocess.STDOUT
        )
    receipt = {
        "terminal": process.returncode,
        "elapsed_ms": 1000 * (time.perf_counter() - started),
        "mode": mode,
        "binary_sha256": digest(binary),
    }
    (directory / "command.json").write_text(json.dumps(receipt, indent=2) + "\n")
    process.check_returncode()
    return receipt


def read(path, shape):
    values = np.fromfile(path, "<f4")
    if values.size != int(np.prod(shape)) or not np.isfinite(values).all():
        raise ValueError("finite native output with exact shape required: " + path.name)
    return values.reshape(shape)


def capture(directory, name, camera):
    width, height = camera["width_px"], camera["height_px"]
    colour = read(directory / (name + "-source-field.f32"), (height, width, 4))
    depth = read(directory / (name + "-source-depth.f32"), (height, width))
    if not (colour[:, :, :3] > 0).any() or not (depth > 0).any():
        raise ValueError("capture must contain lit geometry")
    if np.any((colour[:, :, 3] < 0) | (colour[:, :, 3] > 1)):
        raise ValueError("capture coverage outside [0,1]")
    if np.any((depth < 0) | (depth > 1)):
        raise ValueError("reverse-Z capture depth outside [0,1]")
    return {"colour": colour, "depth": depth}


def qualify(arrays, projected):
    front = arrays["foreground_depth"]
    result = {}
    for mode in ("separated", "interleaved"):
        back = arrays[mode + "-background_depth"]
        combined = arrays[mode + "-original_depth"]
        common = (front > 0) & (back > 0)
        before = int(np.count_nonzero(common & (front > back)))
        after = int(np.count_nonzero(common & (front < back)))
        nearest = np.where(
            (front >= back)[:, :, None],
            arrays["foreground_linear"],
            arrays[mode + "-background_linear"],
        )
        if not np.array_equal(nearest, arrays[mode + "-original_linear"]):
            raise ValueError(
                "native combined colour differs from nearest original hits"
            )
        if not np.array_equal(combined, np.maximum(front, back)):
            raise ValueError("combined original depth differs from native nearest hits")
        if (
            not before
            or (mode == "separated" and after)
            or (mode == "interleaved" and not after)
        ):
            raise ValueError("native rays do not exhibit the required depth ordering")
        low, high = projected[mode + "-background"]
        whole_separated = bool(projected["foreground"][1] < low)
        if (mode == "separated") != whole_separated:
            raise ValueError(
                "whole-geometry depth bounds disagree with control placement"
            )
        colour = arrays[mode + "-original_ss16"]
        fractional = (colour[:, :, 3] > 0) & (colour[:, :, 3] < 1)
        if not fractional.any() or not np.any(colour[:, :, :3] > 0):
            raise ValueError(
                "native reference needs positive colour and partial coverage"
            )
        result[mode] = {
            "common_pixel_centres": int(common.sum()),
            "foreground_nearer": before,
            "background_nearer": after,
            "equal_depth": int(np.count_nonzero(common & (front == back))),
            "whole_depth_intervals_separated": whole_separated,
            "background_forward_interval_m": [low, high],
            "fractional_reference_pixels": int(fractional.sum()),
            "native_union_depth_exact": True,
            "native_union_colour_exact": True,
        }
    foreground = arrays["foreground_ss16"]
    background = arrays["separated-background_ss16"]
    separate = foreground + (1 - foreground[:, :, 3, None]) * background
    delta = np.abs(separate - arrays["separated-original_ss16"])
    result["separated"]["independent_filter_rgb_max_error"] = float(
        delta[:, :, :3].max()
    )
    result["separated"]["independent_filter_coverage_max_error"] = float(
        delta[:, :, 3].max()
    )
    return result


def run(args):
    output = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if output.exists() or output == temporary or not output.is_relative_to(temporary):
        raise ValueError("fresh explicit system-temp output required")
    native_commit = subprocess.check_output(
        ["git", "rev-parse", "--verify", args.native_commit + "^{commit}"],
        cwd=Path(__file__).resolve().parents[3],
        text=True,
    ).strip()
    field = args.field.resolve()
    binary = args.binary.resolve()
    inputs = json.loads((field / "input.json").read_text())
    report = json.loads((field / "radiance-result.json").read_text())
    if report["cases"][0]["query"] != inputs["queries"][0]["name"]:
        raise ValueError("raw capture and input query order differ")
    query = dict(inputs["queries"][0], name="view", filter_axis_samples=4)
    parts, provenance, request = original_inputs()
    for key, value in request.items():
        if (
            key != "source_tests"
            and inputs["source_request"][key] != value
            and (
                key != "cells" or sorted(inputs["source_request"][key]) != sorted(value)
            )
        ):
            raise ValueError("field and whole original source bindings differ: " + key)
    output.mkdir(parents=True)
    arrays = source_bounds(parts)
    projected, commands = {}, {}
    offsets = placements(parts, query)
    forward = np.asarray(query["basis"])[2]
    for name, translations in offsets.items():
        directory = output / name
        points = stage_mesh(directory, parts, translations, query)
        z = (points - query["eye"]) @ forward
        projected[name] = [float(z.min()), float(z.max())]
        commands[name] = native(binary, directory, "--source-mesh")
        for channel, shape in (
            ("linear", (720, 1280, 4)),
            ("depth", (720, 1280)),
            ("ss16", (720, 1280, 4)),
        ):
            arrays[name + "_" + channel] = read(
                directory / ("view-source-mesh." + channel), shape
            )
    camera = report["cases"][0]["source_depth"]
    for key, value in capture(field, inputs["queries"][0]["name"], camera).items():
        arrays["field_capture_" + key] = value
    original = output / "raw-original"
    original.mkdir()
    with (
        closing(
            sqlite3.connect(
                (field / "world-view-assets.sqlite").as_uri() + "?mode=ro", uri=True
            )
        ) as source,
        closing(sqlite3.connect(original / "world-view-assets.sqlite")) as target,
    ):
        source.backup(target)
    original_input = dict(inputs, queries=[dict(query, filter_axis_samples=1)])
    original_input["field_original"] = str(output / "foreground/geometry/0/input.json")
    original_input["source_input"] = str(original / "absent-source/input.json")
    (original / "input.json").write_text(json.dumps(original_input, indent=2) + "\n")
    commands["raw-original"] = native(binary, original, "--radiance-world")
    reference = json.loads((original / "radiance-result.json").read_text())
    if reference["cases"][0]["source_depth"] != camera:
        raise ValueError("original and field capture cameras differ")
    for key, value in capture(original, "view", camera).items():
        arrays["original_capture_" + key] = value
    checks = qualify(arrays, projected)
    np.savez_compressed(output / "captures.npz", **arrays)
    manifest = {
        "version": 1,
        "native_commit": native_commit,
        "binary_sha256": digest(binary),
        "driver_sources_sha256": {
            name: digest(Path(__file__).with_name(name))
            for name in (
                "export_field_backgrounds.py",
                "field_background_inputs.py",
                "native_building_cell_reference.py",
                "native_building_impostors.cpp",
                "BuildingRadianceField.cpp",
            )
        },
        "source": provenance,
        "geometry_loader": "field_background_inputs.original_inputs",
        "geometry_arrays_sha256": {
            f"part{at}_{key}": hashlib.sha256(part[key].tobytes()).hexdigest()
            for at, part in enumerate(parts)
            for key in (*ATTRIBUTES, "binding")
        },
        "input_files": {
            str(path.relative_to(DATA)): digest(path)
            for path in (
                DATA / "source-plane-request.json",
                DATA / "original-inputs.tar.gz",
                DATA / "native-cells/manifest.json",
            )
        },
        "field_capture": {
            "input_sha256": digest(field / "input.json"),
            "report_sha256": digest(field / "radiance-result.json"),
            "asset_key": report["cases"][0]["field_asset_key"],
            "selected_view": report["cases"][0]["selected_view"],
            "device": report["cases"][0]["device"],
            "query": inputs["queries"][0],
            "raw_colour_sha256": digest(
                field / (inputs["queries"][0]["name"] + "-source-field.f32")
            ),
            "raw_depth_sha256": digest(
                field / (inputs["queries"][0]["name"] + "-source-depth.f32")
            ),
        },
        "source_id_namespace": "source ordinal within source-plane-request parent; not an OSM ID",
        "raster_origin": "upper-left; x right, y down; pixel centres at integer + 0.5",
        "colour_encoding": "premultiplied linear HDR RGB; fourth channel coverage = 1-T",
        "capture_camera": camera,
        "viewer": dict(
            query,
            width_px=1280,
            height_px=720,
            near_m=0.1,
            far_m=0.0,
            depth="reverse_z_0_1_float32",
            orthographic=False,
        ),
        "controlled_translations_m": {
            k: [v.tolist() for v in values] for k, values in offsets.items()
        },
        "foreground_forward_interval_m": projected["foreground"],
        "checks": checks,
        "runs": commands,
        "native_array_bytes": sum(v.nbytes for v in arrays.values()),
        "geometry_sources": len(arrays["source_ids"]),
        "geometry_triangles": sum(len(p["indices"]) for p in parts),
        "npz_bytes": (output / "captures.npz").stat().st_size,
        "npz_sha256": digest(output / "captures.npz"),
        "scope": "Controlled translations of complete supported original groups; two unsupported IDs and archived cell33 opacity remain unqualified. Single pixel-centre depth and finite 4x4 microcamera colour samples, not a continuous coverage/depth certificate, geographic neighbour, World admission or performance gate.",
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"checks": checks, "npz_bytes": manifest["npz_bytes"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--native-commit", required=True)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
