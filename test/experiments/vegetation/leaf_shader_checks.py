"""Check actual Blender Float32 decoding and three leaf shader control bindings."""

import json
from pathlib import Path

import bpy
import numpy as np
from leaf_shader_bindings import apply_reference_materials
from leaf_shader_preview import configure_material
from organ_preview import populate
from outshine_ref.vegetation import LeafMaterialLevel
from outshine_ref.vegetation.leaf_optics import optical_mixture


def main():
    folder = Path("build/experiments/vegetation/leaf-library")
    recipe = json.loads((folder / "leaf-shader-front.json").read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    populate(recipe, folder)
    apply_reference_materials(recipe, folder)
    maximum = 0.0
    for product in recipe["products"]:
        with np.load(folder / product["leaf_fields"]) as saved:
            fields = LeafMaterialLevel(
                saved["optical_0"], saved["moments_0"], saved["coverage_0"]
            )
        resolved = fields.resolve()
        optical = np.concatenate(
            (resolved["reflection"], resolved["transmission"]), axis=-1
        )
        colour, transmitted, weight = optical_mixture(optical.reshape(-1, 6))
        expected = {
            "colour": colour.reshape(512, 512, 3),
            "transmittance": transmitted.reshape(512, 512, 3),
            "weight": weight.reshape(512, 512),
            "normal": (resolved["normal"] + 1) / 2,
            "roughness": resolved["roughness"],
            "coverage": fields.coverage,
        }
        obj = bpy.data.objects[Path(product["mesh"]).stem]
        nodes = obj.data.materials[0].node_tree.nodes
        textures = [n for n in nodes if n.type == "TEX_IMAGE"]
        for texture in textures:
            key = texture.image.name.rsplit("-", 1)[-1]
            actual = np.array(texture.image.pixels[:]).reshape(512, 512, 4)[..., :3]
            target = expected[key]
            if target.ndim == 2:
                target = target[..., None]
            error = float(np.max(abs(actual - target)))
            maximum = max(maximum, error)
            assert error <= 3e-8
        configure_material(obj, product["leaf_shader_mode"])
        translucent = next(n for n in nodes if n.type == "BSDF_TRANSLUCENT")
        if product["leaf_shader_mode"] == "reflection-only":
            assert not translucent.inputs["Color"].is_linked
            assert tuple(translucent.inputs["Color"].default_value) == (0, 0, 0, 1)
        elif product["leaf_shader_mode"] == "diffuse-sheet":
            assert (
                translucent.inputs["Normal"].links[0].from_socket.name == "True Normal"
            )
            assert (
                next(n for n in nodes if n.type == "BSDF_DIFFUSE")
                .inputs["Roughness"]
                .default_value
                == 0
            )
        else:
            assert translucent.inputs["Normal"].links[0].from_node.type == "NORMAL_MAP"
    report = {
        "products": len(recipe["products"]),
        "textures_per_product": 6,
        "max_blender_float32_decode_error": maximum,
        "generation_repeated": False,
    }
    (folder / "shader-bindings.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
