"""Full finite-geometry CPU ray images: direct light and one diffuse neighbour."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from diffuse_bounce import direct_states, neighbour_states, surface_optics
from diffuse_scatter import uniform_samples
from grass_light_experiment import light_states
from grass_projection_reader import read_product
from PIL import Image
from scatter_scene import ScatterScene


def camera_packet(view, width, height, samples, first, pixels, seed):
    random = uniform_samples(seed, first * samples, pixels * samples)
    ids = np.repeat(np.arange(first, first + pixels), samples)
    x = ((ids % width + random[:, 0]) / width - 0.5) * 1.4
    y = (0.5 - (ids // width + random[:, 1]) / height) * (1.4 * height / width)
    right = np.cross((0, 0, 1.0), view)
    right = (
        right / np.linalg.norm(right)
        if np.linalg.norm(right) > 0
        else np.array((0, 1, 0.0))
    )
    up = np.cross(view, right)
    origins = 4 * view + x[:, None] * right + y[:, None] * up
    return origins, np.broadcast_to(-view, origins.shape)


def evaluate_packet(scene, states, leaf_faces, view, origins, directions, random, bias):
    ids, points, normals, _ = scene.nearest(origins, directions, 10)
    chosen = np.flatnonzero(ids >= 0)
    output = {name: [np.zeros_like(origins), np.zeros_like(origins)] for name in states}
    if not len(chosen):
        return output
    points, normals = points[chosen], normals[chosen]
    optical = surface_optics(ids[chosen], leaf_faces, next(iter(states.values())))
    direct = direct_states(scene, points, normals, view, optical, states, bias)
    indirect = neighbour_states(
        scene, points, normals, view, optical, random[chosen], states, leaf_faces, bias
    )
    for name in states:
        output[name][0][chosen], output[name][1][chosen] = direct[name], indirect[name]
    return output


def render_arrays(
    scene,
    states,
    leaf_faces,
    view,
    width,
    height,
    samples,
    seed,
    packet=8192,
    bias=2e-6,
):
    if (
        type(width) is not int
        or type(height) is not int
        or min(width, height) < 1
        or width * height > 8_388_608
        or type(samples) is not int
        or not 1 <= samples <= 16
        or width * height * samples > 33_554_432
        or type(packet) is not int
        or not samples <= packet <= 65536
    ):
        raise ValueError(
            "bounded image, 1..16 path samples and explicit packet capacity required"
        )
    count = width * height
    output = {
        name: {
            key: np.zeros((count, 3), np.float32)
            for key in ("direct", "indirect", "indirect_standard_error")
        }
        for name in states
    }
    pixels = packet // samples
    began = time.perf_counter()
    next_progress = 0
    for first in range(0, count, pixels):
        n = min(pixels, count - first)
        origins, directions = camera_packet(
            view, width, height, samples, first, n, seed
        )
        random = uniform_samples(seed + 1, first * samples, n * samples)
        values = evaluate_packet(
            scene, states, leaf_faces, view, origins, directions, random, bias
        )
        for name, (direct, indirect) in values.items():
            direct, indirect = (
                direct.reshape(n, samples, 3),
                indirect.reshape(n, samples, 3),
            )
            output[name]["direct"][first : first + n] = direct.mean(1)
            output[name]["indirect"][first : first + n] = indirect.mean(1)
            output[name]["indirect_standard_error"][first : first + n] = (
                np.sqrt(indirect.var(1, ddof=1) / samples) if samples > 1 else np.nan
            )
        if first + n >= next_progress:
            print(
                json.dumps(
                    {
                        "pixels": first + n,
                        "total_pixels": count,
                        "wall_s": time.perf_counter() - began,
                    }
                ),
                flush=True,
            )
            next_progress = first + n + max(count // 10, 1)
    return {
        name: {k: a.reshape(height, width, 3) for k, a in fields.items()}
        for name, fields in output.items()
    }


def write_images(output, fields):
    images = []
    for name, arrays in fields.items():
        np.savez_compressed(output / f"{name}.npz", **arrays)
        for label, rgb in (
            ("direct", arrays["direct"]),
            ("one-neighbour", arrays["direct"] + arrays["indirect"]),
        ):
            srgb = np.where(
                rgb <= 0.0031308,
                12.92 * rgb,
                1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055,
            )
            target = output / f"{name}-{label}.png"
            Image.fromarray(np.rint(np.clip(srgb, 0, 1) * 255).astype(np.uint8)).save(
                target
            )
            images.append(
                {
                    "name": target.name,
                    "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
                }
            )
    return images


def run(inputs, output, width, height, samples, seed, packet, shadow_backend="cached"):
    if output.exists():
        raise ValueError("fresh finite-scattering output directory required")
    began = time.perf_counter()
    report, stand, _ = read_product(inputs)
    ground = np.array(((-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)), float)
    vertices = np.concatenate((stand["vertices"], ground))
    faces = np.concatenate(
        (stand["faces"], np.array(((0, 1, 2), (0, 2, 3))) + len(stand["vertices"]))
    )
    scene = ScatterScene(vertices, faces, shadow_backend)
    control = json.loads(
        (Path(__file__).parent / "grass_light_control.json").read_text()
    )
    states = light_states(control)
    view = np.array((np.sqrt(3) / 2, 0, 0.5))
    depths = 4 - vertices @ view
    if depths.min() <= 0.1 or depths.max() >= 10:
        raise ValueError("complete finite geometry must fit study camera depth range")
    arrays = render_arrays(
        scene, states, len(stand["faces"]), view, width, height, samples, seed, packet
    )
    output.mkdir(parents=True)
    images = write_images(output, arrays)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_inputs": str(inputs.resolve()),
        "source_report_sha256": hashlib.sha256(
            (inputs / "result.json").read_bytes()
        ).hexdigest(),
        "stand_identity": report["stand_identity"],
        "vertices": len(vertices),
        "triangles": len(faces),
        "camera_mesh_depth_m": [float(depths.min()), float(depths.max())],
        "resolution": [width, height],
        "samples": samples,
        "seed": seed,
        "packet": packet,
        "shadow_backend": shadow_backend,
        "bias_m": 2e-6,
        "view": view.tolist(),
        "states": states,
        "nearest_work": scene.work,
        "direct_shadow_work": scene.shadow_work,
        "index_setup": scene.index.setup,
        "lazy_float64_reference_bytes": scene.reference.known_array_bytes
        if scene.reference
        else 0,
        "known_joined_geometry_bytes": vertices.nbytes + faces.nbytes,
        "output_array_bytes": sum(
            a.nbytes for f in arrays.values() for a in f.values()
        ),
        "mean_rgb": {
            name: {
                k: a.mean((0, 1), dtype=float).tolist()
                if np.isfinite(a).all()
                else None
                for k, a in f.items()
            }
            for name, f in arrays.items()
        },
        "images": images,
        "wall_s": time.perf_counter() - began,
        "scope": "Finite black-boundary geometry; existing diffuse R/T only, direct beams plus exactly one neighbour exchange. No specular/BSSRDF/full multiple scattering, continuous sky, world-neighbour/temporal/LOD/native/device admission. Float32 BVH misses not globally certified; independent samples and ambiguous-hit fallback are required.",
    }
    (output / "result.json").write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps({"wall_s": result["wall_s"], "mean_rgb": result["mean_rgb"]}),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--samples", type=int, default=4)
    parser.add_argument("--seed", type=int, default=73)
    parser.add_argument("--packet", type=int, default=8192)
    parser.add_argument(
        "--shadow-backend", choices=("cached", "legacy"), default="cached"
    )
    a = parser.parse_args()
    run(
        a.inputs,
        a.output,
        a.width,
        a.height,
        a.samples,
        a.seed,
        a.packet,
        a.shadow_backend,
    )
