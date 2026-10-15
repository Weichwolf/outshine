"""Render complete cached conifer views over the unchanged physical tile."""

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
from conifer_preview import populate, protect_inputs, setup_scene_lights
from forest_views import choices
from mathutils import Matrix
from preview_lighting import SUN_ROTATION
from preview_shadows import configure_preview
from scipy.spatial.transform import Rotation
from sheet_material import sheet_material


def conifer_material(name, atlas, textures, role):
    result = sheet_material(name, atlas, textures, True, role)
    nodes = result.node_tree.nodes
    shader = nodes.get("Principled BSDF")
    roughness = shader.inputs["Roughness"].links[0].from_node
    if roughness.operation != "MULTIPLY_ADD":
        raise ValueError("expected explicit leaf-weight roughness control")
    roughness.inputs[1].default_value = 0.59 - 0.8
    for node in nodes:
        if node.type != "MIX_SHADER" or not node.inputs[1].is_linked:
            continue
        if node.inputs[1].links[0].from_node != shader:
            continue
        transmission = node.inputs[0].links[0].from_node
        if transmission.operation != "MULTIPLY":
            raise ValueError("expected explicit leaf-weight transmission control")
        transmission.inputs[1].default_value = 0.08
    return result


def add_trees(recipe, tile_directory, whole):
    plans_path = tile_directory / recipe["plant_recipe"]
    plans = json.loads(plans_path.read_text())["plants"]
    inputs, triangles = [plans_path], 0
    sun = Rotation.from_euler("xyz", SUN_ROTATION).apply((0, 0, 1))
    for plan in plans:
        species = plan["species"]["latin"].lower().replace(" ", "-")
        atlas = whole / species
        manifest_path = atlas / "plant-product.json"
        manifest = json.loads(manifest_path.read_text())
        if manifest["plan_identity"] != plan["identity"]:
            raise ValueError("whole product must retain the physical plant plan")
        path = atlas / manifest["mesh"]
        inputs.extend((manifest_path, path, atlas / manifest["recipe"]))
        inputs.extend(atlas / value for value in manifest["textures"].values())
        with np.load(path, allow_pickle=False) as stored:
            mesh = {key: stored[key] for key in stored.files}
        origin, frame = np.array(plan["origin_m"]), np.array(plan["frame"])
        angles = np.array([np.arctan2(frame[1, 0], frame[0, 0])])
        pivot = np.mean(manifest["bounds_m"], axis=0)
        queries = {
            "camera": {"eye": recipe["camera"]},
            "sun": {"direction": sun},
            "fill": {"eye": recipe["fill_position_m"]},
        }
        for role, query in queries.items():
            index = int(
                choices(mesh, origin[None], np.ones((1, 3)), angles, pivot, **query)[0]
            )
            v, t = manifest["vertices_per_view"], manifest["draw_triangles"]
            faces = mesh["faces"][index * t : (index + 1) * t] - index * v
            surface = conifer_material(
                f"{species}-{role}",
                atlas,
                manifest["textures"],
                "camera" if role == "camera" else "shadow",
            )
            obj = mesh_object(
                f"{species}-{role}",
                mesh["vertices"][index * v : (index + 1) * v],
                faces,
                mesh["normals"][index * t * 3 : (index + 1) * t * 3],
                surface,
                corner_normals=True,
            )
            uv = mesh["uv"][index * v : (index + 1) * v][faces.ravel()]
            obj.data.uv_layers.new(name="whole plant atlas").data.foreach_set(
                "uv", uv.ravel()
            )
            transform = np.eye(4)
            transform[:3, :3] = frame
            transform[:3, 3] = origin
            obj.matrix_world = Matrix(transform.tolist())
            obj["vegetation_pass_role"] = "camera" if role == "camera" else "shadow"
            if role != "camera":
                obj["shadow_light"] = role
            triangles += t
    return inputs, triangles


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--whole", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    recipe = json.loads(args.scenario.read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    began = time.perf_counter()
    environment = {
        "products": [
            p for p in recipe["products"] if p["surface"] in ("grass", "ground")
        ]
    }
    inputs, triangles, instances = populate(environment, args.scenario.parent)
    tree_inputs, tree_triangles = add_trees(recipe, args.scenario.parent, args.whole)
    inputs.extend((args.scenario, *tree_inputs))
    protect_inputs(args.output, inputs)
    setup_scene_lights(recipe)
    scene = bpy.context.scene
    shadows = configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(args.output.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    setup_s = time.perf_counter() - began
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    code_names = (
        "conifer_whole_preview.py",
        "conifer_preview.py",
        "forest_views.py",
        "sheet_material.py",
        "blender_preview.py",
        "blender_instances.py",
        "organ_preview.py",
        "preview_lighting.py",
        "preview_shadows.py",
        "needle_material.py",
        "bark_material.py",
        "layered_bark_material.py",
    )
    result = {
        "engine": scene.render.engine,
        "blender": bpy.app.version_string,
        "resolution": recipe["resolution"],
        "triangles": triangles + tree_triangles,
        "tree_triangles": tree_triangles,
        "setup_s": setup_s,
        "render_s": time.perf_counter() - began,
        "shadow_settings": shadows,
        "samples": recipe["samples"],
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
                    *[str(Path(__file__).with_name(n)) for n in code_names],
                ],
                text=True,
            ).strip()
        ),
        "input_sha256": {
            str(p.resolve()): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs
        },
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in code_names
        },
        "scope": "same complete roots/ground/grass/camera/lights; statistical shoot windows, normal/material/tail and LOD image approval open",
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {k: v for k, v in result.items() if not k.endswith("sha256")}, indent=2
        )
    )


if __name__ == "__main__":
    main()
