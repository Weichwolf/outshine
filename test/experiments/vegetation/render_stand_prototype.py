"""Matched two-tree representation probe, then unchanged flat forest populations."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from outshine_ref.rendering.blender_camera import configure_camera
from render_biome_tiles import HERE, draw_sources, population, setup
from scipy.spatial.transform import Rotation
from stand_prototype_scene import build_template, place_template


def scene_population(case, context):
    if case == "pair":
        return (
            {"size_m": 23, "camera": [0, -22, 11], "target": [0, 0, 3.3]},
            np.array(((-4, 0, 0), (4, 0, 0)), float),
            Rotation.from_euler("z", ((0,), (0.37,))).as_matrix(),
            np.array((1.0, 0.95)),
            [],
            None,
        )
    recipe = json.loads((HERE / "biome_tiles.json").read_text())[case]
    complete = population(case, recipe)
    trees = [row for row in complete if row[0] in ("oak", "beech")]
    other = [row for row in complete if row[0] not in ("oak", "beech")]
    digest = hashlib.sha256()
    for key, *arrays in complete:
        digest.update(key.encode())
        for a in arrays:
            digest.update(str((a.dtype.str, a.shape)).encode())
            digest.update(a.view(np.uint8))
    origins = np.concatenate([row[1] for row in trees])
    frames = np.concatenate([row[2] for row in trees])
    scales = np.concatenate([row[3] for row in trees])
    if scales.ndim == 2:
        if not np.all(scales == scales[:, :1]):
            raise ValueError("source broadleaf scales must remain uniform")
        scales = scales[:, 0]
    if context is None:
        raise ValueError("complete unchanged grass/conifer context products required")
    return recipe, origins, frames, scales, other, digest.hexdigest()


def render(inputs, context, output, case, mode):
    start = time.perf_counter()
    recipe, origins, frames, scales, other, digest = scene_population(case, context)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    template = build_template(inputs, mode)
    prepared = time.perf_counter()
    plant_report = place_template(template, origins, frames, scales, mode)
    instanced = time.perf_counter()
    context_report = draw_sources(case, other, context) if other else []
    scene = setup("Laubwald" if case == "pair" else case, recipe)
    if case == "pair":
        scene.camera = configure_camera(
            {
                "projection": "orthographic",
                "eye": recipe["camera"],
                "target": recipe["target"],
                "span_m": 21,
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str((output / (case + "-" + mode + ".png")).resolve())
    ready = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "mode": mode,
                "case": case,
                "prototype_s": prepared - start,
                "world_s": instanced - prepared,
                "context_environment_s": ready - instanced,
                "plants": plant_report,
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "case": case,
        "mode": mode,
        "resolution": [1920, 1080],
        "samples": 4,
        "plants": plant_report,
        "context": context_report,
        "source_population_sha256": digest,
        "prototype_s": prepared - start,
        "world_instances_s": instanced - prepared,
        "context_environment_s": ready - instanced,
        "render_s": time.perf_counter() - ready,
        "backend": gpu.platform.backend_type_get(),
        "renderer": gpu.platform.renderer_get(),
        "scope": "Same finished physical plant geometry, light, camera and source population; generic research crown substituted for both authored broadleaf categories, not additional botanical species. No forest LOD/native/device budget admission.",
    }
    (output / (case + "-" + mode + ".json")).write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--context", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--case", choices=("pair", "Laubwald", "Mischwald"), default="pair"
    )
    parser.add_argument("--mode", choices=("direct", "nested", "packed"), required=True)
    args = parser.parse_args()
    render(args.inputs, args.context, args.output, args.case, args.mode)
