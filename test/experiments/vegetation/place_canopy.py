"""Demand views from existing complete angular plant products; no organ rebuild."""

import hashlib
import json
from pathlib import Path

import numpy as np
from blender_instances import instance_group
from blender_preview import mesh_object
from conifer_whole_preview import conifer_material
from place_demand import demand, spheres
from place_grass import populate_grass
from preview_lighting import SUN_ANGULAR_DIAMETER_DEG
from sheet_material import sheet_material


def receiver_floor(sites, manifests, scene_floor):
    result = scene_floor
    for kind, manifest in enumerate(manifests):
        members = sites["kind"] == kind
        if not members.any():
            continue
        centers, radii = spheres(
            manifest["render_bounds_m"],
            *(sites[key][members] for key in ("positions", "frames", "scales")),
        )
        result = min(result, float(np.min(centers[:, 2] - radii)))
    return result


def populate(sites, products_path, camera, ground, sun, min_receiver_z):
    inputs, total, counts = [products_path], 0, []
    products = json.loads(products_path.read_text())
    if any(int(kind) >= len(products) or kind < 0 for kind in np.unique(sites["kind"])):
        raise ValueError("every retained form kind needs an explicit cached product")
    manifests = [
        json.loads((Path(item["directory"]) / "plant-product.json").read_text())
        for item in products
    ]
    min_receiver_z = receiver_floor(sites, manifests, min_receiver_z)
    for kind, (item, manifest) in enumerate(zip(products, manifests, strict=True)):
        directory = Path(item["directory"])
        manifest_path = directory / "plant-product.json"
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
        bounds = np.asarray(manifest["render_bounds_m"])
        if np.any(mesh["vertices"] < bounds[0]) or np.any(mesh["vertices"] > bounds[1]):
            raise ValueError("angular-product bounds do not enclose cached geometry")
        centers, radii = spheres(bounds, positions, frames, scales)
        masks = demand(
            camera,
            ground,
            centers,
            radii,
            sun,
            min_receiver_z,
            sun_angular_radius_deg=SUN_ANGULAR_DIAMETER_DEG / 2,
        )
        counts.append(
            {
                "kind": kind,
                "planned": len(selected),
                **{k: int(m.sum()) for k, m in masks.items()},
            }
        )
        if manifest.get("representation") == "spatial_mesh":
            total += populate_grass(manifest, identity, mesh, sites, selected, masks)
            continue
        pivot = np.mean(manifest["bounds_m"], axis=0)
        for role, ray in (
            ("camera", camera.eye(ground) - positions),
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
            for view in np.unique(views[masks[role]]):
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
                members = np.flatnonzero((views == view) & masks[role])
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
        "demand": counts,
        "limits": "Cached 64-view tree proxies plus shared spatial graminoid tufts. Tree camera/sun products remain separate; grass uses one union of both demands. No organ generation on cache hit. Unidentified grass form, incomplete meadow/understorey density and unapproved angular/needle/LOD fidelity; not botanical species coverage.",
    }
