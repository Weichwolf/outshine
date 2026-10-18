"""Original qualitative fern BSDF with explicit connected material-field consumers."""

import bpy
import numpy as np


def field_image(name, array):
    image = bpy.data.images.new(
        name, width=array.shape[1], height=array.shape[0], alpha=True, float_buffer=True
    )
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    image.pixels.foreach_set(np.asarray(array, np.float32).ravel())
    image.update()
    return image


class FieldGraph:
    def __init__(self, material):
        self.nodes, self.links = material.node_tree.nodes, material.node_tree.links

    def link(self, value, socket):
        if hasattr(value, "is_output"):
            self.links.new(value, socket)
        else:
            socket.default_value = value

    def scalar(self, operation, a, b):
        node = self.nodes.new("ShaderNodeMath")
        node.operation = operation
        self.link(a, node.inputs[0])
        self.link(b, node.inputs[1])
        return node.outputs[0]

    def vector(self, operation, a, b=None):
        node = self.nodes.new("ShaderNodeVectorMath")
        node.operation = operation
        self.link(a, node.inputs[0])
        if b is not None:
            self.link(b, node.inputs[1])
        return node.outputs[0]

    def combine(self, x, y, z):
        node = self.nodes.new("ShaderNodeCombineXYZ")
        for v, s in zip((x, y, z), node.inputs, strict=True):
            self.link(v, s)
        return node.outputs[0]

    def texture(self, image):
        node = self.nodes.new("ShaderNodeTexImage")
        node.image, node.interpolation, node.extension = image, "Linear", "EXTEND"
        split = self.nodes.new("ShaderNodeSeparateXYZ")
        self.links.new(node.outputs["Color"], split.inputs[0])
        return node.outputs["Color"], tuple(split.outputs), node.outputs["Alpha"]


def connected_material(template, images, basis, *, broaden):
    result = template.copy()
    result.name = "connected original material " + ("variance" if broaden else "mean")
    result.surface_render_method = "DITHERED"
    g = FieldGraph(result)
    # Remove only the two original image nodes after copying; source keeps its nodes.
    for node in list(g.nodes):
        if node.type == "TEX_IMAGE":
            g.nodes.remove(node)
    colour, _, alpha = g.texture(images["colour"])
    _, a, _ = g.texture(images["slope_a"])
    _, b, _ = g.texture(images["slope_b"])
    safe = g.scalar("MAXIMUM", alpha, 1e-8)

    def scalar(value):
        return g.scalar("DIVIDE", value, safe)

    x, y, xx, yy = (scalar(v) for v in (a[0], a[1], a[2], b[1]))
    normal = g.vector("NORMALIZE", g.combine(x, y, 1.0))
    split = g.nodes.new("ShaderNodeSeparateXYZ")
    g.links.new(normal, split.inputs[0])
    columns = []
    for axis, component in zip(np.asarray(basis).T, split.outputs, strict=True):
        columns.append(
            g.vector(
                "MULTIPLY", tuple(axis), g.combine(component, component, component)
            )
        )
    normal = g.vector("ADD", g.vector("ADD", columns[0], columns[1]), columns[2])
    world = g.nodes.new("ShaderNodeVectorTransform")
    world.vector_type, world.convert_from, world.convert_to = (
        "NORMAL",
        "OBJECT",
        "WORLD",
    )
    g.links.new(normal, world.inputs[0])
    bump = next(n for n in g.nodes if n.type == "BUMP")
    g.links.new(world.outputs[0], bump.inputs["Normal"])
    g.links.new(scalar(b[2]), bump.inputs["Height"])
    pigment = g.vector("DIVIDE", colour, g.combine(safe, safe, safe))
    shader = g.nodes.get("Principled BSDF")
    g.links.new(pigment, shader.inputs["Base Color"])
    thin = next(n for n in g.nodes if n.type == "BSDF_TRANSLUCENT")
    g.links.new(pigment, thin.inputs["Color"])
    if broaden:
        variance = g.scalar(
            "ADD",
            g.scalar("SUBTRACT", xx, g.scalar("MULTIPLY", x, x)),
            g.scalar("SUBTRACT", yy, g.scalar("MULTIPLY", y, y)),
        )
        variance = g.scalar("MAXIMUM", variance, 0.0)
        roughness4 = float(shader.inputs["Roughness"].default_value) ** 4
        roughness = g.scalar("POWER", g.scalar("ADD", roughness4, variance), 0.25)
        g.links.new(g.scalar("MINIMUM", roughness, 1.0), shader.inputs["Roughness"])
    mixed = next(n for n in g.nodes if n.type == "MIX_SHADER")
    transparent = g.nodes.new("ShaderNodeBsdfTransparent")
    coverage = g.nodes.new("ShaderNodeMixShader")
    g.links.new(alpha, coverage.inputs[0])
    g.links.new(transparent.outputs[0], coverage.inputs[1])
    g.links.new(mixed.outputs[0], coverage.inputs[2])
    g.links.new(coverage.outputs[0], g.nodes.get("Material Output").inputs["Surface"])
    return result
