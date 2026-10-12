"""Keep intrinsic bark metres and area radius correct under uniform instance scale."""

import bpy
from bark_material import apply_bark


def apply_graph_bark(recipe, directory):
    paths = apply_bark(recipe, directory)
    meshes = {
        bpy.data.objects[(directory / p["mesh"]).stem].data
        for p in recipe["products"]
        if p["surface"] == "wood"
    }
    materials = {m for mesh in meshes for m in mesh.materials}
    for material in materials:
        nodes, links = material.node_tree.nodes, material.node_tree.links
        transform = nodes.new("ShaderNodeVectorTransform")
        transform.vector_type, transform.convert_from, transform.convert_to = (
            "VECTOR",
            "OBJECT",
            "WORLD",
        )
        transform.inputs[0].default_value = (1, 0, 0)
        length = nodes.new("ShaderNodeVectorMath")
        length.operation = "LENGTH"
        links.new(transform.outputs[0], length.inputs[0])
        for link in list(links):
            if link.from_node.type != "ATTRIBUTE":
                continue
            attribute = link.from_node.attribute_name
            if attribute not in ("bark_coordinates", "bark_radius"):
                continue
            source, target = link.from_socket, link.to_socket
            links.remove(link)
            if attribute == "bark_coordinates":
                scale = nodes.new("ShaderNodeVectorMath")
                scale.operation = "SCALE"
                links.new(length.outputs["Value"], scale.inputs["Scale"])
            else:
                scale = nodes.new("ShaderNodeMath")
                scale.operation = "MULTIPLY"
                links.new(length.outputs["Value"], scale.inputs[1])
            links.new(source, scale.inputs[0])
            links.new(scale.outputs[0], target)
    return paths
