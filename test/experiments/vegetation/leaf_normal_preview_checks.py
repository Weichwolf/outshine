"""Validate actual Blender image decoding and bindings of the three normal controls."""

import json
from pathlib import Path

import bpy
import numpy as np
import tifffile
from organ_preview import populate
from procedural_leaf_material import apply_leaf_materials


def main():
    folder = Path("build/experiments/vegetation/normal-leaves")
    scene = json.loads((folder / "leaf-normal-levels.json").read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    populate(scene, folder)
    apply_leaf_materials(scene, folder)
    checks = []
    for product in scene["products"]:
        obj = bpy.data.objects[Path(product["mesh"]).stem]
        material = obj.data.materials[0]
        nodes = material.node_tree.nodes
        normal = next(n for n in nodes if n.type == "NORMAL_MAP")
        texture = normal.inputs["Color"].links[0].from_node
        expected_level = product["normal_level"]
        image_path = Path(texture.image.filepath)
        assert f"mip-{expected_level}-normal.tiff" in image_path.name
        image = texture.image
        size = image.size[0]
        actual = np.array(image.pixels[:]).reshape(size, size, 4)[..., :3]
        expected = tifffile.imread(image_path)[::-1] / 65535
        error = float(np.max(abs(actual - expected)))
        assert error < 1e-6
        principled = nodes.get("Principled BSDF")
        rough = principled.inputs["Roughness"].links[0].from_node.image
        prefix = "naive-" if product["normal_filter"] == "mean" else ""
        assert (
            f"mip-{expected_level}-{prefix}roughness.png" in Path(rough.filepath).name
        )
        mixed = next(n for n in nodes if n.type == "MIX_SHADER")
        assert (
            "transmission_weight"
            in Path(mixed.inputs[0].links[0].from_node.image.filepath).name
        )
        checks.append(
            {
                "mesh": product["mesh"],
                "normal_level": expected_level,
                "filter": product["normal_filter"],
                "blender_texture_decode_error": error,
            }
        )
    proof = {"controls": checks, "normal_generation_repeated": False}
    (folder / "preview-checks.json").write_text(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof))


if __name__ == "__main__":
    main()
