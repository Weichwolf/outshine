"""Own cached closed needle/wood geometry in a canonical shoot-local frame."""

import hashlib
import json

import numpy as np
from leaf_sheet import sample_image
from PIL import Image


def source_geometry(directory):
    scenario = directory / "shoot.json"
    recipe = json.loads(scenario.read_text())
    wood = next(p for p in recipe["products"] if p.get("surface") == "wood")
    paths = [scenario]
    with np.load(directory / wood["placements"]) as p:
        anchor, frame = p["origins"][0], p["frames"][0]
    vertices, faces, normals, colour, roughness, organ = [], [], [], [], [], []
    ids, offset, ordinal = [], 0, 0
    for product in recipe["products"]:
        mesh_path, pose_path = (
            directory / product["mesh"],
            directory / product["placements"],
        )
        paths.extend((mesh_path, pose_path))
        with np.load(mesh_path) as f:
            mesh = {k: f[k] for k in f.files}
        with np.load(pose_path) as f:
            poses = {k: f[k] for k in f.files}
        base_colour, base_roughness = material_fields(directory, product, mesh, paths)
        for index in range(product["instances"]):
            rotate, origin, scale = (
                poses["frames"][index],
                poses["origins"][index],
                poses["scales"][index],
            )
            p = (mesh["vertices"] * scale) @ rotate.T + origin
            vertices.append((p - anchor) @ frame)
            faces.append(mesh["faces"] + offset)
            normals.append(mesh["normals"].reshape(-1, 3, 3) @ rotate.T @ frame)
            colour.append(base_colour)
            roughness.append(base_roughness)
            organ.append(np.full(len(mesh["faces"]), ordinal, np.int32))
            ids.append((product["surface"], int(poses["ids"][index])))
            offset += len(mesh["vertices"])
            ordinal += 1
    return {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "colour": np.concatenate(colour),
        "roughness": np.concatenate(roughness),
        "organ_faces": np.concatenate(organ),
        "ids": ids,
        "identity": recipe["plan_identity"],
        "axis_path": recipe["selected_axis_path"],
        "input_sha256": {
            str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
    }


def material_fields(directory, product, mesh, paths):
    count = len(mesh["faces"])
    if "needle_material" not in product:
        return np.broadcast_to((0.14, 0.105, 0.037), (count, 3, 3)), np.full(
            (count, 3, 1), 0.8
        )
    path = directory / product["needle_material"]
    recipe = json.loads(path.read_text())
    assert recipe["colour_space"] == "linear"
    paths.append(path)
    uv = mesh["corner_uv"].reshape(-1, 3, 2)
    fields = []
    for name in ("colour", "roughness"):
        image_path = path.parent / recipe["maps"][name]
        paths.append(image_path)
        with Image.open(image_path) as image:
            samples = np.asarray(image).astype(float)[::-1] / 255
        if samples.ndim == 2:
            samples = samples[..., None]
        fields.append(sample_image(samples, uv))
    return tuple(fields)
