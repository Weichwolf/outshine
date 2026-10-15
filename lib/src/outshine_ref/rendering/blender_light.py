"""Explicit common research daylight; callers retain ownership of other lights and materials."""


def configure_daylight(scene):
    import bpy

    if scene.render.engine not in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        raise ValueError("Research daylight presentation requires the Eevee profile")
    settings = {
        "profile": "research-daylight-v1",
        "sun_rotation_radians": [0.45, -0.65, -0.6],
        "sun_energy": 2.5,
        "sun_angle_radians": 0.04,
        "world_linear_rgb": [0.35, 0.45, 0.6],
        "world_strength": 0.35,
        "view_transform": "AgX",
        "exposure": 1.5,
    }
    scene.view_settings.view_transform = settings["view_transform"]
    scene.view_settings.exposure = settings["exposure"]
    light = bpy.data.lights.new("research sun", "SUN")
    light.energy, light.angle = settings["sun_energy"], settings["sun_angle_radians"]
    obj = bpy.data.objects.new("research sun", light)
    obj.rotation_euler = settings["sun_rotation_radians"]
    scene.collection.objects.link(obj)
    scene.world = bpy.data.worlds.new("neutral research studio")
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (*settings["world_linear_rgb"], 1.0)
    background.inputs["Strength"].default_value = settings["world_strength"]
    return settings
