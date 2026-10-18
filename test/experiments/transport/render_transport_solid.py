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
    (0.64, 0.67, 0.70, 1.0),
    (0.32, 0.27, 0.21, 1.0),
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


def imported_body(configuration, item, name, render_origin):
    path = Path(item["mesh"])
    if hashlib.sha256(path.read_bytes()).hexdigest() != item["mesh_sha256"]:
        raise ValueError("render input changed")
    with np.load(path) as data:
        vertices, faces, slots = data["vertices"], data["triangles"], data["materials"]
    origin = np.asarray(item.get("origin_m", [0, 0, 0]), float)
    if origin.shape != (3,) or not np.isfinite(origin).all():
        raise ValueError("finite explicit mesh origin required")
    if item.get("encoded_float32") and vertices.dtype != np.dtype("f4"):
        raise ValueError("actual encoded Float32 mesh payload required")
    geometry = bpy.data.meshes.new(name)
    geometry.from_pydata(vertices.tolist(), [], faces.tolist())
    geometry.update()
    body = bpy.data.objects.new(name, geometry)
    bpy.context.collection.objects.link(body)
    delta = origin - render_origin
    body.location = delta.tolist()
    if not np.array_equal(np.asarray(body.location), delta.astype("f4")):
        raise ValueError(
            "Blender mesh origin differs from Double subtraction before cast"
        )
    payload_sha256 = None
    if item.get("encoded_float32") or "render_origin_m" in configuration:
        imported = np.empty(vertices.size, dtype="f4")
        geometry.vertices.foreach_get("co", imported)
        if not np.array_equal(
            imported.reshape(vertices.shape).view("u4"),
            vertices.astype("f4").view("u4"),
        ):
            raise ValueError("Blender changed actual encoded vertex payload")
        payload_sha256 = hashlib.sha256(imported.tobytes()).hexdigest()
    for i, colour in enumerate(configuration.get("colours", COLOURS)):
        material = bpy.data.materials.new(f"body-{i}")
        material.diffuse_color = colour
        geometry.materials.append(material)
    geometry.polygons.foreach_set("material_index", slots.astype(np.int32))
    binding = {
        "input_mesh_sha256": item["mesh_sha256"],
        "origin_m": origin.tolist(),
        "origin_minus_render_origin_f32_m": np.asarray(body.location).tolist(),
        "actual_vertex_payload_sha256": payload_sha256,
    }
    return vertices.astype(float) + delta, len(faces), body, binding


def render(configuration):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    render_origin = np.asarray(configuration.get("render_origin_m", [0, 0, 0]), float)
    if render_origin.shape != (3,) or not np.isfinite(render_origin).all():
        raise ValueError("finite explicit render origin required")
    camera_configuration = dict(configuration)
    for key in ("eye", "target"):
        camera_configuration[key] = (
            np.asarray(configuration[key], float) - render_origin
        ).tolist()
    objects = configuration.get("objects", [configuration])
    if not 1 <= len(objects) <= 256:
        raise ValueError("bounded render object list required")
    imported = [
        imported_body(
            configuration,
            item,
            configuration["id"]
            if len(objects) == 1
            else configuration["id"] + "-" + str(i),
            render_origin,
        )
        for i, item in enumerate(objects)
    ]
    vertices = np.vstack([item[0] for item in imported])
    face_count = sum(item[1] for item in imported)
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
    camera = configure_camera(camera_configuration)
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
    if "render_origin_m" in configuration:
        bindings = [
            {
                **item[3],
                "actual_model_matrix_f32": np.asarray(item[2].matrix_world).tolist(),
            }
            for item in imported
        ]
        proof = {
            "render_origin_m": render_origin.tolist(),
            "input_eye": configuration["eye"],
            "input_target": configuration["target"],
            "actual_camera_translation_f32_m": np.asarray(camera.location).tolist(),
            "actual_view_matrix_f32": view.tolist(),
            "actual_projection_matrix_f32": projection.tolist(),
            "objects": bindings,
            "scope": "Actual Blender CPU mesh/model/camera bindings before Workbench rendering; no native backend or vertex-shader capture.",
        }
        Path(configuration["output"]).with_suffix(".bindings.json").write_text(
            json.dumps(proof, indent=2) + "\n"
        )
    clip = np.c_[vertices, np.ones(len(vertices))] @ view.T @ projection.T
    if configuration.get("projection") == "perspective":
        verify_place_camera(camera_configuration, projection, view, vertices)
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
                "imported_triangles": face_count,
                **(
                    {"render_origin_m": render_origin.tolist(), "objects": len(objects)}
                    if "render_origin_m" in configuration
                    else {}
                ),
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    configurations = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    for configuration in configurations:
        render(configuration)
