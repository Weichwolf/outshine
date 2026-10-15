"""Blender Workbench inspection of exact, hash-checked transport bodies."""

import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector

COLOURS = (
    (0.32, 0.38, 0.38, 1.0),
    (0.23, 0.51, 0.41, 1.0),
    (0.61, 0.65, 0.62, 1.0),
    (0.12, 0.43, 0.65, 1.0),
    (0.9, 0.29, 0.1, 1.0),
    (0.52, 0.58, 0.39, 1.0),
)


def configure_camera(configuration):
    bpy.ops.object.camera_add(location=configuration["eye"])
    camera = bpy.context.object
    if "camera_basis" in configuration:
        camera.rotation_euler = Matrix(configuration["camera_basis"]).to_euler()
    else:
        camera.rotation_euler = (
            (Vector(configuration["target"]) - camera.location)
            .to_track_quat("-Z", "Y")
            .to_euler()
        )
    if configuration.get("projection") == "perspective":
        camera.data.type = "PERSP"
        camera.data.sensor_fit = "VERTICAL"
        camera.data.sensor_height = 32.0
        camera.data.lens = 16.0 / math.tan(
            math.radians(configuration["vertical_fov_deg"]) / 2
        )
        camera.data.clip_start = configuration["clip_near_m"]
        camera.data.clip_end = configuration["clip_far_m"]
    else:
        camera.data.type = "ORTHO"
        camera.data.ortho_scale = configuration["span_m"]
        camera.data.clip_end = 10000
    return camera


def verify_place_camera(configuration, projection, view, vertices):
    eye = np.asarray(configuration["eye"])
    forward = np.asarray(configuration["target"]) - eye
    forward /= np.linalg.norm(forward)
    if "camera_basis" in configuration:
        up = np.asarray(configuration["camera_basis"])[:, 1]
    else:
        right = np.cross(forward, [0.0, 0.0, 1.0])
        right /= np.linalg.norm(right)
        up = np.cross(right, forward)
    extent = math.tan(math.radians(configuration["vertical_fov_deg"]) / 2)
    probes = eye + 1000 * np.array(
        [forward, forward + up * extent, forward - up * extent]
    )
    clip = np.c_[probes, np.ones(3)] @ view.T @ projection.T
    projected = clip[:, :2] / clip[:, 3:4]
    if not np.allclose(
        projected, [[0.0, 0.0], [0.0, 1.0], [0.0, -1.0]], rtol=0, atol=2e-5
    ):
        raise ValueError(
            "Blender camera disagrees with declared Place pose/vertical FOV"
        )
    positions = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    visible = (positions[:, 3] > 0) & np.all(
        np.abs(positions[:, :2]) <= positions[:, 3:4], axis=1
    )
    if not visible.any():
        raise ValueError(
            "no generated transport vertex is visible from the fixed Place camera"
        )
    proof = {
        "projection": configuration["projection"],
        "eye": configuration["eye"],
        "target": configuration["target"],
        "vertical_fov_deg": configuration["vertical_fov_deg"],
        "profile": configuration["profile"],
        "visible_input_vertices": int(visible.sum()),
        "vertical_ndc_probes": projected.tolist(),
        "projection_matrix": projection.tolist(),
        "view_matrix": view.tolist(),
    }
    path = Path(configuration["output"]).with_name(configuration["id"] + "-camera.json")
    path.write_text(json.dumps(proof, indent=2) + "\n")


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
    scene.world.color = configuration.get("background", (0.86, 0.87, 0.81))
    scene.view_settings.view_transform = "Standard"
    camera = configure_camera(configuration)
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
    if configuration.get("projection") == "perspective":
        verify_place_camera(configuration, projection, view, vertices)
    elif not configuration.get("detail_view", False) and np.any(
        np.abs(clip[:, :2] / clip[:, 3:4]) > 1.00001
    ):
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
