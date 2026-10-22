"""Generate/save/reload a complete coarse city before diagnostic geometry work."""

import argparse
import hashlib
import json
import math
import subprocess
import time
from pathlib import Path

import numpy as np
from coarse_city_domains import load_snapshot
from urban_patch_bake import bake
from urban_patch_mesh import build
from urban_patch_trace import trace
from urban_patches import load


def camera_cases(target):
    result = {}
    for name, offset in {
        "front": [-500, -800, 130],
        "reverse": [650, 750, 160],
        "elevated": [250, -700, 500],
        "moving": [-492, -800, 130],
    }.items():
        eye = target + offset
        forward = target - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, [0, 0, 1])
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
        result[name] = {
            "eye": eye.tolist(),
            "camera_basis": np.array([right, up, forward]).tolist(),
            "width": 1280,
            "height": 720,
            "focal_px": 720 / (2 * math.tan(math.radians(22.5))),
        }
    return result


def image_rays(camera):
    right, up, forward = np.array(camera["camera_basis"])
    x, y = np.meshgrid(np.linspace(0.5, 1279.5, 64), np.linspace(0.5, 719.5, 36))
    directions = forward + (x.ravel() - 640)[:, None] / camera["focal_px"] * right
    directions -= (y.ravel() - 360)[:, None] / camera["focal_px"] * up
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    return np.broadcast_to(camera["eye"], directions.shape).copy(), directions


def verify_views(product, chunks, cameras):
    reports = {}
    for name, camera in cameras.items():
        origins, directions = image_rays(camera)
        rows = {}
        results = []
        for mode in ["boxes", "masks"]:
            started = time.perf_counter()
            result = trace(product, origins, directions, mode=mode, chunks=chunks)
            rows[mode] = {
                "query_seconds": time.perf_counter() - started,
                "work": result[-1],
            }
            results.append(result)
        for a, b in zip(results[0][:4], results[1][:4], strict=True):
            np.testing.assert_array_equal(a, b, err_msg=name)
        reports[name] = {
            "rays": len(origins),
            "hits": int(np.isfinite(results[0][0]).sum()),
            "exact_depth_owner_face_normal": True,
            **rows,
        }
    return reports


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    producer = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    started = time.perf_counter()
    document = load_snapshot(args.input)
    snapshot_seconds = time.perf_counter() - started
    started = time.perf_counter()
    product, manifest = bake(document)
    bake_seconds = time.perf_counter() - started
    np.savez_compressed(args.output / "product.npz", **product.arrays())
    started = time.perf_counter()
    product = load(args.output / "product.npz")
    load_seconds = time.perf_counter() - started
    manifest.update(
        {
            "producer": producer,
            "snapshot_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
            "snapshot_seconds": snapshot_seconds,
            "bake_seconds": bake_seconds,
            "ready_load_seconds": load_seconds,
            "product_arrays": {
                k: {"bytes": a.nbytes, "dtype": str(a.dtype), "shape": a.shape}
                for k, a in product.arrays().items()
            },
        }
    )
    centres = np.array(
        [
            product.origin(i) + product.side * product.pitch / 2
            for i in range(len(product.chunks))
        ]
    )
    if not len(centres):
        raise ValueError(
            "no inferred geometry for a lit study; evidence is not World Empty"
        )
    anchor = np.median(centres, axis=0)
    chunks = np.argsort(np.linalg.norm(centres - anchor, axis=1), kind="stable")[:64]
    target = np.array([*centres[chunks].mean(axis=0), 12.0])
    cameras = camera_cases(target)
    point_reports = verify_views(product, chunks, cameras)
    for mode in ["boxes", "masks"]:
        arrays = build(product, chunks, mode)
        arrays["positions"] -= np.array([*target[:2], 0])
        np.savez_compressed(args.output / (mode + ".npz"), **arrays)
        for name, camera in cameras.items():
            row = dict(camera)
            row["eye"] = (np.array(camera["eye"]) - [*target[:2], 0]).tolist()
            row.update(
                {
                    "producer": producer,
                    "mode": mode,
                    "mesh": "../" + mode + ".npz",
                    "product": "../product.npz",
                    "chunks": chunks.tolist(),
                    "ground_span": 12000,
                    "study": "Bounded flat procedural coarse-city control, not observed detailed buildings/world admission.",
                }
            )
            folder = args.output / name
            folder.mkdir(exist_ok=True)
            (folder / (mode + ".json")).write_text(json.dumps(row, indent=2) + "\n")
    manifest["diagnostic"] = {
        "chunks": chunks.tolist(),
        "own_procedural_lots": int((product.tiers[chunks] > 0).sum()),
        "camera_count": len(cameras),
        "point_reports": point_reports,
        "scope": "Geometry diagnostics created only after cache save/reload. Cameras select the same bounded subregion for both modes; all-source product bake is separate. Point samples do not qualify filtering/opacity/GPU/casters.",
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: manifest[k]
                for k in [
                    "occupied_lots",
                    "patches",
                    "bake_seconds",
                    "ready_load_seconds",
                ]
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
