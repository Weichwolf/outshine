"""Project a packed far-world cubemap through an external benchmark camera."""

import argparse
import json
from pathlib import Path
from time import perf_counter

import numpy as np
from PIL import Image

from far_cubemap import reproject, unit


def view_rays(camera, width, height):
    basis = np.asarray(camera["world_from_camera"]).reshape(4, 4).T
    focal = height / (2 * np.tan(np.deg2rad(camera["perspective_fov_deg"]) / 2))
    x = (np.arange(width) + .5 - width / 2) / focal
    y = (height / 2 - np.arange(height) - .5) / focal
    directions = x[None, :, None] * basis[:3, 0] + y[:, None, None] * basis[:3, 1] - basis[:3, 2]
    return unit(directions), focal


def preview(product, report, output, pose, displacement):
    manifest = json.loads((product / "panorama.json").read_text())
    side = manifest["face_px"]
    colour = np.memmap(product / "colour.rgba16f", mode="r", dtype="<f2", shape=(6, side, side, 4))
    depth = np.memmap(product / "depth.r32f", mode="r", dtype="<f4", shape=(6, side, side))
    camera = report["motion"][pose]
    width, height = (report["profile"][axis] for axis in ("width_px", "height_px"))
    rays, focal = view_rays(camera, width, height)
    delta = np.asarray(camera["position_m"]) - np.asarray(manifest["anchor_eus_m"]) + displacement
    began = perf_counter()
    index, distance, resolved = reproject(depth, rays, delta, focal)
    elapsed_ms = (perf_counter() - began) * 1000
    rgb = colour[index][..., :3].astype(np.float32)
    exposure = next((value["value"] for value in report["metrics"]
                     if value["name"] == "the exposure the picture applied"), None)
    if exposure is None or not np.isfinite(exposure) or exposure <= 0:
        raise ValueError("the comparison camera must report a positive exposure")
    lit = rgb * exposure
    mapped = np.clip(lit * (2.51 * lit + .03) /
                     (lit * (2.43 * lit + .59) + .14), 0, 1)
    display = np.where(mapped <= .0031308, 12.92 * mapped, 1.055 * mapped**(1 / 2.4) - .055)
    Image.fromarray((np.clip(display, 0, 1) * 255 + .5).astype(np.uint8)).save(output)
    print(json.dumps({
        "view_px": [width, height],
        "eye_offset_m": float(np.linalg.norm(delta)),
        "opaque_pixels": int(np.count_nonzero(distance)),
        "unresolved_pixels": int(np.count_nonzero(~resolved)),
        "cpu_reference_ms": elapsed_ms,
    }))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("product", type=Path)
    parser.add_argument("benchmark", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--pose", choices=("start", "finish"), default="start")
    parser.add_argument("--eye-delta", type=float, nargs=3, default=(0, 0, 0))
    options = parser.parse_args()
    report = json.loads(options.benchmark.read_text())
    preview(options.product, report, options.output, options.pose, np.array(options.eye_delta))


if __name__ == "__main__":
    main()
