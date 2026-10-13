"""Evaluate each physical bark chart before the partition-of-unity height blend."""


def scalar(nodes, links, operation, first, second):
    result = nodes.new("ShaderNodeMath")
    result.operation = operation
    links.new(first, result.inputs[0])
    links.new(second, result.inputs[1])
    return result.outputs[0]


def build_layered(recipe):
    from bark_material import bark_material

    count = recipe["layers"]
    if type(count) is not int or not 1 <= count <= 8:
        raise ValueError("bounded complete bark layers required")
    result = bark_material(recipe | {"version": "solid-bark-preview-1"})
    nodes, links = result.node_tree.nodes, result.node_tree.links
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
    old_noise = next(n for n in nodes if n.type == "TEX_NOISE")
    old_radius = next(
        n for n in nodes if n.type == "ATTRIBUTE" and n.attribute_name == "bark_radius"
    )
    noise_targets = [
        link.to_socket for link in links if link.from_socket == old_noise.outputs["Fac"]
    ]
    radius_targets = [link.to_socket for link in links if link.from_node == old_radius]
    height, radius = None, None
    for column in range(count):
        chart = nodes.new("ShaderNodeAttribute")
        chart.attribute_name = f"bark_chart_{column}"
        metres = nodes.new("ShaderNodeVectorMath")
        metres.operation = "SCALE"
        links.new(chart.outputs["Vector"], metres.inputs[0])
        links.new(length.outputs["Value"], metres.inputs["Scale"])
        scale = nodes.new("ShaderNodeVectorMath")
        scale.operation = "MULTIPLY"
        scale.inputs[1].default_value = recipe["noise_scale_per_m"]
        links.new(metres.outputs[0], scale.inputs[0])
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 1
        noise.inputs["Detail"].default_value = recipe["noise_detail"]
        noise.inputs["Roughness"].default_value = recipe["noise_roughness"]
        links.new(scale.outputs[0], noise.inputs["Vector"])
        weight = nodes.new("ShaderNodeAttribute")
        weight.attribute_name = f"bark_weight_{column}"
        local_radius = nodes.new("ShaderNodeAttribute")
        local_radius.attribute_name = f"bark_radius_{column}"
        scaled_radius = scalar(
            nodes,
            links,
            "MULTIPLY",
            local_radius.outputs["Fac"],
            length.outputs["Value"],
        )
        h = scalar(
            nodes, links, "MULTIPLY", noise.outputs["Fac"], weight.outputs["Fac"]
        )
        r = scalar(nodes, links, "MULTIPLY", scaled_radius, weight.outputs["Fac"])
        height = h if height is None else scalar(nodes, links, "ADD", height, h)
        radius = r if radius is None else scalar(nodes, links, "ADD", radius, r)
    for target in noise_targets:
        links.new(height, target)
    for target in radius_targets:
        links.new(radius, target)
    return result
