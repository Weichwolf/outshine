"""Compare fixed original cells against native flat/depth impostors.

PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings
The 4x4 spatial average uses physical microcamera rotations, not native TAA.
"""

import argparse
import hashlib
import json
import os
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

import numpy as np
from PIL import Image

from building_tree_load import read_tree
from native_building_cell_reference import load_cells

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-full-view"
MODES = (
    "original",
    "flat",
    "depth",
    "flat-blended",
    "depth-blended",
    "flat-moments",
    "depth-moments",
    "clipped-flat-moments",
)


def native_axes(values):
    return np.column_stack((values[:, 0], values[:, 2], -values[:, 1]))


def sampling_error(points, query):
    right, up, forward = np.asarray(query["basis"])
    relative = points - np.asarray(query["eye"])
    focal = query["focal_px"]
    projected = (relative @ np.stack((right, up), axis=1)) * (
        focal / (relative @ forward)
    )[:, None]
    most = 0.0
    for dy in (np.arange(4) + 0.5) / 4 - 0.5:
        for dx in (np.arange(4) + 0.5) / 4 - 0.5:
            sampled_forward = forward + right * dx / focal + up * dy / focal
            sampled_forward /= np.linalg.norm(sampled_forward)
            sampled_right = np.cross(sampled_forward, up)
            sampled_right /= np.linalg.norm(sampled_right)
            sampled_up = np.cross(sampled_right, sampled_forward)
            sampled = (relative @ np.stack((sampled_right, sampled_up), axis=1)) * (
                focal / (relative @ sampled_forward)
            )[:, None]
            most = max(
                most,
                float(np.linalg.norm(sampled - projected + [dx, dy], axis=1).max()),
            )
    return most


def capture_frame_controls(queries, catalogue, centre):
    controls = []
    for query in queries:
        relative = np.asarray(query["eye"]) - centre
        toward = catalogue[np.argmax(catalogue @ relative)]
        right = np.array([toward[2], 0, -toward[0]])
        if np.linalg.norm(right) == 0:
            right = np.array([1.0, 0, 0])
        right /= np.linalg.norm(right)
        aligned = dict(query)
        aligned.update(
            name=query["name"] + "-capture-frame",
            eye=(centre + toward * np.linalg.norm(relative)).tolist(),
            basis=[
                right.tolist(),
                np.cross(toward, right).tolist(),
                (-toward).tolist(),
            ],
            comparison="capture-frame control; existing catalogue unchanged",
        )
        controls.append(aligned)
        query["comparison"] = "held-out original pose"
    return controls


def prepare(parents, output, elevation):
    report = json.loads((parents / "report.json").read_text())
    original, provenance = load_cells(
        DATA / "native-cells/manifest.json",
        DATA / "original-inputs.tar.gz",
        set(report["cells"]),
        read_tree(parents),
    )
    role = original["binding"][:, 4].copy()
    metres = np.any(original["metres"][original["indices"]] != 0, axis=(1, 2))
    role[(role == 0) & metres] = 2
    parts = []
    for index in range(3):
        faces = original["indices"][role == index]
        ids, inverse = np.unique(faces, return_inverse=True)
        directory = output / str(index)
        directory.mkdir()
        attributes = {
            "positions": native_axes(original["positions"][ids]),
            "normals": native_axes(original["vertex_normal"][ids]),
            "uv": original["uv"][ids],
            "uv1": original["metres"][ids],
            "colours": original["colour"][ids],
        }
        for name, values in attributes.items():
            np.asarray(values, dtype="<f4").tofile(directory / (name + ".bin"))
        inverse.astype("<u4").tofile(directory / "indices.bin")
        parts.append({"vertices": len(ids), "triangles": len(faces)})
    queries = report["queries"]
    for query in queries:
        query["eye"] = native_axes(np.array(query["eye"])[None])[0].tolist()
        query["basis"] = native_axes(np.array(query["basis"])).tolist()
        query["sun_elevation_deg"] = elevation
    angles = np.arange(8) * np.pi / 4
    catalogue = np.vstack(
        (
            np.column_stack((np.sin(angles), np.ones(8), np.cos(angles))) / np.sqrt(2),
            [0, 1, 0],
        )
    )
    points = native_axes(original["positions"])
    queries.extend(
        capture_frame_controls(queries, catalogue, (points.min(0) + points.max(0)) / 2)
    )
    for query in queries:
        query["maximum_original_vertex_sampling_offset_error_px"] = sampling_error(
            points, query
        )
    result = {
        "parts": parts,
        "queries": queries,
        "least": points.min(0).tolist(),
        "most": points.max(0).tolist(),
        "catalogue": catalogue.tolist(),
        "source": provenance,
        "sampling": "16 physical microcamera rotations; maximum offset error is measured only over original vertices against ideal 4x4 pixel offsets, not a LOD bound or native TAA result.",
        "scope": "Fixed original geometry, current native facade materials, nine fixed recipe views; no terrain, IBL, movement, source-generation or world-budget admission.",
    }
    (output / "input.json").write_text(json.dumps(result, indent=2) + "\n")


def compare(output):
    report = json.loads((output / "result.json").read_text())
    report["input"] = json.loads((output / "input.json").read_text())
    for row in report["cases"]:
        name = row["query"]
        original = np.fromfile(output / (name + "-original.linear"), "<f4").reshape(
            720, 1280, 4
        )
        reference = (
            np.fromfile(output / (name + "-original.depth"), "<f4").reshape(720, 1280)
            > 0
        )
        y, x = np.nonzero(reference)
        if not len(x):
            raise ValueError("original control contains no visible geometry")
        box = (
            max(0, int(x.min()) - 2),
            max(0, int(y.min()) - 2),
            min(1280, int(x.max()) + 3),
            min(720, int(y.max()) + 3),
        )
        filtered_reference = np.fromfile(
            output / (name + "-original.ss16"), "<f4"
        ).reshape(720, 1280, 4)
        if (
            not np.isfinite(filtered_reference).all()
            or filtered_reference[:, :, 3].sum() <= 0
        ):
            raise ValueError("filtered original contains no valid visible geometry")
        if not np.any(
            (filtered_reference[:, :, 3] > 0) & (filtered_reference[:, :, 3] < 1)
        ):
            raise ValueError(
                "native sampling did not produce fractional reference coverage"
            )
        if filtered_reference[:, :, :3].sum() <= 0 or original[:, :, :3].sum() <= 0:
            raise ValueError(
                "original lighting control contains no positive RGB energy"
            )
        row["reference_pixels"] = int(reference.sum())
        row["crop"] = box
        for mode in MODES:
            prefix = name + "-" + mode
            depth = np.fromfile(output / (prefix + ".depth"), "<f4").reshape(720, 1280)
            colour = np.fromfile(output / (prefix + ".linear"), "<f4").reshape(
                720, 1280, 4
            )
            hit = depth > 0
            row[mode].update(
                gpu_median_ms=statistics.median(row[mode]["gpu_ms"]),
                hit_pixels=int(hit.sum()),
                missed_reference_pixels=int((reference & ~hit).sum()),
                extra_pixels=int((hit & ~reference).sum()),
                relative_rgb_L1=float(
                    np.abs(colour[:, :, :3] - original[:, :, :3]).sum()
                    / max(float(np.abs(original[:, :, :3]).sum()), 1e-30)
                ),
                relative_rgb_energy=float(
                    colour[:, :, :3].sum() / max(float(original[:, :, :3].sum()), 1e-30)
                ),
            )
            filtered = np.fromfile(output / (prefix + ".ss16"), "<f4").reshape(
                720, 1280, 4
            )
            if not np.isfinite(filtered).all() or not np.isfinite(colour).all():
                raise ValueError("native colour contains nonfinite values")
            row[mode].update(
                filtered_relative_rgb_L1=float(
                    np.abs(filtered[:, :, :3] - filtered_reference[:, :, :3]).sum()
                    / max(float(filtered_reference[:, :, :3].sum()), 1e-30)
                ),
                filtered_rgb_energy=float(
                    filtered[:, :, :3].sum()
                    / max(float(filtered_reference[:, :, :3].sum()), 1e-30)
                ),
                filtered_coverage_mass=float(
                    filtered[:, :, 3].sum()
                    / max(float(filtered_reference[:, :, 3].sum()), 1e-30)
                ),
                filtered_missed_coverage=float(
                    np.maximum(filtered_reference[:, :, 3] - filtered[:, :, 3], 0).sum()
                    / max(float(filtered_reference[:, :, 3].sum()), 1e-30)
                ),
                filtered_extra_coverage=float(
                    np.maximum(filtered[:, :, 3] - filtered_reference[:, :, 3], 0).sum()
                    / max(float(filtered_reference[:, :, 3].sum()), 1e-30)
                ),
            )
            np.savez_compressed(
                output / (prefix + "-ss16-crop.npz"),
                rgba=filtered[box[1] : box[3], box[0] : box[2]],
            )
            rgba = np.fromfile(output / (prefix + ".rgba"), np.uint8).reshape(
                720, 1280, 4
            )
            Image.fromarray(rgba).crop(box).save(output / (prefix + "-crop.png"))
            np.savez_compressed(
                output / (prefix + "-crop.npz"),
                depth=depth[box[1] : box[3], box[0] : box[2]],
                linear=colour[box[1] : box[3], box[0] : box[2]],
            )
        print(
            name,
            {
                mode: {
                    k: row[mode][k]
                    for k in ("triangles", "gpu_median_ms", "filtered_relative_rgb_L1")
                }
                for mode in MODES
            },
        )
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")


def run(args):
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        raise ValueError("fresh explicit system-temp output required")
    output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    prepare(args.parents, output, args.sun_elevation)
    command = [str(args.binary.resolve()), str(output)]
    receipt = {
        "command": command,
        "prepare_ms": (time.perf_counter() - started) * 1000,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "experiment_sources": {
            str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path(__file__).resolve(),
                Path(__file__).with_name("native_building_impostors.cpp").resolve(),
            )
        },
        "checkout_dirty": subprocess.check_output(
            ["git", "status", "--porcelain", "--untracked-files=all"],
            cwd=ROOT,
            text=True,
        ),
        "terminal": None,
    }
    env = os.environ.copy()
    if args.audit:
        env["MTL_DEBUG_LAYER"] = "1"
    try:
        with (output / "run.log").open("w") as log:
            result = subprocess.run(
                command,
                cwd=ROOT,
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=180,
                check=False,
            )
        receipt["terminal"] = result.returncode
    except subprocess.TimeoutExpired:
        receipt["terminal"] = "timeout"
        raise
    finally:
        (output / "command.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if result.returncode:
        raise RuntimeError("native probe failed: " + str(output / "run.log"))
    compare(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parents", required=True, type=Path)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sun-elevation", type=float, default=45)
    parser.add_argument("--audit", action="store_true")
    run(parser.parse_args())
