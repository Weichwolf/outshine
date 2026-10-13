"""Load physical procedural leaf products without generating their shape or textures."""

import json

import bpy
from organ_preview import leaf_material


def apply_leaf_materials(recipe, directory):
    paths = []
    for product in recipe["products"]:
        if "leaf_material" not in product:
            continue
        path = directory / product["leaf_material"]
        data = json.loads(path.read_text())
        if (
            data["version"]
            not in (
                "procedural-leaf-material-2",
                "procedural-leaf-material-3",
                "procedural-leaf-material-4",
            )
            or data["colour_space"] != "linear"
            or data["vein_field"] != "tapered-envelope-1"
        ):
            raise ValueError("versioned linear procedural leaf product required")
        if data["version"] in (
            "procedural-leaf-material-3",
            "procedural-leaf-material-4",
        ) and not {
            "transmittance",
            "transmission_weight",
        }.issubset(data["maps"]):
            raise ValueError("optical leaf requires transmission and mixture maps")
        if data["version"] == "procedural-leaf-material-4":
            level = product.get("normal_level", 0)
            mode = product.get("normal_filter", "variance")
            if type(level) is not int or not 0 <= level < len(data["normal_levels"]):
                raise ValueError("cached normal level required")
            if mode not in ("variance", "mean"):
                raise ValueError("declared normal filtering method required")
            selected = data["normal_levels"][level]
            data["maps"] = data["maps"] | {
                "normal": selected["normal"],
                "roughness": selected[
                    "roughness" if mode == "variance" else "naive_roughness"
                ],
            }
        material = leaf_material(path.stem)
        nodes, links = material.node_tree.nodes, material.node_tree.links
        mixed = next(n for n in nodes if n.type == "MIX_SHADER")
        mixed.inputs[0].default_value = data["transmission"]
        textures = {}
        for key, name in data["maps"].items():
            image_path = directory / name
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = bpy.data.images.load(
                str(image_path.resolve()), check_existing=True
            )
            texture.image.colorspace_settings.name = "Non-Color"
            texture.interpolation, texture.extension = "Linear", "EXTEND"
            textures[key] = texture
            paths.append(image_path)
        underside = nodes.new("ShaderNodeMixRGB")
        underside.blend_type = "MULTIPLY"
        underside.inputs[0].default_value = 0 if data.get("symmetric_optics") else 0.45
        underside.inputs[2].default_value = (1.4, 1.12, 1.5, 1)
        links.new(textures["colour"].outputs["Color"], underside.inputs[1])
        face = nodes.new("ShaderNodeNewGeometry")
        colour = nodes.new("ShaderNodeMixRGB")
        links.new(face.outputs["Backfacing"], colour.inputs[0])
        links.new(textures["colour"].outputs["Color"], colour.inputs[1])
        links.new(underside.outputs[0], colour.inputs[2])
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Distance"].default_value = data["height_m"]
        links.new(textures["height"].outputs["Color"], bump.inputs["Height"])
        normal_output = bump.outputs["Normal"]
        if "normal" in textures:
            normal_map = nodes.new("ShaderNodeNormalMap")
            links.new(textures["normal"].outputs["Color"], normal_map.inputs["Color"])
            normal_output = normal_map.outputs["Normal"]
        shader = nodes.get("Principled BSDF")
        links.new(colour.outputs[0], shader.inputs["Base Color"])
        links.new(textures["roughness"].outputs["Color"], shader.inputs["Roughness"])
        links.new(normal_output, shader.inputs["Normal"])
        translucent = next(n for n in nodes if n.type == "BSDF_TRANSLUCENT")
        transmitted = (
            textures["transmittance"].outputs["Color"]
            if "transmittance" in textures
            else colour.outputs[0]
        )
        links.new(transmitted, translucent.inputs["Color"])
        if "transmission_weight" in textures:
            links.new(textures["transmission_weight"].outputs["Color"], mixed.inputs[0])
        links.new(normal_output, translucent.inputs["Normal"])
        mesh = bpy.data.objects[(directory / product["mesh"]).stem].data
        mesh.materials.clear()
        mesh.materials.append(material)
        paths.append(path)
    return paths
