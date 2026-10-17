"""Matched source corner colours/geometry normals and existing window shader binding."""

import bpy
import numpy as np
from blender_preview import mesh_object
from mathutils import Vector
from preview_shadows import configure_preview
from window_portrait_blender import bind_mesh, textures, weighted_material
from window_portrait_geometry import planes, select_views
from window_portrait_roles import role_packets

VIEW = np.array([1.0, 0.0, 0.35]) / np.linalg.norm([1.0, 0.0, 0.35])


def bind_physical(geometry, directory, maps):
    surface = weighted_material(
        "same opaque needle/wood shader", directory, maps, "camera"
    )
    nodes, links = surface.node_tree.nodes, surface.node_tree.links
    colour = nodes.new("ShaderNodeVertexColor")
    colour.layer_name = "saved source reflectance"
    leaf = nodes.new("ShaderNodeAttribute")
    leaf.attribute_name = "saved leaf membership"
    # Reuse the actual window shader with coverage one and physical geometry normals.
    for node in nodes:
        if node.type in ("BSDF_PRINCIPLED", "BSDF_TRANSLUCENT"):
            target = (
                node.inputs["Base Color"]
                if node.type == "BSDF_PRINCIPLED"
                else node.inputs["Color"]
            )
            links.new(colour.outputs["Color"], target)
            for link in tuple(node.inputs["Normal"].links):
                links.remove(link)
        if node.type == "MATH" and node.operation == "DIVIDE" and node.use_clamp:
            links.new(leaf.outputs["Fac"], node.inputs[0])
            for link in tuple(node.inputs[1].links):
                links.remove(link)
            node.inputs[1].default_value = 1
    obj = mesh_object(
        "requested physical needles",
        geometry["vertices"],
        geometry["faces"],
        geometry["normals"].reshape(-1, 3),
        surface,
        corner_normals=True,
    )
    mesh = obj.data
    rgba = np.column_stack(
        (geometry["colour"].reshape(-1, 3), np.ones(len(mesh.loops)))
    )
    mesh.color_attributes.new(
        name=colour.layer_name, type="FLOAT_COLOR", domain="CORNER"
    ).data.foreach_set("color", rgba.ravel())
    mesh.attributes.new(leaf.attribute_name, "FLOAT", "FACE").data.foreach_set(
        "value", geometry["leaf"].astype(np.float32)
    )
    mesh.attributes.new("window_tail_weight", "FLOAT", "FACE").data.foreach_set(
        "value", np.ones(len(mesh.polygons), np.float32)
    )
    # Opaque physical surfaces need the same BRDF mixture, without a card mask.
    mixtures = [
        n
        for n in nodes
        if n.type == "MIX_SHADER"
        and n.inputs[1].is_linked
        and n.inputs[1].links[0].from_node.type == "BSDF_PRINCIPLED"
    ]
    if len(mixtures) != 1:
        raise ValueError("one existing needle reflection/translucence mixture required")
    links.new(
        mixtures[0].outputs[0],
        nodes.get("Material Output").inputs["Surface"],
    )
    return obj


def bind_windows(
    window, banks, directory, view, light, *, shadows=True, merge_matching=False
):
    family = int(window["families"][0])
    bank = banks[family]
    if merge_matching:
        camera_view = select_views(window, 64, direction=view)
        shadow_view = select_views(window, 64, direction=light)
        if not shadows or not np.array_equal(camera_view, shadow_view):
            raise ValueError(
                "identical complete camera/light view before merging required"
            )
    maps = textures(bank, directory)
    if merge_matching:
        selected = {"camera": camera_view, "sun": shadow_view}
        for role, material, owners, indices in role_packets(selected, np.array([0])):
            data = planes(window, bank, selected[role], indices, family)
            surface = weighted_material(
                f"same {role} window shader", directory, maps, material
            )
            obj = bind_mesh(f"requested {role} window", data, surface)
            obj["vegetation_pass_role"] = material
            obj["shadow_lights"] = {owner: True for owner in owners}
        return
    for role, direction in (("camera", view), ("shadow", light)):
        if role == "shadow" and (not shadows or merge_matching):
            continue
        selected = select_views(window, 64, direction=direction)
        data = planes(window, bank, selected, np.array([0]), family)
        surface = weighted_material(
            f"same {role} window shader",
            directory,
            maps,
            "all" if merge_matching else role,
        )
        bind_mesh(f"requested {role} window", data, surface)


def configure_scene(extent, pixels, light, *, shadows=True):
    scene = bpy.context.scene
    scene.world = bpy.data.worlds.new("constant neutral diagnostic world")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.18,
        0.18,
        0.18,
        1,
    )
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0
    bpy.ops.object.camera_add(location=tuple(VIEW * 3))
    camera = bpy.context.object
    camera.rotation_euler = (-camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = float(extent * 2560 / pixels)
    camera.data.clip_start, camera.data.clip_end = 0.001, 20
    scene.camera = camera
    bpy.ops.object.light_add(type="SUN")
    sun = bpy.context.object
    sun.rotation_euler = (-Vector(light)).to_track_quat("-Z", "Y").to_euler()
    sun.data.energy, sun.data.angle = 1.0, 0.01
    sun.data.use_shadow = shadows
    shadow = configure_preview(scene)
    scene.eevee.taa_render_samples = 64
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure, scene.view_settings.gamma = 0, 1
    bpy.context.view_layer.update()
    return {
        "resolution": [2560, 1440],
        "samples": 64,
        "view": VIEW.tolist(),
        "light_direction": list(light),
        "sun_energy": 1.0,
        "sun_angle": 0.01,
        "sun_shadows": shadows,
        "ortho_scale_m": camera.data.ortho_scale,
        "target_span_pixels": pixels,
        "world_strength": 0.0,
        "shadow": shadow,
        "scope": "Matched EEVEE binding diagnostic, not a physical transport oracle.",
    }
