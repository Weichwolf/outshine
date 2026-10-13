"""Photo-free anisotropic solid noise for a bark-function preview."""

import json

import bpy
import numpy as np
from blender_preview import material


def bark_material(recipe):
    if recipe["version"] == "layered-bark-preview-1":
        from layered_bark_material import build_layered

        return build_layered(recipe)
    if recipe["version"] != "solid-bark-preview-1":
        raise ValueError("supported complete bark material recipe required")
    result = material(
        "procedural bark in physical branch coordinates",
        (0.14, 0.10, 0.04),
        recipe["roughness"],
    )
    nodes, links = result.node_tree.nodes, result.node_tree.links
    chart = nodes.new("ShaderNodeAttribute")
    chart.attribute_name = "bark_coordinates"
    scale = nodes.new("ShaderNodeVectorMath")
    scale.operation = "MULTIPLY"
    scale.inputs[1].default_value = recipe["noise_scale_per_m"]
    links.new(chart.outputs["Vector"], scale.inputs[0])
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 1
    noise.inputs["Detail"].default_value = recipe["noise_detail"]
    noise.inputs["Roughness"].default_value = recipe["noise_roughness"]
    links.new(scale.outputs[0], noise.inputs["Vector"])
    colour = nodes.new("ShaderNodeValToRGB")
    colour.color_ramp.elements[0].position = recipe["colour_positions"][0]
    colour.color_ramp.elements[0].color = recipe["colour_low"]
    colour.color_ramp.elements[1].position = recipe["colour_positions"][1]
    colour.color_ramp.elements[1].color = recipe["colour_high"]
    links.new(noise.outputs["Fac"], colour.inputs[0])
    radius = nodes.new("ShaderNodeAttribute")
    radius.attribute_name = "bark_radius"
    age = nodes.new("ShaderNodeMapRange")
    age.interpolation_type = "SMOOTHSTEP"
    age.inputs["From Min"].default_value = recipe["radius_range_m"][0]
    age.inputs["From Max"].default_value = recipe["radius_range_m"][1]
    links.new(radius.outputs["Fac"], age.inputs[0])
    mixed = nodes.new("ShaderNodeMixRGB")
    links.new(age.outputs[0], mixed.inputs[0])
    mixed.inputs[1].default_value = recipe["young_colour"]
    links.new(colour.outputs[0], mixed.inputs[2])
    shader = nodes.get("Principled BSDF")
    links.new(mixed.outputs[0], shader.inputs["Base Color"])
    strength = nodes.new("ShaderNodeMath")
    strength.operation = "MULTIPLY"
    strength.inputs[1].default_value = recipe["bump_strength"]
    links.new(age.outputs[0], strength.inputs[0])
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Distance"].default_value = recipe["bump_distance_m"]
    links.new(strength.outputs[0], bump.inputs["Strength"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs[0], shader.inputs["Normal"])
    return result


def apply_bark(recipe, directory):
    materials = {}
    for product in recipe["products"]:
        if product["surface"] != "wood":
            continue
        material_path = directory / product["bark_material"]
        if material_path not in materials:
            materials[material_path] = bark_material(
                json.loads(material_path.read_text())
            )
        path = directory / product["mesh"]
        mesh = bpy.data.objects[path.stem].data
        material_recipe = json.loads(material_path.read_text())
        fields = (
            [
                (f"bark_{prefix}_{i}", kind, field)
                for i in range(material_recipe["layers"])
                for prefix, kind, field in (
                    ("chart", "FLOAT_VECTOR", "vector"),
                    ("weight", "FLOAT", "value"),
                    ("radius", "FLOAT", "value"),
                )
            ]
            if material_recipe["version"] == "layered-bark-preview-1"
            else [
                ("bark_coordinates", "FLOAT_VECTOR", "vector"),
                ("bark_radius", "FLOAT", "value"),
            ]
        )
        with np.load(path) as data:
            for name, kind, field in fields:
                values = data[name]
                if len(values) != len(mesh.vertices) or not np.isfinite(values).all():
                    raise ValueError(
                        "complete finite point material attributes required"
                    )
                attribute = mesh.attributes.new(name, kind, "POINT")
                attribute.data.foreach_set(field, values.astype(np.float32).ravel())
        mesh.materials.clear()
        mesh.materials.append(materials[material_path])
    return list(materials)
