"""Read a clean independently verified whole-window height result for inspection."""

import json

import numpy as np
from camera_height_inputs import digest


def window_heights(root, name, xy, faces, source_sha, camera_generation, datum):
    name = "Wien" if name == "Vienna" else name
    mode = "least_squares"
    folder = root / (name + "-" + mode)
    report = json.loads((folder / (name + "-" + mode + ".json")).read_text())
    verification = json.loads((folder / "verification.json").read_text())
    meta = json.loads((root / "inputs" / (name + "-input.json")).read_text())
    if (
        report["dirty"]
        or report["place"] != name
        or meta["bed_sha256"] != source_sha
        or meta["fixed_datum_m"] != datum
        or meta["source_binding"]["generation_sha256"] != camera_generation
        or report["input_sha256"] != meta["input_sha256"]
    ):
        raise ValueError(
            "height product belongs to another bed/source/frame or is dirty"
        )
    if not report["valid"]:
        if verification["accepted"] or not verification["original_bed_retained"]:
            raise ValueError("unverified height fallback")
        return np.full(len(xy), datum), {
            "valid_fit": False,
            "retained_original_bed": True,
            "reason": verification["reason"],
            "generation_commit": report["commit"],
        }
    path = folder / (name + "-" + mode + ".npz")
    if (
        not verification["accepted"]
        or verification["generation_dirty"]
        or verification["generation_commit"] != report["commit"]
        or verification["mesh_sha256"] != report["mesh_sha256"]
        or digest(path) != report["mesh_sha256"]
    ):
        raise ValueError("clean complete independently verified height mesh required")
    with np.load(path) as data:
        if not np.array_equal(data["xy"], xy) or not np.array_equal(
            data["faces"], faces
        ):
            raise ValueError("height product changed original XY/topology")
        heights = data["heights"]
    return heights, {
        "valid_fit": True,
        "mesh_sha256": digest(path),
        "verification_sha256": digest(folder / "verification.json"),
        "generation_commit": report["commit"],
        "scope": "Whole-face constrained experimental bed heights; existing whole rail-support facets remain at their declared datum. Analytic C1, tiers, traffic and terrain exterior remain open.",
    }
