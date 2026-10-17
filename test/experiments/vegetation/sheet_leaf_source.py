"""Six original near-tree leaves, prepared optical vertices and original owner IDs."""

import hashlib
import json
from importlib.resources import files

import numpy as np
from outshine_ref.vegetation import Blade, LeafOpticsTable
from outshine_ref.vegetation.leaf_pigments import pigment_coordinates
from tree_entry import REFERENCE, reference


def build_source():
    metadata, rows = reference()
    selected = np.flatnonzero(rows["leaf_axis_ids"] == rows["leaf_axis_ids"][0])
    if not 1 <= len(selected) <= 8:
        raise ValueError(
            "one bounded original leaf owner, without whole-tree expansion"
        )
    optics_path = files("outshine_ref.vegetation") / "data/leaf-optics-table-1.json"
    table = LeafOpticsTable.from_recipe(json.loads(optics_path.read_text()))
    vertices, faces, normals, optical, owners = [], [], [], [], []
    offset = 0
    for index in selected:
        variant, identity = (
            int(rows["leaf_variants"][index]),
            int(rows["leaf_ids"][index]),
        )
        blade = Blade(**metadata["recipe"]["blades"][variant])
        mesh = blade.mesh(
            metadata["geometry"]["leaf_segments"], metadata["geometry"]["leaf_across"]
        )
        points = mesh["vertices"]
        pigments = pigment_coordinates(
            points[:, :2], np.zeros(len(points)), identity + 121, 45, 0.2
        )
        optical.append(table.sample(pigments))
        frame = rows["leaf_frames"][index]
        vertices.append(points @ frame.T + rows["leaf_origins"][index])
        faces.append(mesh["faces"] + offset)
        normals.append(mesh["normals"].reshape(-1, 3, 3) @ frame.T)
        owners.append(np.full(len(mesh["faces"]), identity, np.uint64))
        offset += len(points)
    source = {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "normals": np.concatenate(normals),
        "optical": np.concatenate(optical),
        "face_leaf_ids": np.concatenate(owners),
        "surface_model": "thin-sheet",
    }
    origin = source["vertices"].mean(axis=0)
    source["vertices"] -= origin
    identity = {
        "plan_identity": metadata["plan_identity"],
        "reference_sha256": hashlib.sha256(REFERENCE.read_bytes()).hexdigest(),
        "tables_sha256": metadata["tables_sha256"],
        "optics_sha256": hashlib.sha256(optics_path.read_bytes()).hexdigest(),
        "leaf_ids": rows["leaf_ids"][selected].tolist(),
        "leaf_axis_ids": rows["leaf_axis_ids"][selected].tolist(),
        "leaf_variants": rows["leaf_variants"][selected].tolist(),
        "local_origin_enu_m": origin.tolist(),
        "pigment_state_ug_cm2": [45, 0.2],
        "pigment_ridge": "zero; existing continuous12wave patches, no new vein/bump field or species calibration",
        "vertices": len(source["vertices"]),
        "triangles": len(source["faces"]),
    }
    for value in source.values():
        if isinstance(value, np.ndarray):
            value.setflags(write=False)
    return source, identity
