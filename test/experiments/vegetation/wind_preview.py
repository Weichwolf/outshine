"""Render wind products with stable per-leaf material variation after deformation."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from graph_material import apply_graph_bark
from organ_preview import populate, setup_scene
from preview_shadows import configure_preview


def leaf_variation(recipe, directory):
    for product in recipe["products"]:
        if product["surface"] != "leaf":
            continue
        path = directory / product["mesh"]
        mesh = bpy.data.objects[path.stem].data
        with np.load(path) as source:
            values = source["wind_leaf_variation"]
        attribute = mesh.attributes.new("wind_leaf_variation", "FLOAT", "POINT")
        attribute.data.foreach_set("value", values)
        for material in mesh.materials:
            nodes, links = material.node_tree.nodes, material.node_tree.links
            field = nodes.new("ShaderNodeAttribute")
            field.attribute_name = "wind_leaf_variation"
            for link in list(links):
                if (
                    link.from_node.type == "OBJECT_INFO"
                    and link.from_socket.name == "Random"
                ):
                    target = link.to_socket
                    links.remove(link)
                    links.new(field.outputs["Fac"], target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    recipe = json.loads(args.scenario.read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    paths, triangles, instances = populate(recipe, args.scenario.parent)
    paths.extend(apply_graph_bark(recipe, args.scenario.parent))
    leaf_variation(recipe, args.scenario.parent)
    setup_scene(recipe)
    scene = bpy.context.scene
    shadow = configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.output.resolve())
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    names = (
        "wind_preview.py",
        "graph_material.py",
        "bark_material.py",
        "organ_preview.py",
        "blender_preview.py",
        "sheet_material.py",
        "preview_lighting.py",
        "preview_shadows.py",
    )
    if any(
        "layered-bark-preview-1" in p.read_text() for p in paths if p.suffix == ".json"
    ):
        names += ("layered_bark_material.py",)
    report = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "scenario": str(args.scenario),
        "render_s": time.perf_counter() - began,
        "objects": instances,
        "triangles_instanced": triangles,
        "shadow_terminator": shadow,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": True,
        "plan_identity": recipe["plan_identity"],
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.scenario, *paths]
        },
        "implementation_sha256": {
            n: hashlib.sha256(Path(__file__).with_name(n).read_bytes()).hexdigest()
            for n in names
        },
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: report[k] for k in ("objects", "triangles_instanced", "render_s")}
        )
    )


if __name__ == "__main__":
    main()
