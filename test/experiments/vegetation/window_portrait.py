"""Render unchanged executed mature spruce using cached finite physical windows."""

import argparse
import json
import resource
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from blender_preview import material
from conifer_preview import setup_scene_lights
from fern_material import attach_normals, source_normal
from mature_tree_portrait import SCENE
from outshine_ref.vegetation import Needle, NeedledPlant, NeedlePattern, PlantPlan
from preview_lighting import SUN_ROTATION
from preview_shadows import configure_preview
from scipy.spatial.transform import Rotation
from window_portrait_blender import bind_mesh, textures, weighted_material
from window_portrait_geometry import planes, select_views
from window_portrait_source import source, wood


def populate(whole, execution, catalog, output):
    start = time.perf_counter()
    graph, axes, windows, banks, evidence = source(whole, execution, catalog)
    decoded = time.perf_counter()
    surface = material("unchanged constant source wood", (0.14, 0.105, 0.037), 0.8)
    geometry = wood(graph, axes)
    obj = bind_mesh("complete executed wood", geometry, surface)
    attach_normals(obj, geometry["normals"])
    surface.node_tree.links.new(
        source_normal(surface),
        surface.node_tree.nodes["Principled BSDF"].inputs["Normal"],
    )
    triangles = len(geometry["faces"])
    output_bytes = sum(a.nbytes for a in geometry.values())
    wood_seconds = time.perf_counter() - decoded
    del geometry
    queries = {
        "camera": {"eye": SCENE["camera"]},
        "sun": {"direction": Rotation.from_euler("xyz", SUN_ROTATION).apply((0, 0, 1))},
        "fill": {"eye": SCENE["fill_position_m"]},
    }
    selected = {
        role: select_views(windows, 64, **query) for role, query in queries.items()
    }
    for family, bank in enumerate(banks):
        directory = output / f"family-{family}"
        maps = textures(bank, directory)
        indices = np.flatnonzero(windows["families"] == family)
        if not len(indices):
            continue
        for role, views in selected.items():
            data = planes(windows, bank, views, indices, family)
            surface = weighted_material(
                f"window {family} {role}",
                directory,
                maps,
                "camera" if role == "camera" else "shadow",
            )
            obj = bind_mesh(f"complete windows {family} {role}", data, surface)
            obj["vegetation_pass_role"] = "camera" if role == "camera" else "shadow"
            if role != "camera":
                obj["shadow_light"] = role
            triangles += len(data["faces"])
            output_bytes += sum(
                a.nbytes for a in data.values() if isinstance(a, np.ndarray)
            )
    return evidence | {
        "axes": len(axes["origins"]),
        "windows": len(windows["origins"]),
        "represented_needles": int(windows["needle_counts"].sum()),
        "decoded_array_bytes": sum(
            a.nbytes for a in (*axes.values(), *windows.values())
        ),
        "decoded_catalog_image_bytes": sum(
            image.nbytes for bank in banks for image, _ in bank
        ),
        "geometry_output_array_bytes": output_bytes,
        "triangles_camera_and_shadow_roles": triangles,
        "decode_and_provenance_s": decoded - start,
        "wood_assembly_and_bind_s": wood_seconds,
        "populate_s": time.perf_counter() - start,
        "view_histograms": {
            k: np.bincount(v, minlength=64).tolist() for k, v in selected.items()
        },
    }


def render(whole, execution, catalog, output):
    output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.world = bpy.data.worlds.new("same flat portrait world")
    with (
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("new plant organs")
        ),
        patch.object(
            NeedlePattern, "detail", side_effect=AssertionError("new needle poses")
        ),
        patch.object(Needle, "mesh", side_effect=AssertionError("new needle meshes")),
        patch.object(PlantPlan, "axis", side_effect=AssertionError("new growth axes")),
    ):
        report = populate(whole, execution, catalog, output)
    setup_scene_lights(SCENE)
    scene = bpy.context.scene
    configure_preview(scene)
    scene.eevee.taa_render_samples = SCENE["samples"]
    scene.render.resolution_x, scene.render.resolution_y = SCENE["resolution"]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str((output / "portrait.png").resolve())
    ready = time.perf_counter()
    print(json.dumps(report | {"stage": "render"}), flush=True)
    bpy.ops.render.render(write_still=True)
    report |= {
        "scene": SCENE,
        "render_s": time.perf_counter() - ready,
        "process_s": time.perf_counter() - started,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "backend": gpu.platform.renderer_get(),
        "scope": "Same complete cached physical windows and executed wood; new bounded representation assembly and 8bit finite-chart packing, no axis/needle planning or meshing. Statistical windows, tail weighting, mean normals, growth form and native/device/LOD admission remain unapproved.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("whole", "execution", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    render(args.whole, args.execution, args.catalog, args.output)
