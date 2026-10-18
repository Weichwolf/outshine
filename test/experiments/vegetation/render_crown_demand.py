"""Same-camera coverage images of full-reference versus bounded original instances."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import numpy as np
from blender_instances import instance_group
from crown_demand_probe import cameras
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from PIL import Image
from preview_shadows import configure_preview
from render_spatial_clouds import coverage_material
from spatial_group_source import load_source


def scene_objects(poses, templates, camera):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    triangles = 0
    for variant in np.unique(poses["variants"]):
        with np.load(templates / f"prototype-{variant}.npz", allow_pickle=False) as z:
            product = dict(z)
        obj = mesh_object(
            f"leaf{variant}",
            product["vertices"],
            product["faces"],
            product["normals"],
            coverage_material(f"coverage{variant}", product["colour"][..., 3]),
            True,
            uv=product["uv"],
        )
        chosen = poses["variants"] == variant
        ids = poses["ids"][chosen]
        if np.max(ids) > 2**31 - 1:
            raise ValueError("Blender diagnostic point IDs must fit signed32 bits")
        holder = instance_group(
            obj,
            poses["origins"][chosen],
            poses["frames"][chosen],
            np.ones(len(ids)),
            name=f"instances{variant}",
        )
        attr = holder.data.attributes.new("id", "INT", "POINT")
        attr.data.foreach_set("value", ids.astype(np.int32))
        holder.data.update()
        triangles += len(ids) * len(product["faces"])
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = 32
    scene.camera = configure_camera(
        {
            "projection": "perspective",
            "eye": camera.position,
            "target": camera.target,
            "vertical_fov_deg": camera.fov_y,
            "camera_basis": np.column_stack(
                (camera.axes[0], camera.axes[1], -camera.axes[2])
            ).tolist(),
            "clip_near_m": camera.near,
            "clip_far_m": camera.far,
        }
    )
    scene.world = bpy.data.worlds.new("neutral coverage background")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.03,
        0.03,
        0.03,
        1,
    )
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "Standard"
    return scene, triangles


def render(inputs, proof, templates, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    source = load_source(inputs)
    report = json.loads((proof / "report.json").read_text())
    if report["source_identity"] != source.identity:
        raise ValueError("source identity differs from the exhaustive comparator")
    with np.load(proof / "reference-poses.npz", allow_pickle=False) as z:
        reference = dict(z)
    np.testing.assert_array_equal(
        reference["ids"], np.arange(len(source.crown.variants))
    )
    records = []
    images = {}
    camera = cameras()["inside"]
    with patch(
        "crown_shoots.CrownShoots.group",
        side_effect=AssertionError("pose build in reader"),
    ):
        for method in ("reference", "root", "arc"):
            if method == "reference":
                poses = reference
            else:
                with np.load(
                    proof / f"inside-{method}-selection.npz", allow_pickle=False
                ) as z:
                    mask = np.repeat(z["detail"], source.crown.counts)
                poses = {k: v[mask] for k, v in reference.items()}
            scene, triangles = scene_objects(poses, templates, camera)
            scene.render.filepath = str(output / (method + ".png"))
            before = time.perf_counter()
            bpy.ops.render.render(write_still=True)
            records.append(
                {
                    "method": method,
                    "leaf_instances": len(poses["ids"]),
                    "instance_triangles": triangles,
                    "render_s": time.perf_counter() - before,
                }
            )
            images[method] = np.asarray(Image.open(scene.render.filepath))[..., :3]
    differences = {}
    for method in ("root", "arc"):
        error = np.abs(np.subtract(images[method], images["reference"], dtype=np.int16))
        differences[method] = {
            "mean_RGB8_error": float(error.mean()),
            "max_RGB8_error": int(error.max()),
            "different_pixels": int(np.any(error, axis=-1).sum()),
        }
    result = {
        "commit": commit,
        "camera": {
            "eye": camera.position,
            "target": camera.target,
            "vertical_fov_deg": camera.fov_y,
            "clip_m": [camera.near, camera.far],
            "query_profile": [camera.width, camera.height],
        },
        "source_identity": source.identity,
        "resolution": [2560, 1440],
        "samples": 32,
        "cases": records,
        "differences": differences,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Finished full-reference pose table only for independent render control; same original IDs provided to Blender point instances, shared connected32 templates, emission/alpha coverage, no light or shadow/BRDF/native/device/forest admission. Reader does not generate poses. Camera-only culling and rest pose.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("inputs", "proof", "templates", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    a = p.parse_args()
    render(a.inputs, a.proof, a.templates, a.output)
