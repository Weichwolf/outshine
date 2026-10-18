"""Same inventory/light/material with dense versus physically attached leaf shoots."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import bpy
import gpu
import numpy as np
from attached_crown import AttachedCrown
from attached_crown_scene import child_tubes, isolated_carrier
from demand import Camera
from render_shoot_forms import environment
from shoot_form import ShootForm
from shoot_form_detail import matched_detail
from shoot_form_scene import leaf_objects, wood_objects
from spatial_group_source import load_source
from twig_clusters import TwigClusters


def camera_plan(dense, view):
    host = int(
        np.argmin(np.linalg.norm(dense.base.data["origins"] - (0, -6, 10), axis=1))
    )
    target = (
        dense.base.data["origins"][host] + dense.base.data["frames"][host, :, 1] * 0.2
    )
    offsets = {"isolated": (0.5, -1.6, 0.65), "near": (0.35, -2.6, 0.6)}
    if view in offsets:
        eye, fov = target + offsets[view], 45
    else:
        eye = (0, -26, 10) if view == "whole" else (18.3847763, -18.3847763, 10)
        target, fov = (0, 0, 9), 50
    return Camera(
        tuple(eye),
        tuple(target),
        fov_y=fov,
        width=2560,
        height=1440,
        near=0.05,
        far=1000,
    ), host


def render(inputs, study, output, mode, view):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    with (
        patch.object(TwigClusters, "twigs", side_effect=AssertionError),
        patch.object(TwigClusters, "prototype", side_effect=AssertionError),
        patch.object(AttachedCrown, "build", side_effect=AssertionError),
        patch.object(ShootForm, "build", side_effect=AssertionError),
        patch("numpy.random.Generator", side_effect=AssertionError),
    ):
        attached = AttachedCrown.load(
            study / "attached.npz", expected_source=source.identity
        )
        dense = ShootForm.load(inputs / "form.npz", expected_source=source.identity)
        camera, host = camera_plan(dense, view)
        form = attached if mode == "attached" else dense
        if view == "isolated":
            selected = np.arange(len(form.lower)) == host
            fine = np.ones(90000, bool)
        else:
            selected = camera.visible(form.lower, form.upper)
            fine = (
                matched_detail(source, (dense, attached), camera)
                if view == "near"
                else np.zeros(90000, bool)
            )
        bpy.ops.wm.read_factory_settings(use_empty=True)
        before = time.perf_counter()
        leaf_stats = leaf_objects(source, form, camera, inputs, fine, selected=selected)
        wood_stats = (
            isolated_carrier(dense, host)
            if view == "isolated"
            else wood_objects(dense, camera, inputs, near=view == "near")
        )
        children = child_tubes(attached, selected) if mode == "attached" else {}
    scene = environment(camera)
    build_s = time.perf_counter() - before
    scene.render.filepath = str(output / "crown.png")
    print(
        json.dumps({"build_s": build_s, **leaf_stats, **wood_stats, **children}),
        flush=True,
    )
    before = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "form_identity": form.identity,
        "mode": mode,
        "view": view,
        "host_index": host,
        "host_node": int(source.crown.hosts[host]),
        "eye": camera.position,
        "target": camera.target,
        "fov_y": camera.fov_y,
        "resolution": [2560, 1440],
        "samples": 32,
        "renderer": gpu.platform.renderer_get(),
        "vendor": gpu.platform.vendor_get(),
        "fine_leaf_mask_sha256": hashlib.sha256(fine.tobytes()).hexdigest(),
        "fine_id_count": int(fine.sum()),
        "build_s": build_s,
        "render_s": time.perf_counter() - before,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "png_sha256": hashlib.sha256((output / "crown.png").read_bytes()).hexdigest(),
        **leaf_stats,
        **wood_stats,
        **children,
        "scope": "Same physical blades, procedural study material and C1 scaffold/carriers; only additional twig structure and leaf poses change. Same per-ID mesh in each comparison. Shadows disabled for camera-only demand. Isolated view excludes whole scaffold/context. Static study, no species/wind/shadow/LOD/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mode", choices=("dense", "attached"), required=True)
    parser.add_argument(
        "--view", choices=("isolated", "near", "whole", "oblique"), required=True
    )
    args = parser.parse_args()
    render(args.inputs, args.study, args.output, args.mode, args.view)
