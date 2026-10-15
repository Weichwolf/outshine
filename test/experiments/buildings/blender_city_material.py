"""Procedural metric facade parameters composed before one Principled illumination."""

import bpy
from outshine_ref.buildings.facade_finish import FacadeFinish
from outshine_ref.buildings.modelling.material_schema import srgb_to_linear


def base(name, colour, roughness=0.68, metallic=0.0):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    shader = material.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (*colour, 1.0)
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["Metallic"].default_value = metallic
    material.diffuse_color = (*colour, 1.0)
    return material


def definition(name, values):
    colour = srgb_to_linear(
        [float(values.get("color." + axis, 0.72)) for axis in "rgb"]
    )
    return base(
        name,
        colour,
        float(values.get("roughness", 0.68)),
        float(values.get("metallic", 0.0)),
    )


def operation(tree, name, *values):
    node = tree.nodes.new("ShaderNodeMath")
    node.operation = name
    for socket, value in zip(node.inputs, values, strict=False):
        if isinstance(value, (int, float)):
            socket.default_value = value
        else:
            tree.links.new(value, socket)
    return node.outputs[0]


def colour_term(tree, colour, weight):
    node = tree.nodes.new("ShaderNodeMixRGB")
    node.blend_type = "MULTIPLY"
    node.inputs[0].default_value = 1.0
    node.inputs[1].default_value = (*colour, 1.0)
    tree.links.new(weight, node.inputs[2])
    return node.outputs[0]


def colour_sum(tree, first, second):
    node = tree.nodes.new("ShaderNodeMixRGB")
    node.blend_type = "ADD"
    node.inputs[0].default_value = 1.0
    tree.links.new(first, node.inputs[1])
    tree.links.new(second, node.inputs[2])
    return node.outputs[0]


def weights(tree, fractions):
    if list(fractions) == [1.0, 1.0]:
        return tuple(
            operation(tree, "ADD", value, 0.0) for value in (0.0, 0.0, 1.0, 0.0)
        )
    finish = FacadeFinish()
    texture = tree.nodes.new("ShaderNodeTexCoord")
    uv = tree.nodes.new("ShaderNodeSeparateXYZ")
    tree.links.new(texture.outputs["UV"], uv.inputs[0])
    attribute = tree.nodes.new("ShaderNodeAttribute")
    attribute.attribute_name = "facade_cell_m"
    size = tree.nodes.new("ShaderNodeSeparateXYZ")
    tree.links.new(attribute.outputs["Vector"], size.inputs[0])
    outer, inner = [], []
    for index, fraction in enumerate(fractions):
        repeat = operation(tree, "FRACT", uv.outputs[index])
        distance = operation(tree, "ABSOLUTE", operation(tree, "SUBTRACT", repeat, 0.5))
        outer.append(operation(tree, "LESS_THAN", distance, fraction / 2))
        safe_size = operation(tree, "MAXIMUM", size.outputs[index], 1e-6)
        frame = operation(tree, "DIVIDE", finish.frame_m, safe_size)
        half = operation(
            tree, "MAXIMUM", operation(tree, "SUBTRACT", fraction / 2, frame), 0.0
        )
        inner.append(operation(tree, "LESS_THAN", distance, half))
    opening = operation(tree, "MULTIPLY", *outer)
    glass = operation(tree, "MULTIPLY", *inner)
    frame = operation(tree, "SUBTRACT", opening, glass)
    wall = operation(tree, "SUBTRACT", 1.0, opening)
    y = operation(tree, "FRACT", uv.outputs[1])
    edge = operation(tree, "MINIMUM", y, operation(tree, "SUBTRACT", 1.0, y))
    half_band = operation(
        tree,
        "DIVIDE",
        finish.band_m / 2,
        operation(tree, "MAXIMUM", size.outputs[1], 1e-6),
    )
    band = operation(
        tree, "MULTIPLY", wall, operation(tree, "LESS_THAN", edge, half_band)
    )
    return operation(tree, "SUBTRACT", wall, band), frame, glass, band


def facade(name, spec, wall_values):
    material = definition(name, wall_values)
    tree = material.node_tree
    shader = tree.nodes["Principled BSDF"]
    wall = tuple(shader.inputs["Base Color"].default_value[:3])
    roughness = shader.inputs["Roughness"].default_value
    metallic = shader.inputs["Metallic"].default_value
    areas = weights(tree, spec["fractions"])
    colours = (
        wall,
        (0.12, 0.14, 0.16),
        (0.025, 0.035, 0.04),
        tuple(x * 0.65 for x in wall),
    )
    terms = [
        colour_term(tree, colour, area)
        for colour, area in zip(colours, areas, strict=True)
    ]
    mixed = terms[0]
    for term in terms[1:]:
        mixed = colour_sum(tree, mixed, term)
    tree.links.new(mixed, shader.inputs["Base Color"])
    for label, values in (
        ("Roughness", (roughness, 0.28, 0.16, roughness)),
        ("Metallic", (metallic, 0.65, 0.0, metallic)),
    ):
        values = [
            operation(tree, "MULTIPLY", value, area)
            for value, area in zip(values, areas, strict=True)
        ]
        total = values[0]
        for term in values[1:]:
            total = operation(tree, "ADD", total, term)
        tree.links.new(total, shader.inputs[label])
    return material
