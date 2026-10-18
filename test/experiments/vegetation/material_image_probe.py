"""Check original image data, UV orientation and repeat filtering against Blender."""

import argparse
import json
from pathlib import Path

import bpy
import numpy as np
from connected_material_fields import repeated_image
from outshine_ref.rendering.blender_mesh import mesh_object
from PIL import Image
from render_connected_normals import configure


def image_probe(original, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    points = np.random.default_rng(704).uniform(-0.2, 1.2, (64, 2))
    points[:4] = ((0, 0), (1, 1), (-0.1, 0.2), (0.5, 0.5))
    vertices, faces, uv = [], [], []
    for i, coordinate in enumerate(points):
        row, col = divmod(i, 8)
        a = np.array((col / 8 - 0.5, row / 8 - 0.5, 0.0))
        vertices.extend(
            a + np.array(((0, 0, 0), (1 / 8, 0, 0), (1 / 8, 1 / 8, 0), (0, 1 / 8, 0)))
        )
        faces.extend(np.array(((0, 1, 2), (0, 2, 3))) + 4 * i)
        uv.extend((coordinate,) * 4)
    cases = []
    for kind in ("colour", "height"):
        path = original / "spatial" / f"fern-{kind}.png"
        pixels = np.asarray(Image.open(path))[::-1]
        values = pixels / (65535 if kind == "height" else 255)
        if kind == "height":
            values = np.repeat(values[..., None], 3, axis=-1)
        expected = repeated_image(values, points)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        material = bpy.data.materials.new("original image readback")
        material.use_nodes = True
        nodes, links = material.node_tree.nodes, material.node_tree.links
        nodes.clear()
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = bpy.data.images.load(str(path))
        texture.image.colorspace_settings.name = "Non-Color"
        assert texture.extension == "REPEAT" and texture.interpolation == "Linear"
        emission = nodes.new("ShaderNodeEmission")
        links.new(texture.outputs["Color"], emission.inputs["Color"])
        final = nodes.new("ShaderNodeOutputMaterial")
        links.new(emission.outputs[0], final.inputs["Surface"])
        mesh_object(
            "UV readback grid",
            vertices,
            faces,
            np.tile((0.0, 0, 1), (len(vertices), 1)),
            material,
            uv=uv,
        )
        scene = configure(
            np.array((0.0, 0, 1)), np.zeros(3), 1.0, width=256, height=256, samples=16
        )
        scene.render.filepath = str(output / (kind + ".png"))
        bpy.ops.render.render(write_still=True)
        read = (
            np.asarray(Image.open(scene.render.filepath))[..., :3].astype(float) / 255
        )
        actual = []
        for i in range(64):
            row, col = divmod(i, 8)
            y, x = 255 - (32 * row + 16), 32 * col + 16
            actual.append(read[y - 2 : y + 3, x - 2 : x + 3].mean((0, 1)))
        error = float(np.max(abs(np.asarray(actual) - expected)))
        if error > 0.006:
            raise AssertionError(f"original {kind} image readback differs:{error}")
        cases.append(
            {
                "kind": kind,
                "uv": points.tolist(),
                "maximum_linear_channel_error": error,
                "declared_limit": 0.006,
                "expected": expected.tolist(),
                "actual": np.asarray(actual).tolist(),
            }
        )
    (output / "report.json").write_text(
        json.dumps(
            {
                "cases": cases,
                "scope": "Actual original256x256 RGB8/height16 PNGs;64 constant-UV patches, Raw EEVEE image emission. Independent shader image orientation/repeat/linear-data sampler check, PNG8 readback; no BSDF validation.",
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
    image_probe(a.original, a.output)
