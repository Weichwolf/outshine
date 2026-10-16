"""Read finished grass contacts without roots, blades or raster reconstruction."""

import argparse
import hashlib
import json
from pathlib import Path
from unittest.mock import patch

import numpy as np


def read_product(inputs):
    report = json.loads((inputs / "result.json").read_text())
    if report["format"] != "ground-anchored-grass-contacts-1":
        raise ValueError("matching grass-contact schema required")
    for filename, digest in report["hashes"].items():
        if Path(filename).name != filename:
            raise ValueError("flat cached input filename required")
        if hashlib.sha256((inputs / filename).read_bytes()).hexdigest() != digest:
            raise ValueError("cached contact input hash changed")
    required = {
        "stand.npz",
        "stand.json",
        *[f"{v['name']}.npz" for v in report["views"]],
    }
    if not required <= report["hashes"].keys():
        raise ValueError("complete saved geometry/plan/views required")
    planned = json.loads((inputs / "stand.json").read_text())
    identity = hashlib.sha256(
        json.dumps(planned["recipe"], sort_keys=True).encode()
    ).hexdigest()
    if identity != planned["identity"] or identity != report["stand_identity"]:
        raise ValueError("canonical source identity changed")
    with np.load(inputs / "stand.npz", allow_pickle=False) as data:
        stand = dict(data)
    views = {}
    for row in report["views"]:
        with np.load(inputs / f"{row['name']}.npz", allow_pickle=False) as data:
            views[row["name"]] = dict(data)
    return report, stand, views


def check_product(inputs, output):
    # A hit is a finished read even in a process containing generator dependencies.
    with (
        patch(
            "grass_projection_stand.build_stand",
            side_effect=AssertionError("replanned stand"),
        ),
        patch(
            "outshine_ref.vegetation.ground_cover.GroundCoverPlan.sites",
            side_effect=AssertionError("replaced roots"),
        ),
        patch(
            "outshine_ref.vegetation.graminoid.graminoid_tuft_mesh",
            side_effect=AssertionError("remeshed blades"),
        ),
        patch(
            "grass_projection.contacts",
            side_effect=AssertionError("rerasterized contacts"),
        ),
    ):
        report, stand, views = read_product(inputs)
        for view in views.values():
            face, coverage = view["face"], view["coverage"]
            np.testing.assert_array_equal(face >= 0, coverage.astype(bool))
            used = face[face >= 0]
            if np.any(used >= len(stand["faces"])):
                raise ValueError("missing cached source triangle")
            if not np.isfinite(view["height"]).all() or np.any(view["height"] < 0):
                raise ValueError("finite nonnegative saved contact heights required")
            np.testing.assert_allclose(
                np.linalg.norm(view["normal"][coverage > 0], axis=1), 1, atol=1e-12
            )
            roots = stand["face_root"][used]
            if np.any(roots >= len(stand["site_ids"])):
                raise ValueError("missing original canonical root identity")
            means = [view[k].mean((0, 1)) for k in view if k.startswith("moments_")]
            for mean in means:
                np.testing.assert_allclose(mean, means[0], rtol=0, atol=1e-13)
    result = {
        "stand_identity": report["stand_identity"],
        "source_roots": len(stand["site_ids"]),
        "views": list(views),
        "generation_forbidden": True,
    }
    output.write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    check_product(args.inputs, args.output)
