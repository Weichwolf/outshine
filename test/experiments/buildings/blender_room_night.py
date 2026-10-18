"""Explicit local night diagnostic; no change to the common daylight profile."""

import bpy
import numpy as np
from open_floor_lights import light_state

SHADOW_POOL_MB = 1024


def configure(scene, report, daylight):
    plan = report.get("room_light_plan")
    if not plan:
        raise ValueError("Room-night diagnostic requires its source-bound ceiling plan")
    state = light_state(plan, report["room_light_state"])
    maximum_position_error = 0.0
    scene.view_settings.view_transform = daylight["view_transform"]
    scene.view_settings.exposure = daylight["exposure"]
    scene.eevee.shadow_pool_size = str(SHADOW_POOL_MB)
    for obj in scene.objects:
        if obj.type == "LIGHT":
            obj.data.energy = 0
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.003
    for record in state:
        if not record["enabled"]:
            continue
        light = bpy.data.lights.new(record["fixture_id"], "AREA")
        light.shape = "RECTANGLE"
        light.normalize, light.exposure = True, 0
        light.size, light.size_y = record["size"]
        light.energy, light.color = record["power_w"], record["colour_linear"]
        obj = bpy.data.objects.new(record["fixture_id"], light)
        obj.location = record["position"]
        actual = np.array(obj.location[:], dtype=np.float32)
        expected = np.asarray(record["position"])
        if not np.array_equal(actual, expected.astype(np.float32)):
            raise ValueError("Blender changed the declared fixture position")
        maximum_position_error = max(
            maximum_position_error, float(abs(actual - expected).max())
        )
        scene.collection.objects.link(obj)
    return {
        "profile": "buildings-open-floor-night-diagnostic-v1",
        "daylight_basis": daylight,
        "sun_energy": 0,
        "world_strength": 0.003,
        "world_linear_rgb": daylight["world_linear_rgb"],
        "view_transform": daylight["view_transform"],
        "exposure": daylight["exposure"],
        "shadow_pool_mb": int(scene.eevee.shadow_pool_size),
        "shadow_resolution_scale": scene.eevee.shadow_resolution_scale,
        "light_unit": "Blender radiant W; normalize=True, light exposure=0; not electrical W or calibrated lumens",
        "state": report["room_light_state"],
        "fixtures": state,
        "enabled_lights": sum(r["enabled"] for r in state),
        "fixture_position_float32_exact": True,
        "maximum_fixture_position_rounding_m": maximum_position_error,
        "scope": "Declared procedural Blender area-light power/colour, not calibrated photometry; emitters are light objects without visible fixture meshes. Actual open floors and shared room states; no new room partition, GI/night/native/device admission.",
    }
