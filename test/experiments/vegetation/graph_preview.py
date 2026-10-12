"""Render complete generator-graph products with physical instance-scale materials."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).parent))
from graph_material import apply_graph_bark
from organ_preview import populate, setup_scene
from preview_shadows import configure_preview


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
    implementation = [
        Path(__file__).with_name(name)
        for name in (
            "graph_preview.py",
            "graph_material.py",
            "bark_material.py",
            "organ_preview.py",
            "blender_preview.py",
            "sheet_material.py",
            "preview_lighting.py",
            "preview_shadows.py",
        )
    ]
    result = {
        "blender": bpy.app.version_string,
        "engine": scene.render.engine,
        "resolution": recipe["resolution"],
        "samples": recipe["samples"],
        "shadow_terminator": shadow,
        "objects": instances,
        "shared_meshes": len(recipe["products"]),
        "triangles_instanced": triangles,
        "scenario": str(args.scenario),
        "camera": recipe["camera"],
        "render_s": time.perf_counter() - began,
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [args.scenario, *paths]
        },
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in implementation
        },
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_modified": recipe["source_modified"]
        or bool(
            subprocess.check_output(
                ["git", "status", "--porcelain", "--", *map(str, implementation)],
                text=True,
            ).strip()
        ),
        "plan_commit": recipe["source_commit"],
        "plan_identity": recipe["plan_identity"],
        "plan_source_modified": recipe["source_modified"],
        "foliage_lod_quality_proven": False,
    }
    args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {key: result[key] for key in ("objects", "triangles_instanced", "render_s")}
        )
    )


if __name__ == "__main__":
    main()
