"""Geometry portrait of one actual verified SITE instance, rebased at its world root."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import bpy
import gpu
import numpy as np
from demand import Camera
from render_shoot_forms import environment
from shoot_form_scene import constant_material


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def render(products, receipt, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private render output must be under /tmp")
    report = json.loads((products / "comparison.json").read_text())
    proof = json.loads(receipt.read_text())
    if (
        proof["report_sha256"] != digest(products / "comparison.json")
        or not proof["whole_form_replayed_exact"]
        or not proof["all_physical_bounds_and_SITE_products_verified"]
    ):
        raise ValueError("complete same-product physical reader required")
    records = json.loads((products / "plants.json").read_text())
    if not records:
        raise ValueError("an actually admitted whole plant is required")
    for name in ("plants.json", "prototype/complete.npz"):
        if digest(products / name) != report["artifacts"][name]:
            raise ValueError("verified whole plant changed")
    output.mkdir(parents=True, exist_ok=False)
    with np.load(products / "prototype/complete.npz", allow_pickle=False) as z:
        vertices, faces, leaf = z["vertices"], z["faces"], z["leaf_faces"]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    mesh = bpy.data.meshes.new("complete unchanged cached prototype")
    mesh.from_pydata(vertices.tolist(), [], faces.tolist())
    mesh.materials.append(
        constant_material("geometry-only wood", (0.12, 0.065, 0.025), 0.8)
    )
    mesh.materials.append(
        constant_material("geometry-only needles", (0.055, 0.22, 0.075), 0.65)
    )
    mesh.polygons.foreach_set("material_index", leaf.astype(np.int32))
    mesh.polygons.foreach_set("use_smooth", ~leaf)
    mesh.update()
    obj = bpy.data.objects.new("Pinus sylvestris: first verified SITE instance", mesh)
    bpy.context.collection.objects.link(obj)
    target = (vertices.min(axis=0) + vertices.max(axis=0)) / 2
    camera = Camera(
        tuple(target + (3.8, -4.8, 2.5)),
        tuple(target),
        fov_y=40,
        width=2560,
        height=1440,
        near=0.01,
        far=100,
    )
    scene = environment(camera)
    scene.render.filepath = str(output / "plant.png")
    start = time.perf_counter()
    bpy.ops.render.render(write_still=True)
    result = {
        "plant_product_sha256": digest(products / "comparison.json"),
        "source_part_id": records[0]["source_part_id"],
        "world_frame": records[0]["result"]["world_frame"],
        "camera_relative_origin_world_m": records[0]["result"]["world_origin_m"],
        "vertices": len(vertices),
        "triangles": len(faces),
        "resolution": [2560, 1440],
        "renderer": gpu.platform.renderer_get(),
        "render_s": time.perf_counter() - start,
        "scope": "One of four verified instances sharing the identical whole cached prototype. Camera-relative coordinates, no moved/replaced world site. Constant PBR colours and display normals; no new shape/species/material/terrain-footprint/forest/native/device admission.",
    }
    (output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("products", type=Path)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    render(args.products, args.receipt, args.output)
