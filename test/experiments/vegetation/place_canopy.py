"""Demand views from existing complete angular plant products; no organ rebuild."""

import hashlib
import json
from pathlib import Path

import numpy as np
from blender_instances import instance_group
from blender_preview import mesh_object
from conifer_whole_preview import conifer_material
from sheet_material import sheet_material


def populate(sites, products_path, eye, sun):
    inputs, total = [products_path], 0
    products = json.loads(products_path.read_text())
    for kind, item in enumerate(products):
        directory = Path(item["directory"])
        manifest_path = directory / "plant-product.json"
        manifest = json.loads(manifest_path.read_text())
        recipe_path = directory / manifest["recipe"]
        identity = hashlib.sha256(
            json.dumps(json.loads(recipe_path.read_text()), sort_keys=True).encode()
        ).hexdigest()
        if identity != manifest["plan_identity"]:
            raise ValueError("complete plant recipe identity changed")
        mesh_path = directory / manifest["mesh"]
        inputs.extend((manifest_path, recipe_path, mesh_path))
        inputs.extend(directory / p for p in manifest["textures"].values())
        with np.load(mesh_path, allow_pickle=False) as stored:
            mesh = {key: stored[key] for key in stored.files}
        selected = np.flatnonzero(sites["kind"] == kind)
        positions, frames, scales = (
            sites[key][selected] for key in ("positions", "frames", "scales")
        )
        if not len(selected):
            continue
        pivot = np.mean(manifest["bounds_m"], axis=0)
        for role, ray in (
            ("camera", np.asarray(eye) - positions),
            ("sun", np.broadcast_to(sun, positions.shape)),
        ):
            local = np.einsum("nji,nj->ni", frames, ray) / scales[:, None]
            if role == "camera":
                local -= pivot
            views = np.argmax(local @ mesh["directions"].T, axis=1)
            material = (
                conifer_material(
                    item["label"] + role,
                    directory,
                    manifest["textures"],
                    "camera" if role == "camera" else "shadow",
                )
                if kind
                else sheet_material(
                    item["label"] + role,
                    directory,
                    manifest["textures"],
                    True,
                    "camera" if role == "camera" else "shadow",
                )
            )
            v, t = manifest["vertices_per_view"], manifest["draw_triangles"]
            for view in np.unique(views):
                first, face_first = int(view) * v, int(view) * t
                faces = mesh["faces"][face_first : face_first + t] - first
                prototype = mesh_object(
                    f"{kind}-{role}-{view}",
                    mesh["vertices"][first : first + v],
                    faces,
                    mesh["normals"][face_first * 3 : (face_first + t) * 3],
                    material,
                    corner_normals=True,
                )
                uv = mesh["uv"][first : first + v][faces.ravel()]
                prototype.data.uv_layers.new(
                    name="complete plant atlas"
                ).data.foreach_set("uv", uv.ravel())
                members = np.flatnonzero(views == view)
                for begin in range(0, len(members), 100000):
                    part = members[begin : begin + 100000]
                    holder = instance_group(
                        prototype,
                        positions[part],
                        frames[part],
                        scales[part],
                        name=f"{kind}-{role}-{view}-{begin}",
                    )
                    holder["vegetation_pass_role"] = (
                        "camera" if role == "camera" else "shadow"
                    )
                    holder["plant_plan_identity"] = identity
                    if role == "sun":
                        holder["shadow_light"] = "sun"
                total += len(members) * t
    return inputs, {
        "instanced_triangles_camera_plus_sun": total,
        "sites": len(sites["positions"]),
        "models": products,
        "limits": "Existing 64-view whole-plant products and separate sunlight views. No organ generation. Statistical angular/needle fidelity and runtime LOD remain unapproved; initial two morphology proxies do not resolve all local species.",
    }
