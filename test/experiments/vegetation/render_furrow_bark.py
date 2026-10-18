"""Same bent tube and studio, cached furrow maps against previous cell treatment."""

import argparse
import json
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from demand import Camera
from furrow_material import cached_material
from limb_bark import mature_recipe
from limb_bark_material import mature_material
from outshine_ref.rendering.blender_mesh import mesh_object
from outshine_ref.vegetation.axis_surface import axis_mesh
from render_shoot_forms import environment
from shoot_form_scene import constant_material


def tube(surface, width, height):
    sides, segments = 128, 96
    mesh = axis_mesh(
        height,
        0.15,
        width / (2 * np.pi),
        width / (2 * np.pi),
        segments=segments,
        sides=sides,
        bark=True,
    )
    rotation = np.array(((1, 0, 0), (0, 0, -1), (0, 1, 0)))
    obj = mesh_object(
        "same metric bent bark specimen",
        mesh["vertices"] @ rotation.T,
        mesh["faces"],
        mesh["normals"] @ rotation.T,
        surface,
        True,
    )
    for name, kind, field in (
        ("bark_coordinates", "FLOAT_VECTOR", "vector"),
        ("bark_radius", "FLOAT", "value"),
    ):
        attribute = obj.data.attributes.new(name, kind, "POINT")
        attribute.data.foreach_set(field, mesh[name].astype(np.float32).ravel())
    uv = np.zeros((len(mesh["vertices"]), 2))
    uv[:-2, 0] = np.tile(np.arange(sides) / sides, segments + 1)
    uv[:-2, 1] = np.repeat(np.linspace(0, 1, segments + 1), sides)
    corners = uv[mesh["faces"]]
    wrap = np.ptp(corners[:, :, 0], axis=1) > 0.5
    corners[wrap, :, 0] += corners[wrap, :, 0] < 0.5
    layer = obj.data.uv_layers.new(name="periodic metric bark specimen")
    layer.data.foreach_set("uv", corners.astype(np.float32).ravel())
    # End grain is outside this study; caps retain a separate flat material.
    obj.data.materials.append(
        constant_material("unmodelled end grain", (0.12, 0.065, 0.025), 0.8)
    )
    for poly in obj.data.polygons[-2 * sides :]:
        poly.material_index = 1
    return mesh


def render(inputs, output, mode, view):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private render outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with (
        patch("bark_furrows.FurrowRecipe.plan", side_effect=AssertionError),
        patch("furrow_raster.raster", side_effect=AssertionError),
        patch("furrow_maps.maps", side_effect=AssertionError),
    ):
        material = (
            cached_material(inputs)
            if mode == "furrows"
            else mature_material(mature_recipe())
        )
    metadata = json.loads((inputs / "material.json").read_text())
    width, height = metadata["recipe"]["width_m"], metadata["recipe"]["height_m"]
    mesh = tube(material, width, height)
    eye = (0, -0.7, 0.6) if view == "front" else (0.7, -0.03, 0.6)
    camera = Camera(
        eye, (0, -0.03, 0.6), fov_y=35, width=2560, height=1440, near=0.01, far=100
    )
    scene = environment(camera)
    scene.render.filepath = str(output / "bark.png")
    start = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    report = {
        "mode": mode,
        "view": view,
        "eye": eye,
        "resolution": [2560, 1440],
        "render_s": time.perf_counter() - start,
        "renderer": gpu.platform.renderer_get(),
        "triangles": len(mesh["faces"]),
        "scope": "Same curved constant-radius specimen and studio. Cached normal map, no displacement/end grain/collars/taper/LOD/wind/species/native admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("cells", "furrows"), required=True)
    parser.add_argument("--view", choices=("front", "seam"), required=True)
    args = parser.parse_args()
    render(args.inputs, args.output, args.mode, args.view)
