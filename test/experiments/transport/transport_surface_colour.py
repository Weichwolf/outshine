"""Optional explicit linear face albedo for the local Transport research renderer."""

import numpy as np

COLOUR_ATTRIBUTE = "transport_source_base_colour"


def attach_face_colours(obj, colours):
    colours = np.asarray(colours)
    count = len(obj.data.polygons)
    if (
        colours.shape != (count, 4)
        or count > 1048576
        or not np.isfinite(colours).all()
        or np.any(colours[:, :3] < 0)
        or np.any(colours[:, :3] > 1)
        or np.any(colours[:, 3] != 1)
    ):
        raise ValueError("bounded finite opaque linear RGBA per face required")
    corners = np.repeat(colours.astype(np.float32), 3, axis=0)
    attribute = obj.data.attributes.new(COLOUR_ATTRIBUTE, "FLOAT_COLOR", "CORNER")
    attribute.data.foreach_set("color", corners.ravel())
    actual = np.empty_like(corners)
    attribute.data.foreach_get("color", actual.ravel())
    if not np.array_equal(actual, corners):
        raise ValueError("Blender changed explicit source face albedo")
    for material in obj.data.materials:
        node = material.node_tree.nodes.new("ShaderNodeAttribute")
        node.attribute_name = COLOUR_ATTRIBUTE
        shader = material.node_tree.nodes.get("Principled BSDF")
        material.node_tree.links.new(node.outputs["Color"], shader.inputs["Base Color"])
    return {
        "source_base_colour_attribute": COLOUR_ATTRIBUTE,
        "source_base_colour_attribute_float32_exact": True,
        "source_face_colour_bytes": colours.nbytes,
        "explicit_repeated_corner_colour_bytes": corners.nbytes,
        "colour_space": "linear_rgb_opaque_no_srgb_conversion",
    }
