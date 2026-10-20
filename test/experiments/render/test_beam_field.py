"""Independent rays and rational controls for conservative adaptive beam candidates."""

import json
import tempfile
import unittest
from fractions import Fraction
from pathlib import Path

import numpy as np
from beam_field_plan import build
from beam_field_product import cook, load_product, query, route
from beam_intervals import Beams, divide, mul
from beam_patch_fusion import Patches
from native_capture_meshes import independent_depth
from packed_normals import pack, unpack


def scene(*, raised=0):
    positions = np.array(
        [[-4, -4, 10], [4, -4, 10], [4, 4, 10 + raised], [-4, 4, 10]], np.float32
    )
    normal = pack(np.tile([0.0, 0.0, -1.0], (4, 1)))
    return {
        "positions": positions,
        "indices": np.array([[0, 3, 2], [0, 2, 1]]),
        "normal": normal,
        "vertex_normal": unpack(normal),
        "uv": np.tile([256.5, 0.25], (4, 1)).astype(np.float32),
        "metres": np.zeros((4, 2), np.float32),
        "colour": np.ones((4, 4), np.float32),
        "binding": np.zeros((2, 6), np.int32),
    }


BOX = np.array([[-1, -1, -0.2], [1, 1, 0.2]], float)
PROFILE = {"width": 1280, "height": 720, "focal_px": 700.0}


class BeamTests(unittest.TestCase):
    def test_background_remains_visible_through_a_gap(self):
        source = scene()
        rectangles = [(-4, -0.5, 4, 10), (0.5, 4, 4, 10), (-8, 8, 8, 20)]
        source["positions"] = np.array(
            [
                [x, y, z]
                for low, high, half, z in rectangles
                for x, y in ((low, -half), (high, -half), (high, half), (low, half))
            ],
            np.float32,
        )
        source["indices"] = np.concatenate(
            [source["indices"] + 4 * i for i in range(3)]
        )
        for key in ("normal", "vertex_normal", "uv", "metres", "colour"):
            source[key] = np.concatenate([source[key]] * 3)
        source["binding"] = np.zeros((6, 6), np.int32)
        source["binding"][:, :2] = np.repeat(np.arange(3), 2)[:, None]
        rng = np.random.default_rng(22)
        origins = rng.uniform(BOX[0], BOX[1], (3000, 3))
        directions = np.column_stack(
            (rng.uniform(-0.55, 0.55, (3000, 2)), np.ones(3000))
        )
        origins[0], directions[0] = [0, 0, 0], [0, 0, 1]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "field"
            cook(source, BOX, PROFILE, path, {}, certified_opaque=True)
            product = load_product(path, origins, {})
            actual, _, owners, _ = query(product, origins, directions)
            expected = independent_depth(
                source["positions"][source["indices"]], origins, directions
            )
            np.testing.assert_array_equal(np.isfinite(actual), np.isfinite(expected))
            covered = np.isfinite(expected)
            np.testing.assert_allclose(actual[covered], expected[covered], atol=1e-12)
            self.assertEqual(actual[0], 20)
            self.assertEqual(owners[0], 2)

    def test_coincident_original_tie_keeps_first_material(self):
        source = scene()
        source["positions"] = np.concatenate([source["positions"]] * 2)
        source["indices"] = np.concatenate([source["indices"], source["indices"] + 4])
        for key in ("normal", "vertex_normal", "uv", "metres", "colour"):
            source[key] = np.concatenate([source[key]] * 2)
        source["uv"][4:, 0] += 256
        source["binding"] = np.zeros((4, 6), np.int32)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "field"
            cook(source, BOX, PROFILE, path, {}, certified_opaque=True)
            product = load_product(path, [[0, 0, 0]], {})
            depth, fields, _, original = query(product, [[0, 0, 0]], [[0, 0, 1]])
            self.assertEqual(depth[0], 10)
            self.assertEqual(original[0], 0)
            self.assertEqual(fields["code"][0], 1)

    def test_interval_products_and_quotients_cover_exact_fractions(self):
        rng = np.random.default_rng(123)
        for _ in range(500):
            a = np.sort(rng.uniform(-1e4, 1e4, 2))
            b = np.sort(rng.uniform(1e-7, 1e4, 2)) * rng.choice([-1, 1])
            b.sort()
            for function, operation in (
                (mul, lambda x, y: x * y),
                (divide, lambda x, y: x / y),
            ):
                low, high = function((a[0], a[1]), (b[0], b[1]))
                for x in a:
                    for y in b:
                        exact = operation(Fraction(float(x)), Fraction(float(y)))
                        self.assertLessEqual(Fraction(float(low)), exact)
                        self.assertGreaterEqual(Fraction(float(high)), exact)

    def test_joint_patch_covers_moving_diagonal_without_one_triangle_cover(self):
        source = scene(raised=0.01)
        beams = Beams(source["positions"], source["indices"], BOX)
        cell = np.array([[-0.01, -0.01], [0.01, 0.01]])
        _, full, *_ = beams.classify(np.arange(2), cell)
        self.assertFalse(full.any())
        patches = Patches(source, beams)
        self.assertEqual(len(patches.pairs), 1)
        possible, covered = patches.classify(np.arange(1), cell)
        self.assertTrue(possible[0] and covered[0])
        rng = np.random.default_rng(4)
        origins = rng.uniform(BOX[0], BOX[1], (2000, 3))
        directions = (
            np.column_stack((rng.uniform(cell[0], cell[1], (2000, 2)), np.ones(2000)))
            @ beams.basis
        )
        depth = independent_depth(
            source["positions"][source["indices"]], origins, directions
        )
        self.assertTrue(np.isfinite(depth).all())

    def test_concave_patch_does_not_claim_convex_coverage(self):
        source = scene()
        source["positions"][2] = [-3, -3, 10]
        beams = Beams(source["positions"], source["indices"], BOX)
        self.assertEqual(len(Patches(source, beams).pairs), 0)

    def test_explicit_opacity_and_budget_contract(self):
        with self.assertRaisesRegex(ValueError, "opaque"):
            build(scene(), BOX, PROFILE)
        for budget in (0, 65537):
            with self.assertRaises(ValueError):
                build(scene(), BOX, PROFILE, max_nodes=budget, certified_opaque=True)
        for budget in (1, 5, 17, 129):
            arrays, work = build(
                scene(), BOX, PROFILE, max_nodes=budget, certified_opaque=True
            )
            self.assertLessEqual(len(arrays["children"]), budget)
            self.assertEqual(work["program_references"], len(arrays["references"]))

    def test_finished_product_matches_independent_rays_in_domain(self):
        source = scene(raised=0.01)
        rng = np.random.default_rng(8)
        origins = rng.uniform(BOX[0], BOX[1], (5000, 3))
        targets = rng.uniform([-6, -4, 10], [6, 4, 10], (5000, 3))
        directions = targets - origins
        directions /= directions[:, 2, None]
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "field"
            cook(source, BOX, PROFILE, path, {}, certified_opaque=True)
            product = load_product(path, origins, {})
            actual, fields, _, _ = query(product, origins, directions)
            expected = independent_depth(
                source["positions"][source["indices"]], origins, directions
            )
            np.testing.assert_array_equal(np.isfinite(actual), np.isfinite(expected))
            hit = np.isfinite(expected)
            np.testing.assert_allclose(actual[hit], expected[hit], atol=1e-12)
            np.testing.assert_array_equal(fields["code"][hit], 1)
            np.testing.assert_allclose(
                fields["coordinates"][hit],
                np.tile([0.5, 0.25], (hit.sum(), 1)),
                atol=1e-12,
            )
            np.testing.assert_allclose(fields["colour"][hit], 1)

    def test_missing_payload_rejects_unmet_domain_first(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / "header.json").write_text(
                json.dumps(
                    {
                        "format": "adaptive-opaque-beam-field-1",
                        "provenance": {},
                        "observer_bounds": BOX.tolist(),
                    }
                )
            )
            with self.assertRaisesRegex(ValueError, "outside supported domain"):
                load_product(path, [[2, 0, 0]], {})

    def test_route_root_boundary_and_opposite_directions(self):
        source = scene()
        arrays, _ = build(source, BOX, PROFILE, max_nodes=5, certified_opaque=True)
        q = arrays["bounds"]
        directions = np.array([[*q[0], 1], [*q[1], 1], [0, 0, -1]]) @ arrays["basis"]
        rays, nodes, _ = route(arrays, np.zeros((3, 3)), directions)
        np.testing.assert_array_equal(rays, [0, 1])
        self.assertTrue(np.all(arrays["children"][nodes, 0] < 0))


if __name__ == "__main__":
    unittest.main()
