"""Prepare complete, source-bound original geometry for field-view experiments."""

import copy
import hashlib
import json

import numpy as np
from compare_building_impostors import native_axes
from native_building_cell_reference import load_cells
from parent_surface_inputs import open_parent


def prepare_cell(data, recipe, cell, output, *, pixels=128):
    recipe = copy.deepcopy(recipe)
    request = recipe["source_request"]
    basis = np.asarray([request[k] for k in ("east", "north", "up")])
    with open_parent(data / "original-inputs.tar.gz") as parent:
        ids = np.asarray(
            [i for i, r in enumerate(parent.plans["records"]) if r["cell"] == cell],
            np.uint32,
        )
    if not len(ids):
        raise ValueError("The requested complete cell has no source records")
    mesh, receipt = load_cells(
        data / "native-cells/manifest.json",
        data / "original-inputs.tar.gz",
        {cell},
        {
            "basis": basis,
            "anchor": basis @ np.asarray(request["origin"]),
            "selected_sources": ids,
        },
    )
    if receipt["unresolved_originals"]:
        raise ValueError(
            f"Original remainder must stay: {receipt['unresolved_originals']}"
        )
    output.mkdir(parents=True, exist_ok=False)
    geometry = output / "geometry"
    geometry.mkdir()
    role = mesh["binding"][:, 4].copy()
    metres = np.any(mesh["metres"][mesh["indices"]] != 0, axis=(1, 2))
    role[(role == 0) & metres] = 2
    parts, files = [], {}
    for index in range(3):
        faces = mesh["indices"][role == index]
        vertices, inverse = np.unique(faces, return_inverse=True)
        directory = geometry / str(index)
        directory.mkdir()
        attributes = {
            "positions": native_axes(mesh["positions"][vertices]),
            "normals": native_axes(mesh["vertex_normal"][vertices]),
            "uv": mesh["uv"][vertices],
            "uv1": mesh["metres"][vertices],
            "colours": mesh["colour"][vertices],
            "indices": inverse.astype("<u4"),
        }
        for name, values in attributes.items():
            array = np.asarray(values, dtype="<u4" if name == "indices" else "<f4")
            path = directory / (name + ".bin")
            array.tofile(path)
            if not np.array_equal(np.fromfile(path, dtype=array.dtype), array.ravel()):
                raise ValueError("The serialized original attribute changed")
            files[str(path.relative_to(output))] = {
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "bytes": path.stat().st_size,
            }
        parts.append(
            {"vertices": len(vertices), "triangles": len(faces), "material": index}
        )
    positions = native_axes(mesh["positions"])
    least, most = positions.min(0).tolist(), positions.max(0).tolist()
    (geometry / "input.json").write_text(
        json.dumps({"parts": parts, "least": least, "most": most, "source": receipt})
        + "\n"
    )
    (output / "source-receipt.json").write_text(
        json.dumps(
            {"source": receipt, "complete_source_ids": ids.tolist(), "files": files}
        )
        + "\n"
    )
    request["cells"], request["pixels"] = [cell], pixels
    recipe["least"], recipe["most"] = least, most
    return recipe
