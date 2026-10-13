"""Bind complete requested plant products once; export independent time samples."""

import json
import shutil

import numpy as np
from organ_experiment import write_product


def world_mesh(directory, product):
    with np.load(directory / product["mesh"]) as data:
        source = {k: data[k] for k in data.files}
    with np.load(directory / product["placements"]) as data:
        origins, frames = data["origins"], data["frames"]
    if product["surface"] == "wood" and (
        len(origins) != 1 or not np.array_equal(frames[0], np.eye(3))
    ):
        raise ValueError("wind bark export currently requires one world-space body")
    n = len(source["vertices"])
    meshes = []
    for index, (origin, frame) in enumerate(zip(origins, frames)):
        mesh = {key: value.copy() for key, value in source.items()}
        mesh["vertices"] = source["vertices"] @ frame.T + origin
        mesh["normals"] = source["normals"] @ np.linalg.inv(frame)
        mesh["normals"] /= np.linalg.norm(mesh["normals"], axis=1)[:, None]
        if product["surface"] == "leaf":
            mesh["wind_leaf_variation"] = np.full(
                n, np.random.Generator(np.random.PCG64([17, index])).random()
            )
        meshes.append(mesh)
    combined = {key: np.concatenate([m[key] for m in meshes]) for key in meshes[0]}
    combined["faces"] = np.concatenate(
        [m["faces"] + i * n for i, m in enumerate(meshes)]
    )
    return combined


def prepare(directory, recipe, field, output, prefix):
    prepared = []
    for i, product in enumerate(recipe["products"]):
        mesh = world_mesh(directory, product)
        binding = field.bind(mesh["vertices"])
        np.savez_compressed(output / f"{prefix}-{i}-rest.npz", **mesh)
        np.savez_compressed(
            output / f"{prefix}-{i}-binding.npz",
            values=binding[0],
            gradients=binding[1],
        )
        with np.load(output / f"{prefix}-{i}-binding.npz") as stored:
            cached = (stored["values"], stored["gradients"])
        assert all(np.array_equal(a, b) for a, b in zip(binding, cached))
        binding = cached
        prepared.append((product, mesh, binding))
        if "bark_material" in product:
            shutil.copy2(
                directory / product["bark_material"], output / product["bark_material"]
            )
    return prepared


def deform(field, mesh, binding, seconds, strength):
    vertices, gradient = field.move(mesh["vertices"], binding, seconds, strength)
    normals = field.normals(mesh["normals"], gradient[mesh["faces"].ravel()])
    return mesh | {"vertices": vertices, "normals": normals}


def export_sample(output, name, recipe, prepared, field, seconds, strength):
    products = []
    for i, (source, mesh, binding) in enumerate(prepared):
        moved = deform(field, mesh, binding, seconds, strength)
        product = write_product(
            output,
            f"{name}-{i}",
            moved,
            np.zeros((1, 3)),
            np.eye(3)[None],
            source["surface"],
        )
        if "bark_material" in source:
            product["bark_material"] = source["bark_material"]
        products.append(product)
    scenario = recipe | {
        "name": name,
        "products": products,
        "wind_seconds": seconds,
        "wind_strength": strength,
        "source_modified": True,
    }
    (output / f"{name}.json").write_text(json.dumps(scenario, indent=2) + "\n")
