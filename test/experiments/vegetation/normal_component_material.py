"""Rejected minified atlas diagnostic: cross-view mips can mix normal frames."""

import bpy
import numpy as np


def atlas(values):
    values = np.asarray(values, np.float32)
    if values.shape != (64, 32, 32, 3) or not np.isfinite(values).all():
        raise ValueError("same bounded64-view finite vector fields required")
    pixels = np.zeros((320, 320, 4), np.float32)
    for index, value in enumerate(values):
        x, y = index % 8 * 40, index // 8 * 40
        pixels[y : y + 40, x : x + 40, :3] = np.pad(
            value, ((4, 4), (4, 4), (0, 0)), mode="edge"
        )
    pixels[..., 3] = 1
    image = bpy.data.images.new(
        "diagnostic signed Float32 field",
        width=320,
        height=320,
        alpha=True,
        float_buffer=True,
    )
    image.colorspace_settings.name = "Non-Color"
    image.alpha_mode = "CHANNEL_PACKED"
    image.pixels.foreach_set(pixels.ravel())
    image.update()
    return image


def material(vectors, images, frames, light, mode, *, image_factory=atlas):
    vectors, images, frames, light = map(np.asarray, (vectors, images, frames, light))
    if (
        vectors.shape != (64, 32, 32, 4, 3)
        or images.shape != (64, 32, 32, 9)
        or frames.shape != (64, 3, 3)
        or mode not in ("mean", "four")
    ):
        raise ValueError("same complete prototype and declared response mode required")
    result = bpy.data.materials.new("isolated " + mode + " diffuse normal component")
    result.use_nodes = True
    result.surface_render_method = "DITHERED"
    nodes, links = result.node_tree.nodes, result.node_tree.links
    nodes.clear()

    def vector(operation, a, b=None):
        node = nodes.new("ShaderNodeVectorMath")
        node.operation = operation
        for value, socket in zip((a, b), node.inputs, strict=False):
            if value is None:
                continue
            if hasattr(value, "is_output"):
                links.new(value, socket)
            else:
                socket.default_value = value
        return node.outputs[
            "Value" if operation in ("DOT_PRODUCT", "LENGTH") else "Vector"
        ]

    def scalar(operation, a, b):
        node = nodes.new("ShaderNodeMath")
        node.operation = operation
        for value, socket in zip((a, b), node.inputs, strict=False):
            if hasattr(value, "is_output"):
                links.new(value, socket)
            else:
                socket.default_value = value
        return node.outputs[0]

    def texture(fields):
        node = nodes.new("ShaderNodeTexImage")
        node.image = image_factory(fields)
        node.interpolation = "Linear"
        node.extension = "EXTEND"
        return node.outputs["Color"]

    normal = nodes.new("ShaderNodeNewGeometry").outputs["Normal"]
    tangent = nodes.new("ShaderNodeTangent")
    tangent.direction_type = "UV_MAP"
    tangent.uv_map = "physical source UV"
    t = tangent.outputs["Tangent"]
    b = vector("CROSS_PRODUCT", normal, t)
    local = nodes.new("ShaderNodeCombineXYZ")
    for direction, socket in zip((t, b, normal), local.inputs, strict=False):
        links.new(vector("DOT_PRODUCT", direction, light), socket)
    alpha_image = texture(np.repeat(images[..., 3, None], 3, axis=-1))
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(alpha_image, split.inputs[0])
    alpha = split.outputs[0]
    if mode == "four":
        response = 0.0
        for index in range(4):
            term = scalar(
                "MAXIMUM",
                vector(
                    "DOT_PRODUCT", texture(vectors[..., index, :]), local.outputs[0]
                ),
                0,
            )
            response = scalar("ADD", response, term)
        radiance = scalar("DIVIDE", response, scalar("MAXIMUM", alpha, 1e-8))
    else:
        raw = np.einsum("vxyj,vjk->vxyk", images[..., 4:7], frames)
        mean = texture(raw)
        radiance = scalar(
            "MAXIMUM",
            vector("DOT_PRODUCT", vector("NORMALIZE", mean), local.outputs[0]),
            0,
        )
    emission = nodes.new("ShaderNodeEmission")
    links.new(radiance, emission.inputs["Color"])
    weight = nodes.new("ShaderNodeAttribute")
    weight.attribute_name = "window_tail_weight"
    opacity = scalar("MULTIPLY", alpha, weight.outputs["Fac"])
    mix = nodes.new("ShaderNodeMixShader")
    links.new(opacity, mix.inputs[0])
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    links.new(transparent.outputs[0], mix.inputs[1])
    links.new(emission.outputs[0], mix.inputs[2])
    output = nodes.new("ShaderNodeOutputMaterial")
    links.new(mix.outputs[0], output.inputs["Surface"])
    return result
