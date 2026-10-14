"""Controlled shader comparisons from complete cached procedural leaf products."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy
from leaf_shader_bindings import apply_reference_materials
from organ_preview import populate, setup_scene
from preview_shadows import configure_preview


def configure_material(obj, mode):
    if mode == "material":
        return
    material = obj.data.materials[0]
    nodes, links = material.node_tree.nodes, material.node_tree.links
    translucent = next(n for n in nodes if n.type == "BSDF_TRANSLUCENT")
    if mode == "reflection-only":
        for link in tuple(translucent.inputs["Color"].links):
            links.remove(link)
        translucent.inputs["Color"].default_value = (0, 0, 0, 1)
        return
    if mode != "diffuse-sheet":
        raise ValueError("declared leaf shader comparison mode required")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    diffuse.inputs["Roughness"].default_value = 0
    colour = nodes.get("Principled BSDF").inputs["Base Color"].links[0].from_socket
    links.new(colour, diffuse.inputs["Color"])
    mixed = next(
        n
        for n in nodes
        if n.type == "MIX_SHADER"
        and n.inputs[1].is_linked
        and n.inputs[1].links[0].from_node.type == "BSDF_PRINCIPLED"
    )
    links.new(diffuse.outputs[0], mixed.inputs[1])
    geometry = nodes.new("ShaderNodeNewGeometry")
    links.new(geometry.outputs["True Normal"], diffuse.inputs["Normal"])
    links.new(geometry.outputs["True Normal"], translucent.inputs["Normal"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index("--") + 1 :])
    recipe = json.loads(args.scenario.read_text())
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    paths, triangles, instances = populate(recipe, args.scenario.parent)
    paths.extend(apply_reference_materials(recipe, args.scenario.parent))
    for product in recipe["products"]:
        configure_material(
            bpy.data.objects[Path(product["mesh"]).stem], product["leaf_shader_mode"]
        )
    setup_scene(recipe)
    scene = bpy.context.scene
    configure_preview(scene, recipe)
    scene.eevee.taa_render_samples = recipe["samples"]
    scene.render.resolution_x, scene.render.resolution_y = recipe["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(args.output.resolve())
    began = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    root = Path(__file__).resolve().parents[3]
    names = (
        "leaf_shader_preview.py",
        "leaf_shader_bindings.py",
        "organ_preview.py",
        "blender_preview.py",
        "sheet_material.py",
        "preview_lighting.py",
        "preview_shadows.py",
    )
    report = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "scenario": str(args.scenario),
        "render_s": time.perf_counter() - began,
        "objects": instances,
        "triangles_instanced": triangles,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": bool(
            subprocess.check_output(["git", "status", "--porcelain"], text=True).strip()
        ),
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.scenario, *paths]
        },
        "implementation_sha256": {
            str(Path(__file__).with_name(n).relative_to(root)): hashlib.sha256(
                Path(__file__).with_name(n).read_bytes()
            ).hexdigest()
            for n in names
        },
    }
    for name in (
        "leaf_material.py",
        "leaf_optics.py",
        "leaf_normals.py",
        "material_arrays.py",
        "plan_arrays.py",
    ):
        path = root / "lib/src/outshine_ref/vegetation" / name
        report["implementation_sha256"][str(path.relative_to(root))] = hashlib.sha256(
            path.read_bytes()
        ).hexdigest()
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: report[k] for k in ("objects", "triangles_instanced", "render_s")}
        )
    )


if __name__ == "__main__":
    main()
