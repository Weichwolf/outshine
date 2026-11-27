"""Capture scene-linear mirrors once per EEVEE reference camera."""

from contextlib import contextmanager
from pathlib import Path
from tempfile import TemporaryDirectory
from time import perf_counter

import bpy
from materials import WATER_IOR, WATER_MATERIALS
from mathutils import Matrix
from water_reflection import LEVEL_TOLERANCE_M, REFLECTION_RANGE_M


def scalar(tree, operation, a, b):
    node = tree.nodes.new("ShaderNodeMath")
    node.operation = operation
    for target, source in zip(node.inputs, (a, b)):
        if isinstance(source, (int, float)):
            target.default_value = source
        else:
            tree.links.new(source, target)
    return node.outputs[0]


def fade_out(tree, source, start, end):
    node = tree.nodes.new("ShaderNodeMapRange")
    node.interpolation_type = "SMOOTHSTEP"
    tree.links.new(source, node.inputs["Value"])
    node.inputs["From Min"].default_value = start
    node.inputs["From Max"].default_value = end
    node.inputs["To Min"].default_value = 1
    node.inputs["To Max"].default_value = 0
    return node.outputs[0]


@contextmanager
def capture_clip(materials):
    patches = []
    try:
        for material in materials:
            tree = material.node_tree
            target = tree.nodes["Material Output"].inputs["Surface"]
            source = target.links[0].from_socket
            nodes = tree.nodes
            position = nodes.new("ShaderNodeNewGeometry")
            normal = nodes.new("ShaderNodeVectorMath")
            normal.operation = "DOT_PRODUCT"
            clip = nodes.new("ShaderNodeMath")
            clip.operation = "LESS_THAN"
            transparent = nodes.new("ShaderNodeBsdfTransparent")
            mix = nodes.new("ShaderNodeMixShader")
            patches.append(
                (tree, target, source, (position, normal, clip, transparent, mix))
            )
            tree.links.new(position.outputs["Position"], normal.inputs[0])
            tree.links.new(normal.outputs["Value"], clip.inputs[0])
            tree.links.new(clip.outputs[0], mix.inputs[0])
            tree.links.new(source, mix.inputs[1])
            tree.links.new(transparent.outputs[0], mix.inputs[2])
            tree.links.new(mix.outputs[0], target)
        yield [(nodes[1].inputs[1], nodes[2].inputs[1]) for _, _, _, nodes in patches]
    finally:
        for tree, target, source, nodes in patches:
            for node in reversed(nodes):
                tree.nodes.remove(node)
            tree.links.new(source, target)


def mirrored_camera(scene, plane, pose):
    mirror = Matrix(plane.mirror().tolist())
    flip_up = Matrix.Diagonal((1, -1, 1, 1))
    scene.camera.matrix_world = mirror @ pose @ flip_up


def capture_images(scene, geometry, planes):
    start = perf_counter()
    settings = scene.render
    format_before = settings.image_settings.file_format
    color_before = settings.image_settings.color_mode
    original = (
        settings.resolution_percentage,
        settings.filepath,
        settings.film_transparent,
    )
    samples = scene.eevee.taa_render_samples
    bpy.context.view_layer.update()
    pose = scene.camera.matrix_world.copy()
    water = [
        obj
        for obj in geometry.objects
        if obj.type == "MESH"
        and any(m.name in WATER_MATERIALS for m in obj.data.materials)
    ]
    visible = [obj.hide_render for obj in water]
    materials = [
        m for m in bpy.data.materials if m.node_tree and m.name not in WATER_MATERIALS
    ]
    images = []
    try:
        scene.eevee.taa_render_samples = 6
        settings.resolution_percentage = 50
        settings.film_transparent = True
        settings.image_settings.file_format = "OPEN_EXR"
        settings.image_settings.color_mode = "RGBA"
        for obj in water:
            obj.hide_render = True
        with (
            TemporaryDirectory(prefix="outshine-vienna-reflection-") as tmp,
            capture_clip(materials) as clips,
        ):
            for i, plane in enumerate(planes):
                mirrored_camera(scene, plane, pose)
                for normal, threshold in clips:
                    normal.default_value = plane.normal
                    threshold.default_value = -plane.offset - LEVEL_TOLERANCE_M / 2
                settings.filepath = str(Path(tmp) / f"plane-{i}.exr")
                if "FINISHED" not in bpy.ops.render.render(
                    write_still=True, scene=scene.name
                ):
                    raise RuntimeError("EEVEE water capture did not finish")
                image = bpy.data.images.load(settings.filepath)
                image.alpha_mode = "PREMUL"
                image.pack()
                images.append(image)
    finally:
        scene.camera.matrix_world = pose
        settings.resolution_percentage, settings.filepath, settings.film_transparent = (
            original
        )
        settings.image_settings.file_format = format_before
        settings.image_settings.color_mode = color_before
        scene.eevee.taa_render_samples = samples
        for obj, hidden in zip(water, visible, strict=True):
            obj.hide_render = hidden
    return images, perf_counter() - start


def apply_reflection(material, eye, planes, images):
    tree = material.node_tree
    nodes, link = tree.nodes, tree.links.new
    coordinates = nodes.new("ShaderNodeTexCoord")
    window = nodes.new("ShaderNodeVectorMath")
    window.operation = "MULTIPLY_ADD"
    link(coordinates.outputs["Window"], window.inputs[0])
    window.inputs[1].default_value = (1.0, -1.0, 1.0)
    window.inputs[2].default_value = (0.0, 1.0, 0.0)

    position = nodes.new("ShaderNodeNewGeometry").outputs["Position"]
    relative = nodes.new("ShaderNodeVectorMath")
    relative.operation = "SUBTRACT"
    link(position, relative.inputs[0])
    relative.inputs[1].default_value = (*eye[:2], 0)
    horizontal = nodes.new("ShaderNodeVectorMath")
    horizontal.operation = "MULTIPLY"
    link(relative.outputs[0], horizontal.inputs[0])
    horizontal.inputs[1].default_value = (1, 1, 0)
    distance = nodes.new("ShaderNodeVectorMath")
    distance.operation = "LENGTH"
    link(horizontal.outputs[0], distance.inputs[0])
    fade = fade_out(
        tree, distance.outputs["Value"], 0.8 * REFLECTION_RANGE_M, REFLECTION_RANGE_M
    )

    fresnel = nodes.new("ShaderNodeFresnel")
    fresnel.inputs["IOR"].default_value = WATER_IOR
    strength = scalar(tree, "MULTIPLY", fresnel.outputs[0], fade)
    target = nodes["Material Output"].inputs["Surface"]
    shaded = target.links[0].from_socket
    errors = []
    for plane in planes:
        normal = nodes.new("ShaderNodeVectorMath")
        normal.operation = "DOT_PRODUCT"
        link(position, normal.inputs[0])
        normal.inputs[1].default_value = plane.normal
        signed = scalar(tree, "ADD", normal.outputs["Value"], plane.offset)
        errors.append(scalar(tree, "ABSOLUTE", signed, 0))
    selection = [1]
    if len(errors) == 2:
        second = scalar(tree, "LESS_THAN", errors[1], errors[0])
        selection = [scalar(tree, "SUBTRACT", 1, second), second]
    for error, belongs, image in zip(errors, selection, images, strict=True):
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = image
        texture.extension = "CLIP"
        link(window.outputs[0], texture.inputs[0])
        match = fade_out(tree, error, 0.75 * LEVEL_TOLERANCE_M, LEVEL_TOLERANCE_M)
        match = scalar(tree, "MULTIPLY", match, belongs)
        opacity = scalar(tree, "MULTIPLY", texture.outputs["Alpha"], match)
        mix = nodes.new("ShaderNodeMixShader")
        emission = nodes.new("ShaderNodeEmission")
        link(texture.outputs["Color"], emission.inputs["Color"])
        link(scalar(tree, "MULTIPLY", opacity, strength), mix.inputs[0])
        link(shaded, mix.inputs[1])
        link(emission.outputs[0], mix.inputs[2])
        shaded = mix.outputs[0]
    link(shaded, target)
