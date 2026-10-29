"""Prepare complete, source-bound original geometry for field-view experiments."""

import copy
import hashlib
import json

import numpy as np
from compare_building_impostors import native_axes, write_native_parts
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
    parts = write_native_parts(mesh, geometry)
    for index, part in enumerate(parts):
        part["material"] = index
    files = {
        str(path.relative_to(output)): {
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": path.stat().st_size,
        }
        for path in sorted(geometry.glob("*/*.bin"))
    }
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
