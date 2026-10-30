"""Export whole original opponents on separate capture and perspective rasters."""

import argparse
import copy
import hashlib
import json
import math
import platform
import subprocess
from pathlib import Path

import numpy as np
from building_sample_originals import read_original
from experiment_output import create_output
from export_native_field_opponents import (
    CONTROLS,
    DATA,
    ROOT,
    digest,
    document,
    execute,
    pack,
    prepare,
    read_samples,
)
from field_pose_geometry import projection_bound
from field_view_projection import CapturePlane

LAYERS = ("original", "terrain", "external", "union-terrain", "union-external")


def world_cameras(controls):
    capture = controls["capture"]
    centre = np.asarray(capture["eye_m"]) + np.asarray(capture["forward"]) * (
        (capture["near_m"] + capture["far_m"]) / 2
    )
    result = {}
    for name, degrees in (("near-direction", 0.25), ("revealing-side", 35)):
        angle = math.radians(degrees)
        c, s = math.cos(angle), math.sin(angle)
        rotate = np.array(((c, 0, s), (0, 1, 0), (-s, 0, c)))
        axes = [rotate @ np.asarray(capture[k]) for k in ("right", "up", "forward")]
        result[name] = {
            "eye_m": (centre - axes[2] * 32000).tolist(),
            **dict(
                zip(("right", "up", "forward"), (a.tolist() for a in axes), strict=True)
            ),
            "orthographic": False,
            "width_px": 1280,
            "height_px": 720,
            "yfov_rad": 2 * math.atan(0.5),
            "near_m": 0.1,
            "far_m": 240000.0,
            "x_mag_m": 0.0,
            "y_mag_m": 0.0,
        }
    return result


def queries(cameras, capture=None):
    result = [
        {
            "name": name,
            "capture": capture or camera,
            "field_lighting": "viewer",
            "lighting_camera": {"capture": camera},
            "sun_elevation_deg": 45,
        }
        for name, camera in cameras.items()
    ]
    result.append({**copy.deepcopy(result[0]), "name": "repeat"})
    return result


def native_export(binary, root, geometry, requests):
    arrays, reports, executions = {}, {}, {}
    for layer, request in requests.items():
        folder = root / layer
        declaration = {"directory": str(geometry / layer), "queries": request}
        if layer == "capture":
            declaration["directory"] = str(geometry / "original")
        reports[layer] = execute(binary, folder, declaration)
        executions[layer] = {
            "input": document(folder / "input.json"),
            "input_sha256": digest(folder / "input.json"),
            "command": document(folder / "command.json"),
            "exit": int((folder / "terminal.exit").read_text()),
            "report_sha256": digest(folder / "material-fields-result.json"),
            "buffers": {},
        }
        for query, case in zip(request, reports[layer]["cases"], strict=True):
            for k, v in query["capture"].items():
                if case["capture"][k] != v:
                    raise ValueError("effective native camera differs: " + k)
            if (
                case["lighting_camera"]["eye_m"]
                != query["lighting_camera"]["capture"]["eye_m"]
            ):
                raise ValueError("effective PBR viewer differs")
            name = query["name"]
            colour, depth = read_samples(folder, name, case["capture"])
            for channel, value in (("rgba", colour), ("depth", depth)):
                key = layer + "/" + name + "/" + channel
                arrays[key] = value
                executions[layer]["buffers"][key] = hashlib.sha256(
                    value.tobytes()
                ).hexdigest()
    return arrays, reports, executions


def common_crop(arrays, pose, margin=8):
    covered = np.logical_or.reduce(
        [arrays[layer + "/" + pose + "/depth"] > 0 for layer in LAYERS]
    )
    y, x = np.nonzero(covered)
    if not len(x):
        raise ValueError("whole world opponent is outside its raster")
    return [
        max(0, int(x.min()) - margin),
        max(0, int(y.min()) - margin),
        min(1280, int(x.max()) + margin + 1),
        min(720, int(y.max()) + margin + 1),
    ]


def comparison(arrays, pose, opponent):
    source = arrays["original/" + pose + "/rgba"]
    z = arrays["original/" + pose + "/depth"]
    outer = arrays[opponent + "/" + pose + "/rgba"]
    oz = arrays[opponent + "/" + pose + "/depth"]
    union = arrays["union-" + opponent + "/" + pose + "/rgba"]
    uz = arrays["union-" + opponent + "/" + pose + "/depth"]
    both = (z > 0) & (oz > 0)
    colour = np.where((z > oz)[..., None], source, outer)
    return {
        "source_pixels": int(np.count_nonzero(z)),
        "outer_pixels": int(np.count_nonzero(oz)),
        "source_first": int(np.count_nonzero(both & (z > oz))),
        "outer_first": int(np.count_nonzero(both & (z < oz))),
        "ties": int(np.count_nonzero(both & (z == oz))),
        "depth_equal": bool(np.array_equal(np.maximum(z, oz), uz)),
        "coverage_equal": bool(np.array_equal(colour[..., 3], union[..., 3])),
        "rgb_max_abs": float(np.abs(colour[..., :3] - union[..., :3]).max()),
    }


def crop_and_check(arrays, cameras):
    crops, checks, failures = {}, {}, []
    for pose in (*cameras, "repeat"):
        crops[pose] = common_crop(arrays, pose)
        checks[pose] = {k: comparison(arrays, pose, k) for k in ("terrain", "external")}
        for opponent, record in checks[pose].items():
            if not record["depth_equal"] or not record["coverage_equal"]:
                failures.append(
                    pose + "/" + opponent + ": union depth/coverage differs"
                )
            if record["rgb_max_abs"] != 0:
                failures.append(pose + "/" + opponent + ": union colour differs")
        x0, y0, x1, y1 = crops[pose]
        for layer in LAYERS:
            for channel in ("rgba", "depth"):
                key = layer + "/" + pose + "/" + channel
                whole = arrays[key]
                cropped = whole[y0:y1, x0:x1].copy()
                restored = np.zeros_like(whole)
                restored[y0:y1, x0:x1] = cropped
                if not np.array_equal(restored, whole):
                    raise ValueError("crop discards nonzero native data")
                arrays[key] = cropped
    for layer in ("capture", *LAYERS):
        for channel in ("rgba", "depth"):
            a = arrays[layer + "/near-direction/" + channel]
            b = arrays[layer + "/repeat/" + channel]
            if not np.array_equal(a, b):
                failures.append(layer + ": identical repeat changed " + channel)
    return crops, checks, failures


def geometric_checks(capture, reports):
    original, _ = read_original(DATA / "original-geometry")
    centre = np.asarray(capture["eye_m"]) + np.asarray(capture["forward"]) * (
        (capture["near_m"] + capture["far_m"]) / 2
    )
    plane = CapturePlane(
        original["positions"].min(0),
        original["positions"].max(0),
        centre,
        -np.asarray(capture["forward"]),
    )
    result = {}
    for case in reports["original"]["cases"]:
        camera = case["capture"]
        matrix = np.asarray(
            case["lens_operands"]["projection_f32_column_major"]
        ).reshape(4, 4, order="F")
        matrix[:, 2] *= -1
        result[case["query"]] = projection_bound(
            plane,
            camera["eye_m"],
            [camera[k] for k in ("right", "up", "forward")],
            matrix,
            (1280, 720),
        )
    return result


def export(args):
    root = args.output.resolve()
    create_output(root)
    geometry = root / "geometry-inputs"
    geometry.mkdir()
    controls = document(CONTROLS)
    provenance, _ = prepare(geometry, controls)
    cameras = world_cameras(controls)
    capture = {
        **controls["capture"],
        "width_px": 128,
        "height_px": 128,
        "bytes": 128 * 128 * 4,
    }
    requests = {"capture": queries(cameras, capture)}
    requests.update({layer: queries(cameras) for layer in LAYERS})
    arrays, reports, executions = native_export(args.binary, root, geometry, requests)
    crops, checks, failures = crop_and_check(arrays, cameras)
    original = document(DATA / "original-geometry/manifest.json")
    for name, record in original["files"].items():
        if digest(geometry / "original/geometry/0" / name) != record["sha256"]:
            raise ValueError("whole original operand changed during execution")
    records = pack(root, arrays)
    manifest = {
        "version": 1,
        "base_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "source_binding": "base commit plus explicit modified native driver hash",
        "system": platform.platform(),
        "binary_sha256": digest(args.binary),
        "driver_sha256": digest(Path(__file__)),
        "native_driver_sha256": digest(
            ROOT / "test/experiments/render/native_building_impostors.cpp"
        ),
        "controls_sha256": digest(CONTROLS),
        "original_geometry": provenance,
        "capture": capture,
        "world_cameras": cameras,
        "crops_xyxy": crops,
        "raster": [1280, 720],
        "sampling": {
            "origin": "top-left",
            "pixel_centres": [0.5, 0.5],
            "depth": "raw reverse-Z Depth32Float [0,1], clear zero",
            "colour": "unfiltered premultiplied linear float32, clear zero",
            "samples_per_pixel": 1,
            "jitter_px": [0, 0],
            "crop_reconstruction": "zero-fill full raster then place crop at xyxy",
        },
        "runtime_world_from_field": None,
        "runtime_world_revision": None,
        "scope": "native original references, not normal World field admission or GPU uniform readback",
        "executions": executions,
        "native_reports": reports,
        "comparisons": checks,
        "python_geometric_displacement_only": geometric_checks(capture, reports),
        "reference_failures": failures,
        "arrays": records,
        "npz_sha256": digest(root / "samples.npz"),
    }
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        json.dumps({"output": str(root), "failures": failures, "comparisons": checks})
    )
    return bool(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.binary = args.binary.resolve()
    return export(args)


if __name__ == "__main__":
    raise SystemExit(main())
