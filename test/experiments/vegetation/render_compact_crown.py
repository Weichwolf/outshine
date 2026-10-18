"""Original versus exact compact parameters versus explicit Float32 approximation."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import numpy as np
from compact_crown import CompactCrown
from crown_demand import CrownDemand
from crown_demand_probe import cameras
from PIL import Image
from render_crown_demand import scene_objects
from spatial_group_source import load_source


def compare(images):
    result = {}
    for name in ("compact64", "compact32"):
        error = abs(images[name].astype(np.int16) - images["original"])
        result[name] = {
            "mean_RGB8_error": float(error.mean()),
            "max_RGB8_error": int(error.max()),
            "different_pixels": int(np.any(error, axis=-1).sum()),
            "different_fraction": float(np.any(error, axis=-1).mean()),
        }
    if result["compact64"]["different_pixels"]:
        raise AssertionError("Float64 compact image must remain pixel-identical")
    if (
        result["compact32"]["mean_RGB8_error"] > 0.1
        or result["compact32"]["different_fraction"] > 0.005
    ):
        raise AssertionError(
            "predeclared finite Float32 coverage-image limits exceeded"
        )
    return result


def render(inputs, parameters, templates, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    camera = cameras()["inside"]
    plan = CrownDemand(source)
    _, original = plan.prepare((cameras()["whole"],), max_leaves=100000)
    images, records = {}, []
    for name in ("original", "compact64", "compact32"):
        before = time.perf_counter()
        if name == "original":
            poses = original
        else:
            with (
                patch("crown_shoots.CrownShoots.group", side_effect=AssertionError),
                patch("numpy.random.default_rng", side_effect=AssertionError),
                patch.object(CompactCrown, "build", side_effect=AssertionError),
            ):
                product = CompactCrown.load(
                    parameters / f"{name}.npz", expected_source=source.identity
                )
                selected = camera.visible(product.data["lower"], product.data["upper"])
                poses = product.evaluate(selected, max_leaves=100000)
        load_evaluate_s = time.perf_counter() - before
        before = time.perf_counter()
        scene, triangles, drawn, mesh_bytes = scene_objects(poses, templates, camera)
        build_s = time.perf_counter() - before
        scene.render.filepath = str(output / (name + ".png"))
        before = time.perf_counter()
        bpy.ops.render.render(write_still=True)
        records.append(
            {
                "method": name,
                "considered_leaves": len(poses["ids"]),
                "drawn_leaves": drawn,
                "triangles": triangles,
                "expanded_mesh_array_bytes": mesh_bytes,
                "load_and_evaluate_s": load_evaluate_s,
                "scene_build_s": build_s,
                "render_s": time.perf_counter() - before,
            }
        )
        images[name] = np.asarray(Image.open(scene.render.filepath))[..., :3]
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "camera": {
            "eye": camera.position,
            "target": camera.target,
            "fov_y_deg": camera.fov_y,
            "clip_m": [camera.near, camera.far],
        },
        "resolution": [2560, 1440],
        "samples": 32,
        "cases": records,
        "differences": compare(images),
        "float32_predeclared_image_limits": {
            "mean_RGB8_error": 0.1,
            "different_fraction": 0.005,
        },
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Actual same inside camera and shared connected32 geometry/alpha. Float64 exact original poses, Float32 explicitly approximate. Finished compact readers forbid build/RNG/source poses; original full table is only diagnostic comparator. Software EEVEE coverage, no BRDF/shadow/wind/LOD/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--parameters", type=Path, required=True)
    parser.add_argument("--templates", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.inputs, args.parameters, args.templates, args.output)
