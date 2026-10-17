"""Render one unchanged requested twig/window pair member into PNG and linear EXR."""

import argparse
import hashlib
import json
import resource
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from conifer_whole_products import load_catalog
from PIL import Image
from twig_light_blender import VIEW, bind_physical, bind_windows, configure_scene
from twig_light_source import provenance


def projected_span(geometry, scene):
    camera = scene.camera
    inverse = np.array(camera.matrix_world.inverted())
    points = geometry["vertices"] @ inverse[:3, :3].T + inverse[:3, 3]
    frame = np.array(camera.data.view_frame(scene=scene))
    return (
        np.ptp(points[:, :2], axis=0)
        / np.ptp(frame[:, :2], axis=0)
        * np.array([scene.render.resolution_x, scene.render.resolution_y])
    ).tolist()


def render(case, catalog, output, representation, light_kind, pixels, *, shadows=True):
    if (
        representation not in ("physical", "windows")
        or light_kind not in ("front", "back", "side")
        or pixels not in (16, 64, 384)
    ):
        raise ValueError("declared bounded comparison member required")
    started = time.perf_counter()
    code = provenance()
    output.mkdir(parents=True, exist_ok=False)
    with np.load(case / "physical.npz", allow_pickle=False) as saved:
        geometry = {k: saved[k] for k in saved.files}
    with np.load(case / "window.npz", allow_pickle=False) as saved:
        window = {k: saved[k] for k in saved.files}
    if not 0 < len(geometry["faces"]) <= 250000 or len(window["origins"]) != 1:
        raise ValueError("one bounded requested window and physical source required")
    _, banks = load_catalog(catalog)
    right = np.cross((0, 0, 1), VIEW)
    right /= np.linalg.norm(right)
    extent = float(np.ptp(geometry["vertices"] @ right))
    light = {"front": VIEW, "back": -VIEW, "side": right}[light_kind]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if representation == "physical":
        directory = output / "opaque-source"
        directory.mkdir()
        # Unit source texels preserve the actual existing shader's fixed controls.
        for name, colour in (
            ("colour", (255, 255, 255, 255)),
            ("normal", (128, 128, 255, 255)),
            ("leaf", (255, 255, 255, 255)),
        ):
            Image.new("RGBA", (1, 1), colour).save(directory / f"{name}.png")
        bind_physical(
            geometry,
            directory,
            {"colour": "colour.png", "normal": "normal.png", "leaf_weight": "leaf.png"},
        )
    else:
        bind_windows(
            window, banks, output / "window-source", VIEW, light, shadows=shadows
        )
    scene_data = configure_scene(extent, pixels, light, shadows=shadows)
    scene = bpy.context.scene
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.filepath = str(output / "linear.exr")
    ready = time.perf_counter()
    print(
        json.dumps(
            {"stage": "render", "representation": representation, "pixels": pixels}
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(str(output / "linear.exr"), check_existing=False)
    values = np.empty(2560 * 1440 * 4, np.float32)
    image.pixels.foreach_get(values)
    values = values.reshape(1440, 2560, 4)
    np.savez_compressed(output / "linear.npz", rgba=values)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    bpy.data.images["Render Result"].save_render(
        str(output / "portrait.png"), scene=scene
    )
    facts = {
        "producer": code,
        "scene": scene_data,
        "representation": representation,
        "light_kind": light_kind,
        "case": json.loads((case / "source.json").read_text()),
        "source_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (case / "physical.npz", case / "window.npz", case / "source.json")
        },
        "projected_physical_span_pixels": projected_span(geometry, scene),
        "alpha_sum": float(values[..., 3].sum()),
        "linear_rgb_stored_sum": values[..., :3].sum(axis=(0, 1)).tolist(),
        "alpha_weighted_rgb_sum": (values[..., :3] * values[..., 3, None])
        .sum(axis=(0, 1))
        .tolist(),
        "loaded_exr_alpha_mode": image.alpha_mode,
        "radiance_scope": "Both raw and alpha-weighted sums recorded; interpretation requires the analytic emission/coverage check.",
        "render_and_export_s": time.perf_counter() - ready,
        "process_s": time.perf_counter() - started,
        "process_peak_rss_MiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        / 1024,
        "backend": gpu.platform.renderer_get(),
    }
    (output / "report.json").write_text(json.dumps(facts, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in facts.items()
                if k not in ("case", "scene", "source_sha256")
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("case", "catalog", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument(
        "--representation", choices=("physical", "windows"), required=True
    )
    parser.add_argument("--light", choices=("front", "back", "side"), required=True)
    parser.add_argument("--pixels", type=int, choices=(16, 64, 384), required=True)
    parser.add_argument("--shadows", choices=("matched", "off"), default="matched")
    args = parser.parse_args()
    render(
        args.case,
        args.catalog,
        args.output,
        args.representation,
        args.light,
        args.pixels,
        shadows=args.shadows == "matched",
    )
