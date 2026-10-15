"""Fixed existing PlantPlan input and complete cache-only whole-tree products."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from fern_maps import write_maps
from outshine_ref.vegetation import PlantPlan
from outshine_ref.vegetation.axis_surface import axis_mesh

REFERENCE = Path(__file__).parent / "reference/tree-entry-sapling-17.json"
IDENTITY = "59fbc581bf126400b60223be7202cec8d231745e589f1a25b7a28ad05d012f73"


def reference(path=REFERENCE):
    metadata = json.loads(path.read_text())
    table_path = path.parent / metadata["tables"]
    if (
        metadata["plan_identity"] != IDENTITY
        or hashlib.sha256(table_path.read_bytes()).hexdigest()
        != metadata["tables_sha256"]
    ):
        raise ValueError("fixed tree reference identity or rows changed")
    with np.load(table_path, allow_pickle=False) as data:
        rows = {key: data[key] for key in data.files}
    return metadata, rows


def compare_rows(actual, expected):
    if actual.keys() != expected.keys():
        raise ValueError("complete canonical row fields required")
    for key, value in actual.items():
        if value.dtype != expected[key].dtype or value.shape != expected[key].shape:
            raise ValueError("canonical row shape/type changed: " + key)
        if value.dtype.kind == "f":
            np.testing.assert_allclose(
                value, expected[key], rtol=0, atol=1e-14, err_msg=key
            )
        else:
            np.testing.assert_array_equal(value, expected[key], err_msg=key)


def wood_mesh(rows, detail):
    vertices, faces, normals, identities = [], [], [], []
    offset = 0
    for i, identity in enumerate(rows["axis_ids"]):
        mesh = axis_mesh(
            rows["axis_lengths"][i],
            rows["axis_bends"][i],
            *rows["axis_radii"][i],
            detail["axis_segments"],
            detail["axis_sides"],
        )
        frame, origin = rows["axis_frames"][i], rows["axis_origins"][i]
        vertices.append(mesh["vertices"] @ frame.T + origin)
        normals.append(mesh["normals"] @ frame.T)
        faces.append(mesh["faces"] + offset)
        identities.append(np.full(len(mesh["faces"]), identity, np.uint64))
        offset += len(mesh["vertices"])
    faces = np.concatenate(faces).astype(np.int32)
    return {
        "vertices": np.concatenate(vertices),
        "faces": faces,
        "normals": np.concatenate(normals),
        "axis_ids": np.concatenate(identities),
        "corner_uv": np.zeros((faces.size, 2)),
        "surface": np.zeros(len(faces), np.uint8),
    }


def build(output):
    began = time.perf_counter()
    metadata, expected = reference()
    plant = PlantPlan.from_recipe(metadata["recipe"])
    if plant.identity != IDENTITY:
        raise ValueError("fixed existing recipe changed")
    rows = plant.tables(64, 571)
    compare_rows(rows, expected)
    ready = time.perf_counter()
    output.mkdir(parents=True, exist_ok=False)
    products = {"wood": wood_mesh(rows, metadata["geometry"])}
    for i, blade in enumerate(plant.blades):
        mesh = blade.mesh(
            metadata["geometry"]["leaf_segments"], metadata["geometry"]["leaf_across"]
        )
        mesh["corner_uv"] = np.c_[
            (mesh["parameters"][:, 1] + 1) / 2, mesh["parameters"][:, 0]
        ][mesh["faces"].ravel()]
        mesh["surface"] = np.full(len(mesh["faces"]), 6, np.uint8)
        products[f"blade-{i}"] = mesh
    meshed = time.perf_counter()
    np.savez_compressed(output / "tables.npz", **rows)
    for name, mesh in products.items():
        np.savez_compressed(output / (name + ".npz"), **mesh)
    write_maps(output)
    result = {
        "format": "tree-entry-products-1",
        "plan_identity": IDENTITY,
        "tables": "tables.npz",
        "products": [name + ".npz" for name in products],
        "axes": 64,
        "leaves": 571,
        "table_payload_bytes": sum(a.nbytes for a in rows.values()),
        "canonical_mesh_payload_bytes": sum(
            a.nbytes for mesh in products.values() for a in mesh.values()
        ),
        "instanced_triangles": len(products["wood"]["faces"])
        + sum(
            int((rows["leaf_variants"] == i).sum())
            * len(products[f"blade-{i}"]["faces"])
            for i in range(8)
        ),
        "plan_and_reference_s": ready - began,
        "geometry_s": meshed - ready,
        "build_s": time.perf_counter() - began,
        "scope": metadata["scope"]
        + " Preview reuses the existing own fern vein field and thin material, not species-calibrated bark/leaf optics.",
    }
    (output / "recipe.json").write_text(json.dumps(metadata["recipe"], indent=2) + "\n")
    (output / "entry.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


def load(inputs, *, geometry=True):
    """Fresh complete cache read: no source plan, RNG, curve or mesh construction."""
    metadata = json.loads((inputs / "entry.json").read_text())
    products = {}
    names = [metadata["tables"], *(metadata["products"] if geometry else [])]
    for name in names:
        with np.load(inputs / name, allow_pickle=False) as data:
            products[name] = {key: data[key] for key in data.files}
    return metadata, products


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    build(parser.parse_args().output)
