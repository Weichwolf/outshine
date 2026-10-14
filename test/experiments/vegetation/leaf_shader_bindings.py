"""Blender binds decoded public leaf material fields without procedural regeneration."""

import bpy
import numpy as np
from organ_preview import leaf_material
from outshine_ref.vegetation import LeafMaterialLevel
from outshine_ref.vegetation.leaf_optics import optical_mixture


def texture_image(name, values):
    image = bpy.data.images.new(
        name, width=len(values), height=len(values), float_buffer=True
    )
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    pixels = np.ones(values.shape[:2] + (4,), dtype=np.float32)
    pixels[..., :3] = values if values.ndim == 3 else values[..., None]
    image.pixels.foreach_set(pixels.ravel())
    image.update()
    return image


def load_textures(path, level):
    with np.load(path) as saved:
        product = LeafMaterialLevel(
            saved[f"optical_{level}"],
            saved[f"moments_{level}"],
            saved[f"coverage_{level}"],
        )
    resolved = product.resolve()
    optical = np.concatenate(
        (resolved["reflection"], resolved["transmission"]), axis=-1
    )
    colour, transmitted, weight = optical_mixture(optical.reshape(-1, 6))
    size = len(product.coverage)
    fields = {
        "colour": colour.reshape(size, size, 3),
        "transmittance": transmitted.reshape(size, size, 3),
        "weight": weight.reshape(size, size),
        "normal": (resolved["normal"] + 1) / 2,
        "roughness": resolved["roughness"],
        "coverage": product.coverage,
    }
    return {
        key: texture_image(f"{path.stem}-{level}-{key}", value)
        for key, value in fields.items()
    }


def apply_reference_materials(recipe, directory):
    loaded, paths = {}, []
    for product in recipe["products"]:
        path = directory / product["leaf_fields"]
        level = product.get("normal_level", 0)
        key = (path, level)
        if key not in loaded:
            loaded[key] = load_textures(path, level)
            paths.append(path)
        name = (directory / product["mesh"]).stem
        material = leaf_material(name)
        material.surface_render_method = "DITHERED"
        material.use_backface_culling = False
        nodes, links = material.node_tree.nodes, material.node_tree.links
        textures = {}
        for field, image in loaded[key].items():
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = image
            texture.interpolation, texture.extension = "Linear", "EXTEND"
            textures[field] = texture.outputs["Color"]
        normal = nodes.new("ShaderNodeNormalMap")
        links.new(textures["normal"], normal.inputs["Color"])
        shader = nodes.get("Principled BSDF")
        links.new(textures["colour"], shader.inputs["Base Color"])
        links.new(textures["roughness"], shader.inputs["Roughness"])
        links.new(normal.outputs["Normal"], shader.inputs["Normal"])
        translucent = next(n for n in nodes if n.type == "BSDF_TRANSLUCENT")
        links.new(textures["transmittance"], translucent.inputs["Color"])
        links.new(normal.outputs["Normal"], translucent.inputs["Normal"])
        mixed = next(n for n in nodes if n.type == "MIX_SHADER")
        links.new(textures["weight"], mixed.inputs[0])
        transparent = nodes.new("ShaderNodeBsdfTransparent")
        coverage = nodes.new("ShaderNodeMixShader")
        links.new(textures["coverage"], coverage.inputs[0])
        links.new(transparent.outputs[0], coverage.inputs[1])
        links.new(mixed.outputs[0], coverage.inputs[2])
        links.new(coverage.outputs[0], nodes.get("Material Output").inputs["Surface"])
        mesh = bpy.data.objects[name].data
        mesh.materials.clear()
        mesh.materials.append(material)
    return paths
