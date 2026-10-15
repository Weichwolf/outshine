"""Present actual generated transport bodies through the shared Blender research helpers."""

import argparse
import inspect
import json
import resource
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from camera_height_inputs import digest
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from outshine_ref.rendering.blender_light import configure_daylight
from outshine_ref.rendering.blender_mesh import mesh_object
from transport_surface_colour import attach_face_colours

NORMAL_ATTRIBUTE = "transport_source_normal"


def materials(colours):
    result = []
    for index, colour in enumerate(colours):
        surface = bpy.data.materials.new(f"transport-{index}")
        surface.use_nodes = True
        surface.diffuse_color = colour
        shader = surface.node_tree.nodes.get("Principled BSDF")
        shader.inputs["Base Color"].default_value = colour
        shader.inputs["Roughness"].default_value = 0.86
        shader.inputs["Metallic"].default_value = 0
        attribute = surface.node_tree.nodes.new("ShaderNodeAttribute")
        attribute.attribute_name = NORMAL_ATTRIBUTE
        transform = surface.node_tree.nodes.new("ShaderNodeVectorTransform")
        transform.vector_type, transform.convert_from, transform.convert_to = (
            "NORMAL",
            "OBJECT",
            "WORLD",
        )
        normalize = surface.node_tree.nodes.new("ShaderNodeVectorMath")
        normalize.operation = "NORMALIZE"
        surface.node_tree.links.new(
            attribute.outputs["Vector"], transform.inputs["Vector"]
        )
        surface.node_tree.links.new(transform.outputs["Vector"], normalize.inputs[0])
        surface.node_tree.links.new(
            normalize.outputs["Vector"], shader.inputs["Normal"]
        )
        result.append(surface)
    return result


def import_body(row):
    path = Path(row["mesh"])
    if digest(path) != row["mesh_sha256"]:
        raise ValueError("source body changed")
    with np.load(path) as saved:
        vertices, faces, normals, slots = (
            saved[k] for k in ("vertices", "triangles", "normals", "materials")
        )
        colours = saved.get("face_base_colour_linear_rgba", None)
    surfaces = materials(row["colours"])
    obj = mesh_object(
        row["id"], vertices, faces, normals, surfaces[0], corner_normals=True
    )
    for surface in surfaces[1:]:
        obj.data.materials.append(surface)
    if slots.shape != (len(faces),) or slots.min() < 0 or slots.max() >= len(surfaces):
        raise ValueError("source material ownership invalid")
    obj.data.polygons.foreach_set("material_index", slots.astype(np.int32))
    imported = np.empty(len(faces) * 3, np.int32)
    obj.data.loops.foreach_get("vertex_index", imported)
    if not np.array_equal(imported.reshape(-1, 3), faces):
        raise ValueError("Blender changed source transport topology")
    actual = np.empty((len(vertices), 3), np.float32)
    obj.data.vertices.foreach_get("co", actual.ravel())
    if not np.array_equal(actual, vertices.astype(np.float32)):
        raise ValueError("Blender changed source transport positions")
    # Blender's packed custom-normal spaces lose accuracy on the real shared
    # indexed bodies. Preserve the supplied vectors directly for the material.
    packed = np.empty_like(normals, dtype=np.float32)
    obj.data.corner_normals.foreach_get("vector", packed.ravel())
    attribute = obj.data.attributes.new(NORMAL_ATTRIBUTE, "FLOAT_VECTOR", "CORNER")
    attribute.data.foreach_set("vector", normals.astype(np.float32).ravel())
    actual_normals = np.empty_like(normals, dtype=np.float32)
    attribute.data.foreach_get("vector", actual_normals.ravel())
    if not np.array_equal(actual_normals, normals.astype(np.float32)):
        raise ValueError("Blender changed explicit source normal attribute")
    colour_proof = {} if colours is None else attach_face_colours(obj, colours)
    return vertices, {
        "vertices": len(vertices),
        "triangles": len(faces),
        "supplied_corner_normals": len(normals),
        "topology_exact": True,
        "positions_float32_exact": True,
        "source_normal_attribute_float32_exact": True,
        "source_normal_attribute": NORMAL_ATTRIBUTE,
        "unused_packed_normal_max_error": float(abs(packed - normals).max()),
        **colour_proof,
    }


def render(row):
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/transport"],
        text=True,
    ).strip():
        raise ValueError("research presentation requires a clean frozen source")
    output = Path(row["output"])
    if output.exists():
        raise ValueError("fresh render output required")
    began = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    vertices, imported = import_body(row)
    scene = bpy.context.scene
    scene.render.engine = (
        "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
    )
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    scene.eevee.taa_render_samples = row.get("samples", 16)
    lighting = configure_daylight(scene)
    scene.camera = configure_camera(row)
    scene.render.resolution_x, scene.render.resolution_y = row["profile"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(output)
    scene.render.film_transparent = False
    bpy.context.view_layer.update()
    projection = np.asarray(
        scene.camera.calc_matrix_camera(
            bpy.context.evaluated_depsgraph_get(),
            x=scene.render.resolution_x,
            y=scene.render.resolution_y,
        )
    )
    view = np.asarray(scene.camera.matrix_world.inverted())
    camera = verify_camera(row, projection, view, vertices)
    bpy.ops.render.render(write_still=True)
    report = {
        "id": row["id"],
        "source_mesh_sha256": row["mesh_sha256"],
        "scene_config_sha256": row["scene_config_sha256"],
        "image_sha256": digest(output),
        "camera": camera,
        "imported": imported,
        "lighting": lighting,
        "samples": scene.eevee.taa_render_samples,
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "backend": gpu.platform.renderer_get(),
        "vendor": gpu.platform.vendor_get(),
        "helper_files": {
            f.__name__: {
                "module": f.__module__,
                "path": inspect.getfile(f),
                "sha256": digest(inspect.getfile(f)),
            }
            for f in (configure_camera, verify_camera, configure_daylight, mesh_object)
        },
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
    }
    output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "id": row["id"],
                "seconds": report["seconds"],
                "backend": report["backend"],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("scene", type=Path)
    args = parser.parse_args()
    render(
        dict(json.loads(args.scene.read_text()), scene_config_sha256=digest(args.scene))
    )
