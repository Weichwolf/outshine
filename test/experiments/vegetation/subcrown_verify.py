"""Full-input independent growth and fresh saved-product replay checks."""

import argparse
import hashlib
import json
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from crown_shoots import CrownShoots
from demand import Camera
from oak_scaffold import OakRecipe
from shoot_form import ShootForm
from spatial_group_source import load_source
from subcrown_probe import build
from subcrown_space import sample_space


def growth_checks(recipe, pilot):
    records = {}
    for name in ("original", "raised", "limb"):
        with np.load(pilot / f"{name}.npz", allow_pickle=False) as z:
            points = z["points"]
            expected = {k: z[k] for k in ("positions", "parents", "remaining")}
        start = time.perf_counter()
        actual, _, metrics = build(recipe, points, method="brute")
        for key, value in expected.items():
            np.testing.assert_array_equal(actual[key], value)
        records[name] = {
            "exact_brute_indexed_arrays": list(expected),
            "brute_check_s_shared_load": time.perf_counter() - start,
            "queries": metrics["queries"],
        }
    return records


def replay_checks(products):
    records = {}
    for name in ("original", "limb"):
        inputs = products / name
        source = load_source(inputs)
        report = json.loads((inputs / "product.json").read_text())
        with (
            patch.object(ShootForm, "build", side_effect=AssertionError),
            patch.object(CrownShoots, "group", side_effect=AssertionError),
            patch("subcrown_space.sample_space", side_effect=AssertionError),
            patch("colonization.grow", side_effect=AssertionError),
            patch("numpy.random.Generator", side_effect=AssertionError),
        ):
            form = ShootForm.load(inputs / "form.npz", expected_source=source.identity)
            if form.identity != report["form_identity"]:
                raise AssertionError("unexpected product identity")
            rows = form.evaluate(
                np.ones(len(form.scale), bool), max_leaves=100000, anchors=True
            )
        reference = source.crown.tables(100000)
        for key in ("ids", "variants", "origins", "frames"):
            np.testing.assert_array_equal(rows[key], reference[key])
        np.testing.assert_array_equal(rows["ids"], np.arange(90000, dtype=np.uint64))
        owner = np.repeat(
            np.arange(len(form.scale)), np.diff(form.base.data["offsets"]).astype(int)
        )
        maximum = -np.inf
        vertices = 0
        for variant, blade in enumerate(source.blades):
            ids = np.flatnonzero(rows["variants"] == variant)
            mesh = blade.mesh(8, 1)
            for begin in range(0, len(ids), 512):
                selected = ids[begin : begin + 512]
                points = np.einsum(
                    "nij,vj->nvi", rows["frames"][selected], mesh["vertices"]
                )
                points += rows["origins"][selected, None]
                maximum = max(
                    maximum,
                    float((form.lower[owner[selected], None] - points).max()),
                    float((points - form.upper[owner[selected], None]).max()),
                )
                vertices += len(selected) * len(mesh["vertices"])
        if maximum > 0:
            raise AssertionError("geometry outside demand bounds")
        camera = Camera(
            (0, -26, 10),
            (0, 0, 9),
            width=2560,
            height=1440,
            fov_y=50,
            near=0.05,
            far=1000,
        )
        records[name] = {
            "source_identity": source.identity,
            "form_identity": form.identity,
            "full_pose_bit_equal": 90000,
            "checked_source_vertices": vertices,
            "maximum_signed_bound_violation_m": maximum,
            "whole_camera_host_count": int(
                camera.visible(form.lower, form.upper).sum()
            ),
            "pose_sha256": hashlib.sha256(
                b"".join(
                    rows[k].tobytes() for k in ("ids", "variants", "origins", "frames")
                )
            ).hexdigest(),
        }
    return records


def run(pilot, products, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    metadata = json.loads((pilot / "report.json").read_text())
    recipe = OakRecipe(**metadata["recipe"])
    for name in ("raised", "limb"):
        with np.load(pilot / f"{name}.npz", allow_pickle=False) as z:
            np.testing.assert_array_equal(sample_space(recipe, name)[0], z["points"])
    report = {"growth": growth_checks(recipe, pilot), "replay": replay_checks(products)}
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.pilot, args.products, args.output)
