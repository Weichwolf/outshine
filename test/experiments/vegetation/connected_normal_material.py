"""Unit-cosine diagnostic only: explicit original normals and raw object-frame moments."""

import bpy
import numpy as np


def material(name, light, encoded=None):
    result = bpy.data.materials.new(name)
    result.use_nodes = True
    result.surface_render_method = "DITHERED"
    nodes, links = result.node_tree.nodes, result.node_tree.links
    nodes.clear()

    def operation(kind, a, b=None, *, vector=False):
        node = nodes.new("ShaderNodeVectorMath" if vector else "ShaderNodeMath")
        node.operation = kind
        for value, socket in zip((a, b), node.inputs, strict=False):
            if value is None:
                continue
            if hasattr(value, "is_output"):
                links.new(value, socket)
            else:
                socket.default_value = value
        return node.outputs["Value" if vector and kind == "DOT_PRODUCT" else 0]

    if encoded is None:
        attr = nodes.new("ShaderNodeAttribute")
        attr.attribute_name = "original_signed_normal"
        normal = operation("NORMALIZE", attr.outputs["Vector"], vector=True)
        alpha = 1.0
    else:
        image = bpy.data.images.new(
            name,
            width=encoded.shape[1],
            height=encoded.shape[0],
            alpha=True,
            float_buffer=True,
        )
        image.colorspace_settings.name = "Non-Color"
        image.alpha_mode = "CHANNEL_PACKED"
        image.pixels.foreach_set(np.asarray(encoded, np.float32).ravel())
        image.update()
        tex = nodes.new("ShaderNodeTexImage")
        tex.image, tex.interpolation, tex.extension = image, "Linear", "EXTEND"
        alpha = tex.outputs["Alpha"]
        a = nodes.new("ShaderNodeCombineXYZ")
        for socket in a.inputs:
            links.new(alpha, socket)
        twice = operation("MULTIPLY", tex.outputs["Color"], (2, 2, 2), vector=True)
        # Stored RGB = (raw premultiplied moment + alpha)/2. Never normalize it.
        normal = operation("SUBTRACT", twice, a.outputs[0], vector=True)
    dot = operation("DOT_PRODUCT", normal, tuple(light), vector=True)
    response = operation("MAXIMUM", dot, 0.0)
    if encoded is not None:
        response = operation("DIVIDE", response, operation("MAXIMUM", alpha, 1e-8))
    emission = nodes.new("ShaderNodeEmission")
    links.new(response, emission.inputs["Color"])
    output = nodes.new("ShaderNodeOutputMaterial")
    if encoded is None:
        links.new(emission.outputs[0], output.inputs["Surface"])
    else:
        mix = nodes.new("ShaderNodeMixShader")
        transparent = nodes.new("ShaderNodeBsdfTransparent")
        links.new(alpha, mix.inputs[0])
        links.new(transparent.outputs[0], mix.inputs[1])
        links.new(emission.outputs[0], mix.inputs[2])
        links.new(mix.outputs[0], output.inputs["Surface"])
    return result


def source_attribute(obj, normals):
    attr = obj.data.attributes.new("original_signed_normal", "FLOAT_VECTOR", "CORNER")
    attr.data.foreach_set("vector", np.asarray(normals, np.float32).ravel())


def check_graph(result, *, raw):
    nodes = list(result.node_tree.nodes)
    operations = [n.operation for n in nodes if n.type == "VECT_MATH"]
    assert not any(n.type == "NEW_GEOMETRY" for n in nodes)
    assert operations.count("NORMALIZE") == (0 if raw else 1)
    if raw:
        assert operations.count("SUBTRACT") == 1
        assert all(
            n.image.colorspace_settings.name == "Non-Color"
            and n.image.alpha_mode == "CHANNEL_PACKED"
            and n.interpolation == "Linear"
            for n in nodes
            if n.type == "TEX_IMAGE"
        )
