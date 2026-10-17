"""Compare atlas and isolated-view normal components on the same cached whole tree."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from conifer_preview import setup_scene_lights
from fern_material import source_normal
from mature_tree_portrait import SCENE
from needle_response_study import source_digest
from normal_component_material import material
from normal_view_images import ViewImages
from normal_view_tiles import TileFields
from normal_view_uv import isolated_uv, local_uv
from preview_lighting import SUN_ROTATION
from scipy.spatial.transform import Rotation
from window_portrait_blender import bind_mesh
from window_portrait_geometry import planes, select_views
from window_portrait_source import source
from window_wood_instances import bind


def wood_material(light):
    surface = bpy.data.materials.new("same actual unit-diffuse wood normals")
    surface.use_nodes = True
    nodes, links = surface.node_tree.nodes, surface.node_tree.links
    dot = nodes.new("ShaderNodeVectorMath")
    dot.operation = "DOT_PRODUCT"
    links.new(source_normal(surface), dot.inputs[0])
    dot.inputs[1].default_value = light
    positive = nodes.new("ShaderNodeMath")
    positive.operation = "MAXIMUM"
    links.new(dot.outputs["Value"], positive.inputs[0])
    positive.inputs[1].default_value = 0
    emission = nodes.new("ShaderNodeEmission")
    links.new(positive.outputs[0], emission.inputs["Color"])
    links.new(emission.outputs[0], nodes["Material Output"].inputs["Surface"])
    return surface


def bind_views(
    windows, bank, vectors, images, frames, selected, indices, family, mode, light
):
    if not len(indices):
        return 0, 0, 0, 0
    triangles, payload, image_count, groups, emitted = 0, 0, 0, 0, 0
    choices = selected[indices]
    order = np.argsort(choices, kind="stable")
    views, starts = np.unique(choices[order], return_index=True)
    for view, first, last in zip(
        views, starts, np.r_[starts[1:], len(order)], strict=True
    ):
        members = indices[order[first:last]]
        factory = ViewImages(0)
        surface = material(
            vectors[view : view + 1],
            images[view : view + 1],
            frames[view : view + 1],
            light,
            mode,
            image_factory=factory,
        )
        geometry = planes(windows, bank, selected, members, family)
        geometry["uv"] = local_uv(geometry["uv"], selected[members])
        obj = bind_mesh(f"camera family{family} view{view}", geometry, surface)
        obj["source_view_id"] = int(view)
        triangles += len(geometry["faces"])
        emitted += len(members)
        payload += factory.float_payload_bytes
        image_count += len(factory.images)
        groups += 1
    if emitted != len(indices):
        raise AssertionError("every demanded source window must be emitted once")
    return triangles, payload, image_count, groups


def populate(whole, catalog, fields, mode, *, storage="view", staging=None):
    if storage not in ("atlas", "isolated", "view") or (
        storage == "isolated" and staging is None
    ):
        raise ValueError("declared storage and isolated staging destination required")
    graph, axes, windows, banks, proof = source(whole, whole / "execution", catalog)
    manifest = json.loads((fields / "bank.json").read_text())
    if (
        manifest["schema"] != "diagnostic-positive-vector-bank-1"
        or manifest["source_catalog_sha256"] != source_digest(catalog / "catalog.json")
        or len(manifest["records"]) != len(banks)
    ):
        raise ValueError("same complete canonical prototype bank required")
    for path, sha in manifest["input_sha256"].items():
        if source_digest(Path(path)) != sha:
            raise ValueError("unchanged consumed prototype inputs required")
    light = Rotation.from_euler("xyz", SUN_ROTATION).apply([0, 0, 1])
    wood = bind(graph, axes, wood_material(light))
    selected = select_views(windows, 64, eye=SCENE["camera"])
    triangles, tile_payload, tile_count, view_payload, view_images, view_groups = (
        0,
        0,
        0,
        0,
        0,
        0,
    )
    for record, bank in zip(manifest["records"], banks, strict=True):
        path = fields / record["file"]
        if source_digest(path) != record["sha256"]:
            raise ValueError("unchanged positive-vector product required")
        with np.load(path, allow_pickle=False) as data:
            vectors = data["vectors"]
        family = record["family"]
        images = np.array([image for image, _ in bank])
        frames = np.array([p[0] for _, p in bank])
        indices = np.flatnonzero(windows["families"] == family)
        if storage == "view":
            t, p, n, g = bind_views(
                windows,
                bank,
                vectors,
                images,
                frames,
                selected,
                indices,
                family,
                mode,
                light,
            )
            triangles += t
            view_payload += p
            view_images += n
            view_groups += g
            continue
        factory = (
            TileFields(staging / f"family-{family}", prepare=True)
            if storage == "isolated"
            else None
        )
        surface = material(
            vectors,
            images,
            frames,
            light,
            mode,
            **({"image_factory": factory} if factory else {}),
        )
        if factory:
            tile_payload += factory.float_payload_bytes
            tile_count += sum(len(image.tiles) for image in factory.images)
        if len(indices):
            geometry = planes(windows, bank, selected, indices, family)
            if factory:
                geometry["uv"] = isolated_uv(geometry["uv"], selected[indices])
            bind_mesh(f"unchanged camera family{family}", geometry, surface)
            triangles += len(geometry["faces"])
    return proof | {
        "axes": len(axes["origins"]),
        "windows": len(windows["origins"]),
        "represented_needles": int(windows["needle_counts"].sum()),
        "wood": wood,
        "camera_window_triangles": triangles,
        "positive_vector_payload_bytes": sum(
            r["vector_bytes"] for r in manifest["records"]
        ),
        "light": light.tolist(),
        "mode": mode,
        "storage": storage,
        "controlled_tile_float_bytes": tile_payload,
        "staged_tiles": tile_count,
        "controlled_view_image_float_bytes": view_payload,
        "independent_view_images": view_images,
        "independent_view_groups": view_groups,
    }


def render(whole, catalog, fields, output, mode, *, storage="view"):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    began = time.perf_counter()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.world = bpy.data.worlds.new(
        "same neutral component diagnostic world"
    )
    report = populate(
        whole, catalog, fields, mode, storage=storage, staging=output / "tile-fields"
    )
    setup_scene_lights(SCENE | {"own_ground": True})
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.eevee.taa_render_samples = 32
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    scene.render.filepath = str(output / "portrait.exr")
    ready = time.perf_counter()
    print(json.dumps(report | {"stage": "render"}), flush=True)
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(str(output / "portrait.exr"), check_existing=False)
    pixels = np.asarray(image.pixels[:], np.float32).reshape(1440, 2560, 4)
    np.savez_compressed(output / "linear.npz", rgba=pixels)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_depth = "8"
    bpy.data.images["Render Result"].save_render(
        str(output / "portrait.png"), scene=scene
    )
    report |= {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "renderer": gpu.platform.renderer_get(),
        "resolution": [2560, 1440],
        "samples": 32,
        "populate_s": ready - began,
        "render_and_export_s": time.perf_counter() - ready,
        "process_peak_RSS_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "scope": {
            "atlas": "REJECTED cross-view atlas mip binding; ",
            "isolated": "REJECTED tested UDIM coverage binding; ",
            "view": "clamped per-view mip diagnostic; ",
        }[storage]
        + "unshadowed unit-diffuse only, no PBR/LOD/default approval",
        "controlled_atlas_image_float_bytes": sum(
            image.size[0] * image.size[1] * 4 * 4
            for image in bpy.data.images
            if image.name.startswith("diagnostic signed Float32 field")
        ),
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "files"}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("whole", "catalog", "fields", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--mode", choices=("mean", "four"), required=True)
    parser.add_argument(
        "--storage", choices=("atlas", "isolated", "view"), default="view"
    )
    args = parser.parse_args()
    render(
        args.whole,
        args.catalog,
        args.fields,
        args.output,
        args.mode,
        storage=args.storage,
    )
