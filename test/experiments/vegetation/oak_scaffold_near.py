"""One prepared mature crown; only the demanded leaf neighbourhood gets fine meshes."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import bpy
import gpu
import numpy as np
from blender_instances import instance_group
from botanical_profiles import plant_recipe
from fern_material import palette, source_normal
from oak_scaffold import near_demand
from outshine_ref.rendering.blender_camera import configure_camera
from render_biome_tiles import setup
from render_fern import model, model_arrays
from tree_entry import load


def backend():
    return {
        "renderer": gpu.platform.renderer_get(),
        "vendor": gpu.platform.vendor_get(),
        "version": gpu.platform.version_get(),
    }


def fine_mesh(blade):
    mesh = blade.mesh(32, 4)
    mesh["corner_uv"] = np.c_[
        (mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0]
    ][mesh["faces"].ravel()]
    mesh["surface"] = np.full(len(mesh["faces"]), 6, np.uint8)
    return mesh


def near(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private render outputs must be under/tmp")
    began = time.perf_counter()
    study = json.loads((inputs / "comparison.json").read_text())
    folder = inputs / "spatial"
    _, products = load(folder)
    rows = products["tables.npz"]
    origins = rows["leaf_origins"]
    requested = np.array(
        [0, -study["recipe"]["spread_m"] * 0.42, study["recipe"]["height_m"] * 0.55]
    )
    selected = int(np.argmin(np.linalg.norm(origins - requested, axis=1)))
    owner = rows["leaf_owner_nodes"][selected]
    target = origins[rows["leaf_owner_nodes"] == owner].mean(axis=0)
    blades = tuple(
        replace(b, length=b.length * 7 / 6, width=b.width * 7 / 6)
        for b in plant_recipe("quercus", study["recipe"]["seed"]).blades
    )
    eye = target + np.array([0.55, -1.8, 0.65])
    detail = near_demand(origins, rows["leaf_variants"], blades, eye, target)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mats = palette(folder)
    bump = next(n for n in mats[0].node_tree.nodes if n.type == "BUMP")
    mats[0].node_tree.links.new(source_normal(mats[0]), bump.inputs["Normal"])
    model(folder / "wood.npz", mats)
    triangles = len(products["wood.npz"]["faces"])
    fine_triangles = 0
    for variant, blade in enumerate(blades):
        coarse = products[f"blade-{variant}.npz"]
        original = blade.mesh(8, 1)
        for field, value in original.items():
            np.testing.assert_array_equal(value, coarse[field])
        for fine in (False, True):
            mask = (rows["leaf_variants"] == variant) & (detail if fine else ~detail)
            if not mask.any():
                continue
            data = fine_mesh(blade) if fine else coarse
            obj = model_arrays(
                f"leaf-{variant}-" + ("fine" if fine else "coarse"), data, mats
            )
            count = int(mask.sum())
            added = count * len(data["faces"])
            triangles += added
            if fine:
                fine_triangles += added
            instance_group(
                obj,
                origins[mask],
                rows["leaf_frames"][mask],
                np.ones(count),
                name=f"leaf-{variant}-{fine}",
            )
    scene = setup(
        "Laubwald", {"size_m": 48, "camera": eye.tolist(), "target": target.tolist()}
    )
    scene.camera = configure_camera(
        {
            "projection": "orthographic",
            "eye": eye.tolist(),
            "target": target.tolist(),
            "span_m": 1.8,
        }
    )
    scene.render.resolution_x, scene.render.resolution_y = 2560, 1440
    scene.eevee.taa_render_samples = 8
    output.mkdir(parents=True, exist_ok=False)
    path = output / "near.png"
    scene.render.filepath = str(path.resolve())
    ready = time.perf_counter()
    print(
        json.dumps(
            {
                "stage": "render",
                "setup_s": ready - began,
                "fine_leaves": int(detail.sum()),
            }
        ),
        flush=True,
    )
    bpy.ops.render.render(write_still=True)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": study["products"]["spatial"]["identity"],
        "source_product_identity": study["products"]["spatial"].get("product_identity"),
        "source_tables_sha256": hashlib.sha256(
            (folder / "tables.npz").read_bytes()
        ).hexdigest(),
        "selected_host": int(owner),
        "fine_leaf_ids": rows["leaf_ids"][detail].tolist(),
        "fine_leaves": int(detail.sum()),
        "coarse_leaves": int((~detail).sum()),
        "instanced_triangles": triangles,
        "fine_triangles": fine_triangles,
        "resolution": [2560, 1440],
        "samples": 8,
        "eye": eye.tolist(),
        "target": target.tolist(),
        "setup_s": ready - began,
        "render_s": time.perf_counter() - ready,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "backend": backend(),
        "image_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "scope": "same complete prepared plant/IDs/coarse context/shadows, 32x4 detail only on demanded leaves, existing qualitative procedural material, not species/LOD/device acceptance",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    near(args.inputs, args.output)
