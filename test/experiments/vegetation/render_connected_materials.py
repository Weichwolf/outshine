"""Original qualitative PBR material versus two explicit filtered connected proxies."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import bpy
import numpy as np
from connected_material_fields import field_identity, validate_fields
from connected_material_shader import connected_material, field_image
from connected_reader import read_host
from fern_material import attach_normals, leaf_material
from mathutils import Vector
from outshine_ref.rendering.blender_camera import configure_camera
from outshine_ref.rendering.blender_mesh import mesh_object
from preview_shadows import configure_preview


def read_materials(folder, study, variants):
    products = {}
    for variant in np.unique(variants):
        variant = int(variant)
        description = json.loads((folder / f"material-{variant}.json").read_text())
        carrier = json.loads(
            (study / "connected32" / f"prototype-{variant}.json").read_text()
        )
        if description["carrier_identity"] != carrier["identity"]:
            raise ValueError("material fields belong to a different saved carrier")
        with np.load(folder / f"material-{variant}.npz", allow_pickle=False) as z:
            arrays = dict(z)
        validate_fields(arrays)
        if field_identity(arrays, description) != description["identity"]:
            raise ValueError("finished material field content differs")
        products[variant] = arrays
    return products


def subjects(source, surfaces, arrays, original, direction):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    template = leaf_material(original / "spatial")
    points = np.vstack((source["vertices"], *[s["mesh"]["vertices"] for s in surfaces]))
    centre = (points.max(axis=0) + points.min(axis=0)) / 2
    distance = float(np.linalg.norm(np.ptp(points, axis=0))) * 1.15
    side = np.cross((0, 0, 1), direction)
    side /= np.linalg.norm(side)
    uv = np.column_stack(
        ((source["parameters"][:, 1] + 1) / 2, source["parameters"][:, 0])
    )
    obj = mesh_object(
        "original material host",
        source["vertices"] - distance * side,
        source["faces"],
        source["normals"],
        template,
        True,
        uv=uv,
    )
    attach_normals(obj, source["normals"])
    images = {
        v: {k: field_image(f"variant{v} {k}", a) for k, a in data.items()}
        for v, data in arrays.items()
    }
    for broaden in (False, True):
        offset = distance * side if broaden else np.zeros(3)
        for index, (surface, variant) in enumerate(
            zip(surfaces, source["leaf_variants"], strict=True)
        ):
            result = connected_material(
                template, images[int(variant)], surface["basis"], broaden=broaden
            )
            m = surface["mesh"]
            mesh_object(
                f"{'variance' if broaden else 'mean'} leaf{index}",
                m["vertices"] + offset,
                m["faces"],
                m["normals"],
                result,
                True,
                uv=m["uv"],
            )
    return centre, distance


def environment(direction, centre, distance, *, light):
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = 32
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": (centre + 3 * direction).tolist(),
            "target": centre.tolist(),
            "span_m": distance * 3.3,
        }
    )
    scene.camera.data.clip_start, scene.camera.data.clip_end = 0.01, 10
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.view_settings.view_transform = "Standard"
    scene.world = bpy.data.worlds.new("dark neutral comparison world")
    scene.world.use_nodes = True
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = (
        0.07,
        0.07,
        0.07,
        1,
    )
    scene.world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.2
    bpy.ops.object.light_add(type="SUN")
    sun = bpy.context.object
    sun.rotation_euler = Vector(-light).to_track_quat("-Z", "Y").to_euler()
    sun.data.energy, sun.data.angle = 4.0, 0.05
    sun.data.use_shadow = False
    return scene


def render(study, fields, original, output, node):
    from prepare_connected_materials import original_material

    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    _, _, identity, _ = original_material(original, study)
    report = json.loads((fields / "report.json").read_text())
    if report["material_identity"] != identity:
        raise ValueError("finished fields do not bind the original material")
    source, surfaces = read_host(study, node)
    arrays = read_materials(fields, study, source["leaf_variants"])
    output.mkdir(parents=True, exist_ok=False)
    records = []
    start = time.perf_counter()
    for name, direction in (
        ("oblique", (np.cos(1.8), np.sin(1.8), 0.8)),
        ("grazing", (np.cos(0.4), np.sin(0.4), 0.05)),
    ):
        direction = np.asarray(direction, float)
        direction /= np.linalg.norm(direction)
        for state, sign in (("front", 1), ("back", -1)):
            centre, distance = subjects(source, surfaces, arrays, original, direction)
            light = sign * np.array((0.8, -0.3, 0.45))
            light /= np.linalg.norm(light)
            scene = environment(direction, centre, distance, light=light)
            path = output / f"{name}-{state}.png"
            scene.render.filepath = str(path)
            before = time.perf_counter()
            bpy.ops.render.render(write_still=True)
            records.append(
                {
                    "view": name,
                    "light": state,
                    "unit_light": light.tolist(),
                    "camera_direction": direction.tolist(),
                    "span_m": distance * 3.3,
                    "render_s": time.perf_counter() - before,
                }
            )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "host": node,
        "resolution": [2560, 1440],
        "samples": 32,
        "blender": bpy.app.version_string,
        "material_identity": identity,
        "cases": records,
        "wall_s": time.perf_counter() - start,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Left original qualitative fern colour/height/Principled roughness.55/specular.2 +.12 Translucent adapter; middle conditional mean slopes; right isotropic slope-variance roughness proxy. Sun shadows disabled to isolate material response. Same original host/poses and carrier geometry, no botanical optical/GGX-convolution/whole-crown/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("study", "fields", "original", "output"):
        p.add_argument("--" + key, type=Path, required=True)
    p.add_argument("--host", type=int, default=17)
    a = p.parse_args()
    render(a.study, a.fields, a.original, a.output, a.host)
