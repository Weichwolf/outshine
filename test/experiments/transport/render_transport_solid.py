"""Blender Workbench inspection of exact, hash-checked transport bodies."""

import hashlib
import json
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector

COLOURS = (
    (0.32, 0.38, 0.38, 1.0),
    (0.23, 0.51, 0.41, 1.0),
    (0.61, 0.65, 0.62, 1.0),
    (0.12, 0.43, 0.65, 1.0),
    (0.9, 0.29, 0.1, 1.0),
    (0.52, 0.58, 0.39, 1.0),
)


def render(configuration):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    path = Path(configuration["mesh"])
    if hashlib.sha256(path.read_bytes()).hexdigest() != configuration["mesh_sha256"]:
        raise ValueError("render input changed")
    with np.load(path) as data:
        vertices, faces, slots = data["vertices"], data["triangles"], data["materials"]
    geometry = bpy.data.meshes.new(configuration["id"])
    geometry.from_pydata(vertices.tolist(), [], faces.tolist())
    geometry.update()
    body = bpy.data.objects.new(configuration["id"], geometry)
    bpy.context.collection.objects.link(body)
    for i, colour in enumerate(configuration.get("colours", COLOURS)):
        material = bpy.data.materials.new(f"body-{i}")
        material.diffuse_color = colour
        geometry.materials.append(material)
    geometry.polygons.foreach_set("material_index", slots.astype(np.int32))
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    shading = scene.display.shading
    shading.light = "STUDIO"
    shading.color_type = "MATERIAL"
    shading.show_shadows = True
    shading.show_cavity = True
    shading.cavity_type = "BOTH"
    shading.show_specular_highlight = False
    shading.background_type = "WORLD"
    scene.world = bpy.data.worlds.new("inspection")
    scene.world.color = (0.86, 0.87, 0.81)
    scene.view_settings.view_transform = "Standard"
    bpy.ops.object.camera_add(location=configuration["eye"])
    camera = bpy.context.object
    camera.rotation_euler = (
        (Vector(configuration["target"]) - camera.location)
        .to_track_quat("-Z", "Y")
        .to_euler()
    )
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = configuration["span_m"]
    camera.data.clip_end = 10000
    scene.camera = camera
    scene.render.resolution_x, scene.render.resolution_y = configuration["profile"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = configuration["output"]
    scene.render.film_transparent = False
    bpy.context.view_layer.update()
    projection = np.asarray(
        camera.calc_matrix_camera(
            bpy.context.evaluated_depsgraph_get(),
            x=scene.render.resolution_x,
            y=scene.render.resolution_y,
        )
    )
    view = np.asarray(camera.matrix_world.inverted())
    clip = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    if np.any(np.abs(clip[:, :2] / clip[:, 3:4]) > 1.00001):
        raise ValueError("full model is outside the inspection camera")
    bpy.ops.render.render(write_still=True)
    print(
        json.dumps(
            {
                "id": configuration["id"],
                "renderer": bpy.app.version_string,
                "engine": scene.render.engine,
                "profile": configuration["profile"],
                "imported_vertices": len(vertices),
                "imported_triangles": len(faces),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    configurations = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    for configuration in configurations:
        render(configuration)
