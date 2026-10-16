"""Project cached first-exchange fields through the actual unchanged triangle mesh."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from exchange_basis import finite_mesh, source_nodes
from exchange_io import read_exchange
from PIL import Image
from scatter_render import camera_packet
from scatter_scene import ScatterScene


def project(scene, basis, fields, width, height, samples=4, packet=8192):
    count = width * height
    output = {name: np.zeros((count, 3), np.float32) for name in fields}
    view = np.array((np.sqrt(3) / 2, 0, 0.5))
    for first in range(0, count, packet // samples):
        n = min(packet // samples, count - first)
        origins, directions = camera_packet(view, width, height, samples, first, n, 73)
        ids, points, normals, _ = scene.nearest(origins, directions, 10)
        selected = np.flatnonzero(ids >= 0)
        nodes = source_nodes(ids[selected], points[selected], basis)
        side = normals[selected] @ view < 0
        for name, values in fields.items():
            paths = np.zeros_like(origins)
            paths[selected] = values[2 * nodes + side]
            output[name][first : first + n] = paths.reshape(n, samples, 3).mean(1)
    return {name: a.reshape(height, width, 3) for name, a in output.items()}


def block_mean(image):
    height, width = image.shape[:2]
    ys, xs = np.arange(0, height, 16), np.arange(0, width, 16)
    areas = np.minimum(16, height - ys)[:, None] * np.minimum(16, width - xs)[None, :]
    sums = np.add.reduceat(np.add.reduceat(image.astype(float), ys, axis=0), xs, axis=1)
    return sums / areas[:, :, None], areas


def assess(candidate, reference):
    diff = candidate.astype(float) - reference.astype(float)
    block, areas = block_mean(diff)
    mean = diff.mean((0, 1))
    error = float(np.sum(abs(block) * areas[:, :, None]) / (areas.sum() * 3))
    return {
        "mean_rgb_difference": mean.tolist(),
        "block16_mean_abs_rgb_error": error,
        "mean_limit": 0.0005,
        "block_limit": 0.0015,
        "pass": bool(np.max(abs(mean)) <= 0.0005 and error <= 0.0015),
    }


def write_rgb(path, rgb):
    srgb = np.where(
        rgb <= 0.0031308, 12.92 * rgb, 1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055
    )
    Image.fromarray(np.rint(np.clip(srgb, 0, 1) * 255).astype(np.uint8)).save(path)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(inputs, prepared, reference, output):
    if output.exists():
        raise ValueError("fresh exchange projection output required")
    began = time.perf_counter()
    report, vertices, faces, identity = finite_mesh(inputs)
    basis, links = read_exchange(prepared / "product", identity)
    del links
    build = json.loads((prepared / "result.json").read_text())
    ref = json.loads((reference / "result.json").read_text())
    if (
        basis["identity"] != build["product_identity"]
        or ref["stand_identity"] != report["stand_identity"]
        or ref["resolution"] != [1920, 1080]
        or ref["samples"] != 4
    ):
        raise ValueError(
            "matching complete canonical four-sample ray reference required"
        )
    with np.load(prepared / "frame-diagnostics.npz", allow_pickle=False) as f:
        fields = {name: f[name + "_first"] for name in ("front", "back")}
    scene = ScatterScene(vertices, faces)
    setup = time.perf_counter() - began
    began = time.perf_counter()
    arrays = project(scene, basis, fields, 1920, 1080)
    projection = time.perf_counter() - began
    output.mkdir(parents=True)
    checks = {}
    images = {}
    for name, indirect in arrays.items():
        with np.load(reference / f"{name}.npz", allow_pickle=False) as f:
            direct, original = f["direct"], f["indirect"]
        checks[name] = assess(indirect, original)
        np.savez_compressed(output / f"{name}.npz", indirect=indirect)
        images[name] = write_rgb(output / f"{name}-candidate.png", direct + indirect)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "product_identity": basis["identity"],
        "source_geometry_sha256": identity,
        "reference_commit": ref["commit"],
        "reference_report_sha256": hashlib.sha256(
            (reference / "result.json").read_bytes()
        ).hexdigest(),
        "setup_load_s": setup,
        "projection_s": projection,
        "nearest_work": scene.work,
        "checks": checks,
        "images": images,
        "all_pass": all(v["pass"] for v in checks.values()),
        "scope": "First-neighbour approximation only; unchanged exact direct image and primary sample positions. Complete mesh projection, fixed predeclared mean/block criteria. No new lighting baked into the geometric asset.",
    }
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "prepared", "reference", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run(a.inputs, a.prepared, a.reference, a.output)
