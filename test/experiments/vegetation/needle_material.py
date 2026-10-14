"""Cache intrinsic needle band colour/roughness; consume them in the MR preview."""

import json
from pathlib import Path

import numpy as np
from PIL import Image


def write_needle_material(output, section, *, base_colour=None):
    phi, t = np.meshgrid(
        2 * np.pi * (np.arange(64) + 0.5) / 64, (np.arange(256) + 0.5) / 256
    )
    frequency = 2 if section == "rhombus" else 1
    band = np.exp(-((np.sin(frequency * phi) / 0.18) ** 2))
    if section == "sector":
        bands = [
            np.exp(-((np.angle(np.exp(1j * (phi - centre))) / 0.18) ** 2))
            for centre in (np.pi / 4, np.pi, 7 * np.pi / 4)
        ]
        band = np.maximum.reduce(bands)
    base = np.array([0.024, 0.075, 0.028] if base_colour is None else base_colour)
    if (
        base.shape != (3,)
        or not np.isfinite(base).all()
        or np.any((base < 0) | (base > 0.8))
    ):
        raise ValueError("bounded linear RGB needle colour required")
    colour = base * (0.93 + 0.07 * np.sin(t[..., None] * 23)) + 0.035 * band[
        ..., None
    ] * np.array([0.55, 0.65, 0.6])
    brown = np.exp(-((t / 0.05) ** 2))[..., None]
    colour = colour * (1 - brown) + np.array([0.09, 0.064, 0.027]) * brown
    roughness = 0.59 - 0.09 * band + 0.02 * np.cos(t * 17)
    maps = {
        "colour": f"needle-{section}-colour.png",
        "roughness": f"needle-{section}-roughness.png",
    }
    for key, value in (("colour", colour), ("roughness", roughness)):
        Image.fromarray(np.rint(value[::-1] * 255).astype(np.uint8)).save(
            output / maps[key]
        )
    name = f"needle-{section}-material.json"
    recipe = {
        "version": "procedural-needle-material-2",
        "colour_space": "linear",
        "maps": maps,
        "band_model": "angular-aggregate-1",
        "section": section,
        "metallic": 0.0,
        "transmission": 0.08,
    }
    (output / name).write_text(json.dumps(recipe, indent=2) + "\n")
    return name


def apply_needle_materials(recipe, directory):
    import bpy
    from organ_preview import leaf_material

    paths = []
    for product in recipe["products"]:
        if "needle_material" not in product:
            continue
        path = directory / product["needle_material"]
        data = json.loads(path.read_text())
        if (
            data["version"]
            not in ("procedural-needle-material-1", "procedural-needle-material-2")
            or data["colour_space"] != "linear"
        ):
            raise ValueError("versioned linear procedural needle material required")
        material = leaf_material(path.stem)
        nodes, links = material.node_tree.nodes, material.node_tree.links
        shader = nodes.get("Principled BSDF")
        shader.inputs["Metallic"].default_value = data["metallic"]
        textures = {}
        for key, name in data["maps"].items():
            p = path.parent / name
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(str(p.resolve()), check_existing=True)
            texture.image.colorspace_settings.name = "Non-Color"
            texture.interpolation = "Linear"
            textures[key] = texture
            paths.append(p)
        links.new(textures["colour"].outputs["Color"], shader.inputs["Base Color"])
        links.new(textures["roughness"].outputs["Color"], shader.inputs["Roughness"])
        mixed = next(n for n in nodes if n.type == "MIX_SHADER")
        mixed.inputs[0].default_value = data["transmission"]
        transmitted = next(n for n in nodes if n.type == "BSDF_TRANSLUCENT")
        links.new(textures["colour"].outputs["Color"], transmitted.inputs["Color"])
        mesh = bpy.data.objects[Path(product["mesh"]).stem].data
        mesh.materials.clear()
        mesh.materials.append(material)
        with np.load(directory / product["mesh"]) as saved:
            mesh.uv_layers.active.data.foreach_set("uv", saved["corner_uv"].ravel())
        paths.append(path)
    return paths
