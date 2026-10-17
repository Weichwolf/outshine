"""Source-loaded near-leaf R/T portraits with the existing primary/ray procedures."""

import argparse
import hashlib
import io
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response
from PIL import Image
from scatter_render import camera_packet
from scatter_scene import ScatterScene
from sheet_light_measure import contact_optics, sheet_visibility
from surface_normal_transfer import interpolate_normals


def read_source(inputs):
    identity = json.loads((inputs / "source.json").read_text())
    if (
        identity.get("schema") != "selected-thin-leaves-1"
        or identity.get("surface_model") != "thin-sheet"
    ):
        raise ValueError("explicit prepared thin-leaf source required")
    path = inputs / "source.npz"
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("bounded selected-leaf source required before decode")
    encoded = path.read_bytes()
    if hashlib.sha256(encoded).hexdigest() != identity["source_sha256"]:
        raise ValueError("prepared leaf source bytes do not match their descriptor")
    with np.load(io.BytesIO(encoded)) as saved:
        source = {key: saved[key] for key in saved.files}
    source["surface_model"] = identity["surface_model"]
    return source, identity


def shading(source, scene, points, face_ids, view, lights):
    normals = interpolate_normals(source, points, face_ids)
    optical = contact_optics(source, points, face_ids)
    rgb, reflected = (np.zeros((len(lights), len(points), 3)) for _ in range(2))
    for index, direction in enumerate(lights):
        response = leaf_diffuse_response(optical, normals, view, direction)
        visible, _, _ = sheet_visibility(
            scene, points, normals, view, direction, response, 2e-6
        )
        rgb[index] = response * visible[:, None] * [3.0, 2.7, 2.4]
        front = np.where(normals @ view < 0, -1, 1) * (normals @ direction) >= 0
        reflected[index] = rgb[index] * front[:, None]
    return rgb, reflected


def render(source, scene, width, height, samples):
    if (
        (width, height) != (2560, 1440)
        or type(samples) is not int
        or samples not in (1, 2)
    ):
        raise ValueError("fixed2560x1440 and one/two actual camera samples required")
    view = np.array([0.2, -0.6, 0.77])
    view /= np.linalg.norm(view)
    front = np.array([0.2, -0.3, 1.0])
    front /= np.linalg.norm(front)
    lights = np.stack((front, -front))
    centre = (source["vertices"].min(axis=0) + source["vertices"].max(axis=0)) / 2
    radius = np.linalg.norm(source["vertices"] - centre, axis=1).max()
    span = 2.1 * radius * width / height
    distance = scene.index.scale + 1
    count, packet_pixels = width * height, 8192 // samples
    rgb, reflected = (np.zeros((2, count, 3), np.float32) for _ in range(2))
    coverage = np.zeros(count, np.float32)
    work = {
        "primary_rays": count * samples,
        "visible_contacts": 0,
        "light_contact_products": 0,
    }
    began = time.perf_counter()
    for first in range(0, count, packet_pixels):
        pixels = min(packet_pixels, count - first)
        origins, directions = camera_packet(
            view, width, height, samples, first, pixels, 643
        )
        origins = (origins - 4 * view) * (span / 1.4) + centre + distance * view
        ids, points, _, _ = scene.nearest(origins, directions, 2 * distance)
        active = ids >= 0
        work["visible_contacts"] += int(active.sum())
        work["light_contact_products"] += int(active.sum()) * len(lights)
        values, reflected_values = (np.zeros((2, len(ids), 3)) for _ in range(2))
        if active.any():
            values[:, active], reflected_values[:, active] = shading(
                source, scene, points[active], ids[active], view, lights
            )
        rgb[:, first : first + pixels] = values.reshape(2, pixels, samples, 3).mean(
            axis=2
        )
        reflected[:, first : first + pixels] = reflected_values.reshape(
            2, pixels, samples, 3
        ).mean(axis=2)
        coverage[first : first + pixels] = active.reshape(pixels, samples).mean(axis=1)
        if first == 0 or first + pixels == count or first % (packet_pixels * 100) == 0:
            print(
                json.dumps(
                    {
                        "pixels": first + pixels,
                        "total": count,
                        "seconds": time.perf_counter() - began,
                    }
                ),
                flush=True,
            )
    return {
        "rgb": rgb.reshape(2, height, width, 3),
        "reflection_only": reflected.reshape(2, height, width, 3),
        "coverage": coverage.reshape(height, width),
    }, work | {
        "view": view.tolist(),
        "lights": lights.tolist(),
        "camera_sphere_radius_m": float(radius),
        "horizontal_span_m": float(span),
    }


def run(inputs, output, samples):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private render output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source, identity = read_source(inputs)
    scene = ScatterScene(source["vertices"], source["faces"])
    began = time.perf_counter()
    arrays, work = render(source, scene, 2560, 1440, samples)
    np.savez_compressed(output / "linear.npz", **arrays)
    images = {}
    for state, name in enumerate(("front", "back")):
        for field in ("rgb", "reflection_only"):
            linear = arrays[field][state]
            srgb = np.where(
                linear <= 0.0031308,
                12.92 * linear,
                1.055 * np.maximum(linear, 0) ** (1 / 2.4) - 0.055,
            )
            path = output / f"{name}-{field}.png"
            Image.fromarray(np.rint(np.clip(srgb, 0, 1) * 255).astype(np.uint8)).save(
                path
            )
            images[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    (output / "report.json").write_text(
        json.dumps(
            {
                "source": identity,
                "source_sha256": hashlib.sha256(
                    (inputs / "source.npz").read_bytes()
                ).hexdigest(),
                "commit": subprocess.check_output(
                    ["git", "rev-parse", "HEAD"], text=True
                ).strip(),
                "resolution": [2560, 1440],
                "samples": samples,
                "work": work,
                "rays": scene.work,
                "render_and_export_s": time.perf_counter() - began,
                "process_peak_rss_bytes_linux": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss
                * 1024,
                "controlled_output_bytes": sum(a.nbytes for a in arrays.values()),
                "images": images,
                "scope": "existing primary camera/ray kernels, prepared original six leaves, local diffuse R/T with opaque neighbour visibility; no indirect/BSSRDF/ballistic-through-leaf/specular/closed-needle or device acceptance",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, choices=(1, 2), default=2)
    args = parser.parse_args()
    run(args.inputs, args.output, args.samples)
