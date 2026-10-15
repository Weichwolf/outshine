"""Append exact rail products for inspection without changing any accepted bed."""

import hashlib
import json

import numpy as np
from outshine_ref.contracts import Mesh


def append_hardware(body, slots, root, name, origin, datum, camera_generation):
    receipt = json.loads((root / "generation.json").read_text())
    verification_path = root / "verification.json"
    verification = json.loads(verification_path.read_text())
    receipt_sha = hashlib.sha256((root / "generation.json").read_bytes()).hexdigest()
    if (
        receipt["dirty"]
        or verification["diagnostic_dirty_allowed"]
        or verification["generation_sha256"] != receipt_sha
        or not verification["cases"][name]["valid"]
    ):
        raise ValueError("fresh complete clean rail verification required")
    row = receipt["cases"][name]
    if row["pins"]["generation_sha256"] != camera_generation:
        raise ValueError("rail hardware belongs to another captured camera window")
    if row["origin_lat_lon"] != origin or row["horizontal_bed_datum_m"] != datum:
        raise ValueError("rail hardware belongs to another flat bed frame/datum")
    path = root / (name + ".npz")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != row["mesh_sha256"]:
        raise ValueError("rail display product bytes changed")
    with np.load(path) as data:
        vertices, faces, materials = (
            data["vertices"],
            data["triangles"],
            data["materials"],
        )
    mesh = Mesh(
        np.vstack((body.vertices, vertices)),
        np.vstack((body.triangles, faces + len(body.vertices))),
    )
    proof = {
        "mesh_sha256": digest,
        "generation_commit": receipt["generation_commit"],
        "generation_dirty": receipt["dirty"],
        "generation_sha256": hashlib.sha256(
            (root / "generation.json").read_bytes()
        ).hexdigest(),
        "verification_sha256": hashlib.sha256(
            verification_path.read_bytes()
        ).hexdigest(),
        "accepted_pieces": row["accepted_pieces"],
        "sleeper_instances": row["sleeper_instances"],
        "added_vertices": len(vertices),
        "added_triangles": len(faces),
        "old_body_prefix_vertices": len(body.vertices),
        "old_body_prefix_triangles": len(body.triangles),
        "scope": "Baked inspection only; generator returns shared sleeper mesh and instances. Original bed vertices/indices/materials are exact prefixes. Historical plan frame transformed together with the bed; no new ENU gauge or contact certificate.",
    }
    return mesh, np.r_[slots, materials], proof
