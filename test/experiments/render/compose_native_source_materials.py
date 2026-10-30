"""Compose actual source material readbacks with bound native World opponents."""

import argparse
import hashlib
import io
import json
from pathlib import Path
from types import SimpleNamespace
from zipfile import ZipFile

import numpy as np
from compare_native_world_fields import composition
from experiment_output import create_output
from field_composition import opaque_points
from native_source_triangle_materials import DATA
from native_world_inputs import POSES, load, read_bounded


def source_points(samples, pose, mode, crop):
    low, high = np.asarray(crop[:2]), np.asarray(crop[2:])
    xy = samples[pose + "/pixel_xy"]
    if (
        xy.ndim != 2
        or xy.shape[1] != 2
        or xy.dtype.kind not in "iu"
        or np.any(xy < low)
        or np.any(xy >= high)
        or len(np.unique(xy, axis=0)) != len(xy)
        or np.any(high <= low)
        or np.prod(high - low) > 262144
    ):
        raise ValueError("Unique bounded native point coordinates required")
    colour, depth = (samples[pose + "/" + mode + "_" + c] for c in ("rgba", "depth"))
    if (
        colour.shape != (len(xy), 4)
        or depth.shape != (len(xy),)
        or colour.dtype != np.dtype("<f4")
        or depth.dtype != np.dtype("<f4")
    ):
        raise ValueError("Native colour/depth sample inventory differs")
    shape = (int(high[1] - low[1]), int(high[0] - low[0]))
    rgba, reverse_z = np.zeros((*shape, 4), "<f4"), np.zeros(shape, "<f4")
    x, y = (xy - low).T
    rgba[y, x], reverse_z[y, x] = colour, depth
    opaque_points(rgba, 262144)
    if (
        not np.isfinite(reverse_z).all()
        or np.any((reverse_z < 0) | (reverse_z > 1))
        or not np.array_equal(rgba[:, :, 3], reverse_z > 0)
    ):
        raise ValueError("Opaque native coverage must match reverse-Z depth")
    return SimpleNamespace(
        rgba=rgba, reverse_z=reverse_z, evaluated=np.ones(shape, dtype=bool)
    )


def bound_source(samples, report, arrays, cases, pose, crop):
    for mode in ("whole", "selected"):
        actual = report["native_reports"][mode]["cases"]
        matches = [case for case in actual if case["query"] == pose]
        if len(matches) != 1 or any(
            matches[0][key] != cases["original", pose][key]
            for key in ("capture", "lighting_camera", "lens_operands")
        ):
            raise ValueError("Native query/lighting operands differ from World")
    whole = source_points(samples, pose, "whole", crop)
    if not np.array_equal(whole.rgba, arrays["original/" + pose + "/rgba"]) or not (
        np.array_equal(whole.reverse_z, arrays["original/" + pose + "/depth"])
    ):
        raise ValueError("Whole native source differs from bound World input")
    return source_points(samples, pose, "selected", crop)


def run(native, directory, output):
    manifest, arrays, cases, _ = load(directory, attributes=True)
    report_bytes = read_bounded(native / "report.json")
    native_report = json.loads(report_bytes)
    payload = read_bounded(native / "samples.npz", 8 * 1024**2)
    with ZipFile(io.BytesIO(payload)) as archive:
        if sum(item.file_size for item in archive.infolist()) > 8 * 1024**2:
            raise ValueError("Bounded sparse native samples required")
    create_output(output)
    result = {}
    with np.load(io.BytesIO(payload), allow_pickle=False) as samples:
        for pose in POSES:
            crop = manifest["crops_xyxy"][pose]
            model = bound_source(samples, native_report, arrays, cases, pose, crop)
            result[pose] = composition(
                arrays, pose, model, crop, manifest["sample_axis"], output, 0.25, 0.125
            )
    report = {
        "native_selection": native_report["selection"],
        "native_binary_sha256": native_report["binary_sha256"],
        "input_sha256": {
            "native_report": hashlib.sha256(report_bytes).hexdigest(),
            "native_samples": hashlib.sha256(payload).hexdigest(),
            "world_manifest": hashlib.sha256(
                (directory / "manifest.json").read_bytes()
            ).hexdigest(),
        },
        "sample_axis": manifest["sample_axis"],
        "limits": {"rgb_max": 0.25, "visible_source_coverage_max": 0.125},
        "poses": result,
        "scope": "Finite native source material points against terrain/opening opponents; common rays depth-ordered before filtering. Runtime World/pose/light admission and performance remain unqualified.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"selection": report["native_selection"], "poses": result}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--input", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.native, args.input, args.output)
