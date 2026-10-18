"""Paired finished crown products with shared source organ assets and exact counts."""

import json
import shutil

import numpy as np


def export_pair(
    inputs, output, source, posed, tables, identity, *, scope, added_wood_faces=0
):
    for name, wood, rows in (
        ("recursive", source, tables[0]),
        ("spatial", posed, tables[1]),
    ):
        folder = output / name
        folder.mkdir()
        metadata = json.loads((inputs / "spatial/entry.json").read_text())
        for file in (inputs / "spatial").iterdir():
            if file.name not in ("entry.json", "tables.npz", "wood.npz"):
                shutil.copyfile(file, folder / file.name)
        np.savez_compressed(folder / "wood.npz", **wood)
        np.savez_compressed(folder / "tables.npz", **rows)
        if name == "spatial":
            metadata["plan_identity"] = identity
            metadata["instanced_triangles"] += added_wood_faces
        metadata["scope"] = scope
        (folder / "entry.json").write_text(json.dumps(metadata, indent=2) + "\n")
