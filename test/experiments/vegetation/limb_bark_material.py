"""Reuse solid bark shading, with shared procedural plate height and pigmentation."""

from bark_material import bark_material


def mature_material(recipe):
    material = bark_material(recipe)
    nodes, links = material.node_tree.nodes, material.node_tree.links
    noise = next(node for node in nodes if node.bl_idname == "ShaderNodeTexNoise")
    chart = next(
        node
        for node in nodes
        if node.bl_idname == "ShaderNodeAttribute"
        and node.attribute_name == "bark_coordinates"
    )
    targets = [link.to_socket for link in noise.outputs["Fac"].links]
    scale = nodes.new("ShaderNodeVectorMath")
    scale.operation = "MULTIPLY"
    scale.inputs[1].default_value = recipe["fissure_scale_per_m"]
    links.new(chart.outputs["Vector"], scale.inputs[0])
    warp = nodes.new("ShaderNodeTexNoise")
    warp.inputs["Scale"].default_value = 0.8
    warp.inputs["Detail"].default_value = 2
    links.new(scale.outputs[0], warp.inputs["Vector"])
    offset = nodes.new("ShaderNodeVectorMath")
    offset.operation = "SUBTRACT"
    offset.inputs[1].default_value = (0.5, 0.5, 0.5)
    links.new(warp.outputs["Color"], offset.inputs[0])
    amplitude = nodes.new("ShaderNodeVectorMath")
    amplitude.operation = "SCALE"
    amplitude.inputs["Scale"].default_value = 0.9
    links.new(offset.outputs[0], amplitude.inputs[0])
    warped = nodes.new("ShaderNodeVectorMath")
    warped.operation = "ADD"
    links.new(scale.outputs[0], warped.inputs[0])
    links.new(amplitude.outputs[0], warped.inputs[1])
    cells = nodes.new("ShaderNodeTexVoronoi")
    cells.feature = "DISTANCE_TO_EDGE"
    cells.inputs["Scale"].default_value = 1
    links.new(warped.outputs[0], cells.inputs["Vector"])
    height = nodes.new("ShaderNodeMapRange")
    height.interpolation_type = "SMOOTHSTEP"
    height.inputs["From Max"].default_value = recipe["fissure_plateau"]
    links.new(cells.outputs["Distance"], height.inputs["Value"])
    modulation = nodes.new("ShaderNodeMath")
    modulation.operation = "MULTIPLY_ADD"
    modulation.inputs[1].default_value = 0.25
    modulation.inputs[2].default_value = 0.75
    links.new(warp.outputs["Fac"], modulation.inputs[0])
    plate = nodes.new("ShaderNodeMath")
    plate.operation = "MULTIPLY"
    links.new(height.outputs[0], plate.inputs[0])
    links.new(modulation.outputs[0], plate.inputs[1])
    grain = nodes.new("ShaderNodeMath")
    grain.operation = "MULTIPLY_ADD"
    links.new(noise.outputs["Fac"], grain.inputs[0])
    grain.inputs[1].default_value = 0.08
    links.new(plate.outputs[0], grain.inputs[2])
    for target in targets:
        links.new(grain.outputs[0], target)
    return material
