"""Blender Workbench inspection of exact, hash-checked transport bodies."""

import hashlib
import json
from pathlib import Path
import sys

import bpy
from mathutils import Vector
import numpy as np


COLOURS = ((.32, .38, .38, 1.), (.23, .51, .41, 1.), (.61, .65, .62, 1.), (.12, .43, .65, 1.),
           (.9, .29, .1, 1.), (.52, .58, .39, 1.))


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
    for i, colour in enumerate(COLOURS):
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
    scene.world.color = (.86, .87, .81)
    scene.view_settings.view_transform = "Standard"
    bpy.ops.object.camera_add(location=configuration["eye"])
    camera = bpy.context.object
    camera.rotation_euler = (Vector(configuration["target"]) - camera.location).to_track_quat("-Z", "Y").to_euler()
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
    projection = np.asarray(camera.calc_matrix_camera(bpy.context.evaluated_depsgraph_get(),
                                                     x=scene.render.resolution_x, y=scene.render.resolution_y))
    view = np.asarray(camera.matrix_world.inverted())
    clip = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    if np.any(np.abs(clip[:, :2] / clip[:, 3:4]) > 1.00001):
        raise ValueError("full model is outside the inspection camera")
    bpy.ops.render.render(write_still=True)
    print(json.dumps(dict(id=configuration["id"], renderer=bpy.app.version_string,
                          engine=scene.render.engine, profile=configuration["profile"],
                          imported_vertices=len(vertices), imported_triangles=len(faces))), flush=True)


if __name__ == "__main__":
    configurations = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    for configuration in configurations:
        render(configuration)
