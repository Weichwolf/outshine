"""Eevee presentation of the exact compact-city Python research mesh, using shared camera/import helpers."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_city_material import base
from blender_city_mesh import import_mesh
from evidence import revision
from outshine_ref.rendering.blender_camera import configure_camera, verify_camera
from outshine_ref.rendering.blender_light import configure_daylight


def studio(scene, data, samples, ground_span_m=None):
    scene.render.engine = (
        "BLENDER_EEVEE" if bpy.app.version >= (5, 0, 0) else "BLENDER_EEVEE_NEXT"
    )
    scene.render.threads_mode, scene.render.threads = "FIXED", 2
    scene.eevee.taa_render_samples = samples
    lighting = configure_daylight(scene)
    low, high = data["vertices"][:, :2].min(axis=0), data["vertices"][:, :2].max(axis=0)
    centre, span = (low + high) / 2, float((high - low).max()) * 2
    if ground_span_m is not None:
        if not np.isfinite(ground_span_m) or ground_span_m < span:
            raise ValueError("Declared flat ground must contain the source scene")
        span = float(ground_span_m)
    bpy.ops.mesh.primitive_plane_add(size=span, location=(*centre, -0.01))
    bpy.context.object.data.materials.append(
        base("flat neutral ground", (0.20, 0.22, 0.24), 0.95)
    )
    return {
        **lighting,
        "extra_lights": [],
        "ground_z_m": -0.01,
        "ground_span_m": span,
    }


def render(geometry, output, samples):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Blender presentation requires a frozen clean implementation")
    report = json.loads(geometry.read_text())
    if report["experiment_tree_dirty"] or report["ground_mode"] != "flat-no-dem":
        raise ValueError("Clean actual flat city research geometry required")
    path = Path(report["mesh"])
    if hashlib.sha256(path.read_bytes()).hexdigest() != report["mesh_sha256"]:
        raise ValueError("City mesh no longer matches the source-generation proof")
    started = time.perf_counter()
    with np.load(path) as saved:
        data = dict(saved)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    obj, imported = import_mesh(report, data)
    scene = bpy.context.scene
    lighting = studio(scene, data, samples, report.get("ground_span_m"))
    output.mkdir(parents=True, exist_ok=True)
    image = output / (report["city"] + ".png")
    camera_config = report["camera"] | {
        "id": report["city"],
        "output": str(image.resolve()),
    }
    scene.camera = configure_camera(camera_config)
    scene.render.resolution_x, scene.render.resolution_y = camera_config["profile"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(image.resolve())
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
    camera = verify_camera(camera_config, projection, view, data["vertices"])
    if len(obj.data.polygons) != report["triangles"]:
        raise ValueError("Renderer omitted source building triangles")
    setup = time.perf_counter() - started
    render_start = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    elapsed = time.perf_counter() - render_start
    record = provenance | {
        "representation": "python-city-building-presentation",
        "city": report["city"],
        "geometry_commit": report["commit"],
        "geometry_report_sha256": hashlib.sha256(geometry.read_bytes()).hexdigest(),
        "geometry_report": str(geometry.resolve()),
        "mesh_sha256": report["mesh_sha256"],
        "ground_mode": "flat-no-dem",
        "camera": camera,
        "lighting": lighting,
        "profile": camera_config["profile"],
        "samples": samples,
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "backend": gpu.platform.renderer_get(),
        "vendor": gpu.platform.vendor_get(),
        "imported": imported,
        "setup_seconds": setup,
        "render_seconds": elapsed,
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "image": image.name,
        "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
        "scope": "Actual current Python mesh and supplied normals/UVs; single Principled parameter composition with procedural facade masks and Eevee sampling, no analytic CPU footprint-oracle or native/device/LOD admission",
    }
    (output / (report["city"] + "-render.json")).write_text(
        json.dumps(record, indent=2) + "\n"
    )
    print(
        report["city"],
        "rendered",
        imported["source_triangles"],
        "source triangles",
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("geometry", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=32)
    args = parser.parse_args()
    render(args.geometry, args.output, args.samples)
