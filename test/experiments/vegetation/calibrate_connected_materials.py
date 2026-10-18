"""Actual qualitative-BSDF field-consumer calibration with known rigid normals."""

import argparse
import json
from pathlib import Path

import bpy
import numpy as np
from connected_material_shader import connected_material, field_image
from fern_material import attach_normals, leaf_material
from outshine_ref.rendering.blender_mesh import mesh_object
from PIL import Image
from render_connected_materials import environment


def calibrate(original, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    vertices = np.array(
        ((-0.35, -0.35, 0), (0.35, -0.35, 0), (0.35, 0.35, 0), (-0.35, 0.35, 0))
    )
    faces = np.array(((0, 1, 2), (0, 2, 3)))
    uv = vertices[:, :2] + 0.5
    normal = np.array((0.3, -0.1, 1.0))
    normal /= np.linalg.norm(normal)
    records = []
    for name, basis in (
        ("identity", np.eye(3)),
        ("rotated", np.array(((0.0, -1, 0), (1, 0, 0), (0, 0, 1)))),
    ):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        template = leaf_material(original / "spatial")
        for node in template.node_tree.nodes:
            if node.type == "TEX_IMAGE":
                colour = "colour" in Path(node.image.filepath).name
                value = (0.1, 0.2, 0.3, 1.0) if colour else (0.25, 0.25, 0.25, 1.0)
                node.image = field_image(
                    "known constant source", np.full((4, 4, 4), value)
                )
        arrays = {
            "colour": np.full((4, 4, 4), (0.1, 0.2, 0.3, 1.0)),
            "slope_a": np.full((4, 4, 4), (0.3, -0.1, 0.09, 1.0)),
            "slope_b": np.full((4, 4, 4), (-0.03, 0.01, 0.25, 1.0)),
        }
        images = {k: field_image(k, v) for k, v in arrays.items()}
        surfaces = [
            template,
            *[
                connected_material(template, images, basis, broaden=b)
                for b in (False, True)
            ],
        ]
        for index, surface in enumerate(surfaces):
            normals = np.tile(basis @ normal, (6, 1))
            obj = mesh_object(
                "known calibration",
                vertices + (index - 1) * np.array((1.0, 0, 0)),
                faces,
                normals,
                surface,
                True,
                uv=uv,
            )
            attach_normals(obj, normals)
        light = np.array((0.4, 0.2, 0.8))
        light /= np.linalg.norm(light)
        scene = environment(np.array((0.0, 0, 1)), np.zeros(3), 1.0, light=light)
        scene.render.resolution_x, scene.render.resolution_y = 384, 128
        scene.render.filepath = str(output / (name + ".png"))
        bpy.ops.render.render(write_still=True)
        pixels = (
            np.asarray(Image.open(scene.render.filepath))[..., :3].astype(float) / 255
        )
        means = []
        for centre in (192 - 384 / 3.3, 192, 192 + 384 / 3.3):
            x = round(centre)
            means.append(pixels[54:74, x - 10 : x + 10].mean((0, 1)))
        error = float(np.max(abs(np.asarray(means[1:]) - means[0])))
        if error > 0.01:
            raise AssertionError(
                f"original/candidate rigid BSDF calibration differs:{error}"
            )
        records.append(
            {
                "name": name,
                "linear_fields": True,
                "display_RGB_means": np.asarray(means).tolist(),
                "max_display_channel_difference": error,
                "limit": 0.01,
            }
        )
    (output / "report.json").write_text(
        json.dumps(
            {
                "cases": records,
                "scope": "Actual same-original-adapter flat-field BSDF consumers, original and rigid90-degree normals. Colour/height/slope decoding, frame rotation and zero-variance roughness checked; not botanical optical or nonzero-variance GGX-convolution calibration.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("original", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    a = p.parse_args()
    calibrate(a.original, a.output)
