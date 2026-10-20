"""Shared coverage, depth separation and observer-domain counterexamples."""

import json
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
import shapely
from native_capture_meshes import independent_depth
from packed_normals import pack
from region_occlusion import (
    compile_projection,
    inner_projection,
    outer_projection,
    project,
)
from region_occlusion_product import load_product, select, store


def walls(gap=False):
    edges = [(-3, -0.1 if gap else 0.5, 5), (0.1 if gap else -0.5, 3, 5), (-2, 2, 10)]
    positions = np.array(
        [
            [x, y, z]
            for low, high, z in edges
            for x, y in ((low, -3), (high, -3), (high, 3), (low, 3))
        ],
        np.float32,
    )
    indices = np.concatenate(
        [np.array([[0, 3, 2], [0, 2, 1]]) + 4 * i for i in range(3)]
    )
    binding = np.zeros((6, 6), np.int32)
    binding[:, :2] = np.repeat(np.arange(3), 2)[:, None]
    return {
        "positions": positions,
        "indices": indices,
        "binding": binding,
        "normal": pack(np.tile([0, 0, -1], (12, 1))),
    }


BOX = np.array([[-0.5, -0.2, -0.1], [0.5, 0.2, 0.1]])


class RegionTests(unittest.TestCase):
    def test_finished_selection_preserves_anonymous_identities_and_unknown_sources(
        self,
    ):
        result = compile_projection(
            walls(), BOX, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        records = [{"index": i, "kind": 0, "id": 0} for i in range(3)]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "selection"
            store(result, records, path, {"parent": "fixed"})
            product = load_product(path, [[0, 0, 0]], {"parent": "fixed"})
            np.testing.assert_array_equal(product["sources"]["ordinal"], [0, 1, 2])
            np.testing.assert_array_equal(
                select(product, [[0, 0, 0]], [2, 0, 999, -1]), [False, True, True, True]
            )
            with self.assertRaisesRegex(ValueError, "identity"):
                load_product(path, [[0, 0, 0]], {"parent": "changed"})

    def test_domain_rejection_precedes_selection_payload_io(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / "header.json").write_text(
                json.dumps(
                    {
                        "format": "opaque-region-source-selection-1",
                        "provenance": {},
                        "observer_bounds": BOX.tolist(),
                    }
                )
            )
            with self.assertRaisesRegex(ValueError, "outside supported"):
                load_product(path, [[100, 0, 0]], {})

    def test_joint_projection_can_hide_what_neither_blocker_hides(self):
        source = walls()
        result = compile_projection(
            source, BOX, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        np.testing.assert_array_equal(result["hidden"], [False, False, True])
        eyes = np.array(
            [[x, y, z] for x in BOX[:, 0] for y in BOX[:, 1] for z in BOX[:, 2]]
        )
        target = outer_projection(source["positions"][8:], eyes, 7.5, 1e-8)
        pieces = [
            inner_projection(source["positions"][4 * i : 4 * i + 4], eyes, 7.5, 1e-8)
            for i in (0, 1)
        ]
        self.assertTrue(shapely.union_all(pieces).covers(target))
        self.assertFalse(any(p.covers(target) for p in pieces))

    def test_real_gap_and_camera_disocclusion_retain_sources(self):
        source = walls(gap=True)
        result = compile_projection(
            source, BOX, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        self.assertFalse(result["hidden"].any())
        large = BOX.copy()
        large[:, 0] = [-5, 5]
        result = compile_projection(
            walls(), large, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        self.assertFalse(result["hidden"].any())

    def test_blocker_on_wrong_side_of_plane_cannot_hide(self):
        result = compile_projection(
            walls(), BOX, certified_opaque=True, tight_hulls=True, plane=4
        )
        self.assertFalse(result["hidden"].any())
        with self.assertRaisesRegex(ValueError, "opaque"):
            compile_projection(walls(), BOX)
        with self.assertRaisesRegex(ValueError, "beyond"):
            compile_projection(walls(), BOX, certified_opaque=True, plane=0)

    def test_projected_intervals_enclose_exact_rational_coordinates(self):
        rng = np.random.default_rng(913)
        points = rng.uniform([-10, -10, 3], [10, 10, 8], (15, 3))
        eyes = rng.uniform([-1, -1, -1], [1, 1, 1], (8, 3))
        low, high = project(points, eyes, 10.0)
        for i, eye in enumerate(eyes):
            for j, point in enumerate(points):
                p, e = (
                    list(map(Fraction, map(float, point))),
                    list(map(Fraction, map(float, eye))),
                )
                factor = (10 - e[2]) / (p[2] - e[2])
                for axis in (0, 1):
                    exact = e[axis] + factor * (p[axis] - e[axis])
                    self.assertLessEqual(Fraction(float(low[i, j, axis])), exact)
                    self.assertGreaterEqual(Fraction(float(high[i, j, axis])), exact)

    def test_removed_target_is_occluded_for_independent_interior_world_rays(self):
        source = walls()
        result = compile_projection(
            source, BOX, certified_opaque=True, tight_hulls=True, plane=7.5
        )
        self.assertTrue(result["hidden"][2])
        rng = np.random.default_rng(779)
        origins = rng.uniform(BOX[0], BOX[1], (3000, 3))
        targets = np.column_stack(
            (rng.uniform(-2, 2, 3000), rng.uniform(-3, 3, 3000), np.full(3000, 10))
        )
        rays = targets - origins
        triangles = source["positions"][source["indices"]]
        original = independent_depth(triangles, origins, rays)
        reduced = independent_depth(triangles[:4], origins, rays)
        np.testing.assert_array_equal(original, reduced)
        self.assertTrue((original < 1).all())


if __name__ == "__main__":
    unittest.main()
