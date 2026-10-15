"""Analytic visibility/depth cases, unchanged legacy buffers and cached view checks."""

import argparse
import json
import subprocess
import time
import types
from unittest.mock import patch

import numpy as np
from needle_group_experiment import BASE, HERE, OUTPUT, digest
from needle_group_reprojection import reproject
from needle_group_source import source_geometry
from needle_group_views import load_views, projection, render_view
from outshine_ref.vegetation import Needle, NeedledPlant
from plant_depth import depth_mesh


def analytic_first_hit():
    square = np.array(((-1, -1), (1, -1), (1, 1), (-1, 1)))
    source = {
        "vertices": np.concatenate(
            [np.column_stack((square, np.full(4, z))) for z in (-0.2, 0.1)]
        ),
        "faces": np.array(((0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7))),
        "normals": np.broadcast_to((0, 0, 1), (4, 3, 3)).copy(),
        "colour": np.repeat(np.array(((0.8, 0.1, 0), (0.2, 0.4, 0.6))), 2, axis=0)[
            :, None
        ].repeat(3, axis=1),
        "roughness": np.full((4, 3, 1), 0.8),
    }
    view = render_view(source, (0, 0, 1), 16, samples=4)
    coordinates = np.linspace(-1.1, 1.1, 64, endpoint=False) + 2.2 / 128
    covered = (abs(coordinates[:, None]) < 1) & (abs(coordinates[None, :]) < 1)
    expected = covered.reshape(16, 4, 16, 4).mean((1, 3))
    np.testing.assert_array_equal(view["rgba"][..., 3], expected)
    np.testing.assert_allclose(
        view["rgba"][..., :3], expected[..., None] * (0.2, 0.4, 0.6), atol=3e-8
    )
    np.testing.assert_allclose(view["depth"][..., 0], expected * 0.1, atol=3e-9)
    np.testing.assert_allclose(
        view["roughness_fourth"][..., 0], expected * 0.8**4, atol=5e-8
    )
    reverse = dict(source)
    for field in ("faces", "normals", "colour", "roughness"):
        reverse[field] = source[field][::-1]
    other = render_view(reverse, (0, 0, 1), 16, samples=4)
    for field in ("rgba", "normal", "depth", "raw_samples"):
        np.testing.assert_array_equal(view[field], other[field])
    np.testing.assert_array_equal(view["raw_samples"][..., 8].mean(axis=-1), expected)
    for direction in ((1, 0, 0), (0, 0, 1), (1, 2, -3)):
        frame, _, _ = projection(source["vertices"], direction)
        np.testing.assert_allclose(frame.T @ frame, np.eye(3), atol=5e-16)
        np.testing.assert_allclose(
            frame[:, 2], np.array(direction) / np.linalg.norm(direction), atol=5e-16
        )
    for direction in ((0, 0, 0), (np.nan, 0, 1), (1, 2)):
        try:
            projection(source["vertices"], direction)
        except ValueError:
            continue
        raise AssertionError("invalid direction accepted")


def legacy_depth_buffers():
    text = subprocess.check_output(
        ["git", "show", "44403307c:test/experiments/vegetation/plant_depth.py"],
        text=True,
    )
    previous = types.ModuleType("previous_depth")
    exec(text, previous.__dict__)
    image = np.random.default_rng(154).random((16, 16, 9))
    image[:3, :, 3] = 0
    lower, upper = np.array((-0.4, -0.8)), np.array((0.9, 1.1))
    for grid in (1, 4, 8, 16):
        before = previous.depth_mesh(image, np.eye(3), lower, upper, grid)
        after = depth_mesh(image, np.eye(3), lower, upper, grid)
        for key in before:
            np.testing.assert_array_equal(before[key], after[key])
    image[..., 3], image[..., 8] = 0.5, 0.125
    for grid in (1, 4, 8, 16):
        mesh = depth_mesh(image, np.eye(3), lower, upper, grid, anchored=False)
        np.testing.assert_array_equal(
            mesh["vertices"][:, 2], np.full(len(mesh["vertices"]), 0.25)
        )


def constant_depth_reprojection():
    vertices = np.array(((-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)))
    source = {"vertices": vertices}
    frame, lower, upper = projection(vertices, (0, 0, 1))
    rgba = np.broadcast_to((0.2, 0.4, 0.6, 1), (16, 16, 4))
    view = {
        "frame": frame,
        "lower": lower,
        "upper": upper,
        "rgba": rgba,
        "normal": np.broadcast_to((0, 0, 1), (16, 16, 3)),
        "roughness_fourth": np.full((16, 16, 1), 0.8**4),
        "depth": np.full((16, 16, 1), 0.25),
    }
    for grid in (1, 4, 8, 16):
        image = reproject(view, source, (0, 0, 1), 16, grid=grid)
        np.testing.assert_allclose(image["rgba"], rgba, atol=1e-14)
        np.testing.assert_array_equal(image["depth"], view["depth"])
        np.testing.assert_allclose(image["normal"], view["normal"], atol=1e-14)


def cache_checks():
    results = []
    records = json.loads((OUTPUT / "angular-report.json").read_text())
    with (
        patch.object(Needle, "mesh", side_effect=AssertionError("cached meshing")),
        patch.object(
            NeedledPlant, "foliage", side_effect=AssertionError("cached organs")
        ),
    ):
        for record in records:
            for name, sha in record["implementation_sha256"].items():
                assert digest(HERE / name) == sha, name
            for name, sha in record["input_sha256"].items():
                assert digest(HERE.parents[2] / name) == sha, name
            assert (
                digest(HERE.parents[2] / record["cache_path"]) == record["cache_sha256"]
            )
            began = time.perf_counter()
            views = load_views(record["cache_path"], record["plan_identity"])
            elapsed = time.perf_counter() - began
            source = source_geometry(BASE / record["species"])
            assert source["ids"] == [tuple(i) for i in record["organ_ids"]]
            assert source["identity"] == record["plan_identity"]
            assert source["axis_path"] == record["axis_path"]
            assert len(set(source["ids"])) == len(source["ids"])
            with np.load(record["cache_path"], allow_pickle=False) as stored:
                np.testing.assert_array_equal(stored["axis_path"], source["axis_path"])
                assert json.loads(str(stored["organ_ids"])) == record["organ_ids"]
            try:
                load_views(record["cache_path"], "wrong-plan-identity")
            except ValueError:
                pass
            else:
                raise AssertionError("different plan identity accepted")
            for view in views:
                raw = view["raw_samples"]
                assert raw.shape == (64, 64, 16, 9) and raw.dtype == np.float32
                np.testing.assert_array_equal(
                    raw[..., 8].mean(axis=-1), view["rgba"][..., 3]
                )
                assert all(
                    np.isfinite(a).all() and not a.flags.writeable
                    for a in view.values()
                    if isinstance(a, np.ndarray)
                )
            results.append(
                {
                    "species": record["species"],
                    "complete_view_decode_s": elapsed,
                    "decoded_bytes": record["cache_decoded_array_bytes"],
                }
            )
    (OUTPUT / "cache-checks.json").write_text(json.dumps(results, indent=2) + "\n")
    print("four complete cache decodes; placement and meshing disabled", results)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", action="store_true")
    args = parser.parse_args()
    analytic_first_hit()
    legacy_depth_buffers()
    constant_depth_reprojection()
    if args.cache:
        cache_checks()
    print(
        "analytic first-hit, frame, raw coverage and legacy depth buffer checks passed"
    )


if __name__ == "__main__":
    main()
