"""Original row equivalence, refusal, finished reader and separate Float32 controls."""

import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compact_crown import CompactCrown
from compact_crown_kernel import frame32
from outshine_ref.vegetation.shoot_plan import normal_frame
from spatial_group_source import SpatialGroupSource
from spatial_source_checks import fixture


class CompactCrownChecks(unittest.TestCase):
    def test_parameter_build_needs_no_original_leaf_poses_or_meshes(self):
        source = fixture()
        with (
            patch(
                "crown_shoots.CrownShoots.group", side_effect=AssertionError("poses")
            ),
            patch("outshine_ref.vegetation.Shoot.detail", side_effect=AssertionError),
            patch("outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError),
        ):
            product = CompactCrown.build(source)
        self.assertEqual(product.identity, source.identity)
        self.assertFalse(source.crown._groups)
        self.assertFalse(source.prototypes)
        for value in product.data.values():
            self.assertFalse(value.flags.writeable)

    def test_float64_rows_match_original_groups_for_bends_and_unequal_counts(self):
        original = fixture()
        for bend in (-1, 0, 1):
            crown = replace(
                original.crown, bend_rad=bend, variants=original.crown.variants[:-1]
            )
            source = SpatialGroupSource(crown, original.blades)
            product = CompactCrown.build(source)
            expected = [crown.group(int(node), 100000) for node in crown.hosts]
            actual = product.evaluate(
                np.ones(len(crown.hosts), bool), max_leaves=100000, anchors=True
            )
            for key in actual:
                np.testing.assert_array_equal(
                    actual[key], np.concatenate([v[key] for v in expected])
                )
                if actual[key].dtype == np.dtype(np.float64):
                    np.testing.assert_array_equal(
                        actual[key].view(np.uint64),
                        np.concatenate([v[key] for v in expected]).view(np.uint64),
                    )

    def test_demanded_subsets_and_batch_sizes_preserve_exact_rows(self):
        product = CompactCrown.build(fixture())
        mask = np.arange(len(product.data["hosts"])) % 2 == 0
        full = product.evaluate(np.ones(len(mask), bool), max_leaves=100000)
        rows = np.repeat(mask, np.diff(product.data["offsets"]).astype(int))
        for size in (1, 2, 256):
            subset = product.evaluate(mask, max_leaves=100000, batch_hosts=size)
            for key in full:
                np.testing.assert_array_equal(subset[key], full[key][rows])
                self.assertFalse(subset[key].flags.writeable)

    def test_refusal_and_empty_demand_precede_local_pose_evaluation(self):
        product = CompactCrown.build(fixture())
        mask = np.ones(len(product.data["hosts"]), bool)
        with patch("compact_crown_kernel.local_poses", side_effect=AssertionError):
            self.assertIsNone(product.evaluate(mask, max_leaves=0))
            result = product.evaluate(~mask, max_leaves=0)
        self.assertEqual(result["frames"].shape, (0, 3, 3))

    def test_zero_leaf_plan_has_complete_empty_results(self):
        original = fixture()
        source = SpatialGroupSource(
            replace(original.crown, variants=np.empty(0, np.uint8)), original.blades
        )
        product = CompactCrown.build(source)
        actual = product.evaluate(
            np.ones(len(product.data["hosts"]), bool), max_leaves=0, anchors=True
        )
        self.assertTrue(all(len(v) == 0 for v in actual.values()))

    def test_finished_reader_never_builds_or_repeats_randomness(self):
        product = CompactCrown.build(fixture())
        mask = np.ones(len(product.data["hosts"]), bool)
        with tempfile.TemporaryDirectory(
            prefix="outshine-compact-check-", dir="/tmp"
        ) as d:
            path = Path(d) / "parameters.npz"
            product.save(path)
            with (
                patch.object(CompactCrown, "build", side_effect=AssertionError),
                patch("numpy.random.default_rng", side_effect=AssertionError),
                patch(
                    "outshine_ref.vegetation.Shoot.detail", side_effect=AssertionError
                ),
            ):
                restored = CompactCrown.load(path, expected_source=product.identity)
                result = restored.evaluate(mask, max_leaves=100000)
            for key, value in product.evaluate(mask, max_leaves=100000).items():
                np.testing.assert_array_equal(result[key], value)
            with self.assertRaises(ValueError):
                CompactCrown.load(path, expected_source="another-source")

    def test_float32_is_explicit_and_keeps_ids_and_small_fixture_errors(self):
        product = CompactCrown.build(fixture())
        mask = np.ones(len(product.data["hosts"]), bool)
        exact = product.evaluate(mask, max_leaves=100000)
        approx = product.quantized().evaluate(mask, max_leaves=100000)
        for key in ("ids", "variants"):
            np.testing.assert_array_equal(approx[key], exact[key])
        for key, limit in (("origins", 1e-5), ("frames", 1e-4)):
            self.assertEqual(approx[key].dtype, np.dtype(np.float32))
            self.assertLess(float(abs(approx[key] - exact[key]).max()), limit)
        self.assertEqual(product.data["phases"].dtype, np.dtype(np.float64))

    def test_float32_frame_poles_have_finite_orthonormal_fallbacks(self):
        directions = np.array(((0, 0, 1), (0, 0, -1), (1e-12, 0, 1), (1, 0, 0)))
        result = frame32(directions.astype(np.float32))
        self.assertTrue(np.isfinite(result).all())
        np.testing.assert_allclose(
            result, normal_frame(directions, (0, 0, 1)), atol=2e-7
        )
        np.testing.assert_allclose(
            result.swapaxes(-1, -2) @ result,
            np.broadcast_to(np.eye(3), result.shape),
            atol=2e-7,
        )

    def test_invalid_capacity_mask_and_batch_do_not_start_pose_work(self):
        product = CompactCrown.build(fixture())
        valid = np.ones(len(product.data["hosts"]), bool)
        with patch("compact_crown_kernel.local_poses", side_effect=AssertionError):
            for cap in (-1, 100001, True):
                with self.assertRaises(ValueError):
                    product.evaluate(valid, max_leaves=cap)
            for mask in (valid.astype(int), valid[:-1]):
                with self.assertRaises(ValueError):
                    product.evaluate(mask, max_leaves=100000)
            for size in (0, 4097, True):
                with self.assertRaises(ValueError):
                    product.evaluate(valid, max_leaves=100000, batch_hosts=size)


if __name__ == "__main__":
    unittest.main()
