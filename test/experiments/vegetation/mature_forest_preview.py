"""Render all original boreal sites using complete mature catalog products."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from blender_preview import mesh_object
from conifer_preview import protect_inputs
from conifer_whole_preview import conifer_material
from landscape_preview import setup_scene
from mathutils import Matrix
from mature_forest_plan import initial_views, load_plan
from preview_shadows import configure_preview


def add_model(model, sites, selected_views):
    mesh, manifest = model["mesh"], model["manifest"]
    vertices, triangles = manifest["vertices_per_view"], manifest["draw_triangles"]
    total, objects = 0, 0
    for role, views in selected_views.items():
        pass_role = "camera" if role == "camera" else "shadow"
        surface = conifer_material(
            f"{model['case']['key']}-{role}",
            model["directory"],
            manifest["textures"],
            pass_role,
        )
        for view in np.unique(views):
            selected = model["sites"][views == view]
            first, face_first = int(view) * vertices, int(view) * triangles
            faces = mesh["faces"][face_first : face_first + triangles] - first
            original = mesh_object(
                f"{model['case']['key']}-{role}-{int(view)}",
                mesh["vertices"][first : first + vertices],
                faces,
                mesh["normals"][face_first * 3 : (face_first + triangles) * 3],
                surface,
                corner_normals=True,
            )
            uv = mesh["uv"][first : first + vertices][faces.ravel()]
            original.data.uv_layers.new(name="complete plant atlas").data.foreach_set(
                "uv", uv.ravel()
            )
            for offset, index in enumerate(selected):
                obj = original if offset == 0 else original.copy()
                if offset:
                    obj.data = original.data
                    bpy.context.collection.objects.link(obj)
                angle = sites["angles"][index]
                c, s = np.cos(angle), np.sin(angle)
                transform = np.eye(4)
                transform[:3, :3] = ((c, -s, 0), (s, c, 0), (0, 0, 1))
                transform[:3, 3] = sites["positions"][index]
                obj.matrix_world = Matrix(transform.tolist())
                obj["vegetation_pass_role"] = pass_role
                obj["plant_plan_identity"] = manifest["plan_identity"]
                obj["stand_source_id"] = json.dumps(sites["ids"][index].tolist())
                if role != "camera":
                    obj["shadow_light"] = role
            total += len(selected) * triangles
            objects += len(selected)
    return total, objects


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    began = time.perf_counter()
    recipe, sites, models, inputs = load_plan(args.scenario, args.products)
    protect_inputs(args.output, inputs)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    triangles, objects = 0, 0
    for model, views in zip(models, initial_views(recipe, sites, models), strict=True):
        faces, count = add_model(model, sites, views)
        triangles += faces
        objects += count
    setup_scene(recipe)
    scene = bpy.context.scene
    shadows = configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.output.resolve())
    setup_s = time.perf_counter() - began
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    names = (
        "mature_forest_preview.py",
        "mature_forest_plan.py",
        "conifer_whole_preview.py",
        "landscape_preview.py",
        "landscape_experiment.py",
        "conifer_preview.py",
        "blender_preview.py",
        "blender_instances.py",
        "organ_preview.py",
        "forest_views.py",
        "sheet_material.py",
        "preview_shadows.py",
        "preview_lighting.py",
    )
    here = Path(__file__).parent
    result = {
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "engine": scene.render.engine,
        "blender": bpy.app.version_string,
        "source_sites": len(sites["ids"]),
        "species_counts": recipe["counts"],
        "complete_catalog_forms": len(models),
        "tree_triangles": int(triangles),
        "ground_triangles": 2 * 127**2,
        "tree_objects": int(objects),
        "setup_s": setup_s,
        "render_s": time.perf_counter() - began,
        "camera": recipe["camera"],
        "shadow_settings": shadows,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(
                [
                    "git",
                    "status",
                    "--porcelain",
                    "--",
                    *[str(here / name) for name in names],
                ],
                text=True,
            ).strip()
        ),
        "input_sha256": {
            str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
        },
        "implementation_sha256": {
            name: hashlib.sha256((here / name).read_bytes()).hexdigest()
            for name in names
        },
        "scope": "all original 350 sites, 160 m extent, size variants, ground/camera/sun/samples preserved; independent camera/sun views; own mature architectures, physical needle sizes; botanical/LAI/normal/material/transition/native and device approval open",
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in result.items() if not k.endswith("sha256")}, indent=2
        )
    )


if __name__ == "__main__":
    main()
