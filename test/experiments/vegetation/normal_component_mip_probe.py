"""Counterprobe for accidental cross-view mip filtering in the diagnostic atlas."""

import argparse
import json
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector
from normal_component_material import material
from normal_view_images import ViewImages
from normal_view_tiles import TileFields
from normal_view_uv import isolated_uv, local_uv
from window_portrait_blender import bind_mesh


def run(output, *, storage="atlas", views=(0,), footprints=(64, 1), pattern="constant"):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    if (
        storage not in ("atlas", "isolated", "view")
        or pattern not in ("constant", "checker")
        or not 1 <= len(views) <= 64
        or any(type(v) is not int or not 0 <= v < 64 for v in views)
    ):
        raise ValueError("declared storage/pattern and bounded64-view demand required")
    output.mkdir(parents=True, exist_ok=False)
    results = []
    for selected in views:
        for interpolation in (
            ("Linear",) if storage != "atlas" else ("Linear", "Closest")
        ):
            for footprint in footprints:
                bpy.ops.wm.read_factory_settings(use_empty=True)
                frames = np.tile(np.diag([-1.0, 1.0, -1.0]), (64, 1, 1))
                frames[selected] = np.eye(3)
                vectors = np.zeros((64, 32, 32, 4, 3), np.float32)
                vectors[..., 0, 2] = -1
                vectors[selected, ..., 0, 2] = 1
                if pattern == "checker":
                    checker = np.indices((32, 32)).sum(axis=0) % 2
                    vectors[selected, ..., 0, 2] = checker
                    vectors[selected, ..., 0, 0] = 1 - checker
                images = np.zeros((64, 32, 32, 9), np.float32)
                images[..., 3] = 1
                images[..., 6] = 1
                factory = (
                    TileFields(
                        output / f"fields-{selected}",
                        prepare=footprint == footprints[0],
                    )
                    if storage == "isolated"
                    else None
                )
                if storage == "view":
                    factory = ViewImages(0)
                    vectors, images, frames = (
                        a[selected : selected + 1] for a in (vectors, images, frames)
                    )
                surface = material(
                    vectors,
                    images,
                    frames,
                    [0, 0, 1],
                    "four",
                    **({"image_factory": factory} if factory else {}),
                )
                for node in surface.node_tree.nodes:
                    if node.type == "TEX_IMAGE":
                        node.interpolation = interpolation
                lower = np.array((selected % 8 * 40 + 4, selected // 8 * 40 + 4))
                uv = (
                    np.array([[0, 0], [32, 0], [32, 32], [0, 32]], float) + lower
                ) / 320
                if storage == "isolated":
                    uv = isolated_uv(uv, np.array([selected]))
                elif storage == "view":
                    uv = local_uv(uv, np.array([selected]))
                bind_mesh(
                    "same view0 control",
                    {
                        "vertices": np.array(
                            [[-1, -1, 0], [1, -1, 0], [1, 1, 0], [-1, 1, 0]], float
                        ),
                        "faces": np.array([[0, 1, 2], [0, 2, 3]]),
                        "uv": uv,
                        "weights": np.ones(2),
                    },
                    surface,
                )
                scene = bpy.context.scene
                scene.render.engine = "BLENDER_EEVEE"
                camera = bpy.data.objects.new("camera", bpy.data.cameras.new("camera"))
                scene.collection.objects.link(camera)
                scene.camera = camera
                camera.location = Vector([0, 0, 3])
                camera.rotation_euler = (
                    (-camera.location).to_track_quat("-Z", "Y").to_euler()
                )
                camera.data.type = "ORTHO"
                camera.data.ortho_scale = 128 / footprint
                scene.render.resolution_x = scene.render.resolution_y = 64
                scene.render.resolution_percentage = 100
                scene.render.film_transparent = True
                scene.eevee.taa_render_samples = 32
                scene.render.image_settings.file_format = "OPEN_EXR"
                scene.render.image_settings.color_depth = "32"
                path = output / f"{storage}-{selected}-{interpolation}-{footprint}.exr"
                scene.render.filepath = str(path)
                bpy.ops.render.render(write_still=True)
                image = bpy.data.images.load(str(path), check_existing=False)
                pixels = np.asarray(image.pixels[:], float).reshape(64, 64, 4)
                alpha = pixels[..., 3].sum()
                response = float(pixels[..., 0].sum() / alpha)
                expected = 0.5 if pattern == "checker" else 1
                result = {
                    "storage": storage,
                    "view_index": selected,
                    "interpolation": interpolation,
                    "footprint_pixels": footprint,
                    "pattern": pattern,
                    "expected_conditional_response": expected,
                    "measured_response": response,
                    "alpha_sum": float(alpha),
                    "response_passes": abs(response - expected) < 0.003,
                    "coverage_passes": abs(alpha - footprint**2)
                    < max(0.003 * footprint**2, 0.003),
                    "passes": abs(response - expected) < 0.003
                    and abs(alpha - footprint**2) < max(0.003 * footprint**2, 0.003),
                    "controlled_float_image_bytes": factory.float_payload_bytes
                    if factory
                    else None,
                }
                results.append(result)
                print(json.dumps(result), flush=True)
    (output / "report.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--storage", choices=("atlas", "isolated", "view"), default="atlas"
    )
    parser.add_argument("--views", type=int, nargs="+", default=[0])
    parser.add_argument(
        "--pattern", choices=("constant", "checker"), default="constant"
    )
    args = parser.parse_args()
    run(
        args.output, storage=args.storage, views=tuple(args.views), pattern=args.pattern
    )
