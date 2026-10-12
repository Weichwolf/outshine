"""Explicit EEVEE settings for smooth vegetation surfaces and their self shadows."""

import math

import bpy


def configure_preview(scene, recipe=None):
    recipe = recipe or {}
    normal = float(recipe.get("shadow_terminator_normal_offset", 0.0))
    geometry = float(recipe.get("shadow_terminator_geometry_offset", 0.1))
    if not all(math.isfinite(x) and 0 <= x <= 1 for x in (normal, geometry)):
        raise ValueError(
            "finite shadow terminator offsets between zero and one required"
        )
    scene.render.engine = (
        "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
    )
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    count = 0
    for obj in scene.objects:
        if obj.type != "MESH":
            continue
        if not hasattr(obj, "shadow_terminator_normal_offset"):
            raise RuntimeError("shadow-safe previews require .venv bpy 5.2.2")
        obj.shadow_terminator_normal_offset = normal
        obj.shadow_terminator_geometry_offset = geometry
        count += 1
    return {"normal_offset": normal, "geometry_offset": geometry, "objects": count}
