"""Stable exact foliage budget and deferred canonical end-shoot controls."""

import unittest
from unittest.mock import patch

import numpy as np
from crown_attractors import attractors
from crown_shoots import CrownShoots
from outshine_ref.vegetation import Shoot


def small(variants=None):
    positions = np.array(
        ((0, 0, 0), (0, 0, 2), (-1, 0, 3), (1, 0.2, 3), (0, 1, 3)), float
    )
    parents = np.array((-1, 0, 1, 1, 1))
    variants = np.arange(17, dtype=np.uint8) % 3 if variants is None else variants
    return CrownShoots(positions, parents, variants)


class CrownShootChecks(unittest.TestCase):
    def test_asymmetric_growth_space_keeps_seed_count_and_macro_bounds(self):
        for seed in (718, 719, 720):
            simple = attractors(2048, seed, "ellipsoid")
            shaped = attractors(2048, seed)
            self.assertEqual(shaped.shape, (2048, 3))
            self.assertTrue(np.isfinite(shaped).all())
            np.testing.assert_allclose(shaped.min(0), simple.min(0), atol=1e-14)
            np.testing.assert_allclose(shaped.max(0), simple.max(0), atol=1e-14)
            self.assertFalse(np.array_equal(simple, shaped))
            np.testing.assert_array_equal(shaped, attractors(2048, seed))

    def test_coarse_summary_and_capacity_do_not_create_organ_poses(self):
        c = small()
        with patch.object(
            Shoot, "detail", side_effect=AssertionError("unexpected poses")
        ):
            self.assertEqual(c.summary()["leaves"], 17)
            np.testing.assert_array_equal(c.counts, (6, 6, 5))
            self.assertIsNone(c.tables(16))
            self.assertIsNone(c.group(2, 5))
        self.assertFalse(c._groups)

    def test_exact_variant_id_budget_and_reversed_subset_replay(self):
        a, b = small(), small()
        whole = a.tables(17)
        np.testing.assert_array_equal(whole["ids"], np.arange(17))
        np.testing.assert_array_equal(whole["variants"], a.variants)
        for node in b.tips[::-1]:
            part = b.group(int(node), 17)
            mask = whole["owner_nodes"] == node
            for key, values in part.items():
                np.testing.assert_array_equal(values, whole[key][mask])
        self.assertEqual(a.identity, b.identity)
        self.assertIs(a.group(2, 6), a.group(2, 6))
        with self.assertRaises(ValueError):
            a.group(2, 6)["origins"].setflags(write=True)

    def test_rigid_right_handed_frames_and_exact_petiole_anchors(self):
        c = small()
        rows = c.tables(17)
        f = rows["frames"]
        np.testing.assert_allclose(
            np.einsum("nji,njk->nik", f, f),
            np.broadcast_to(np.eye(3), f.shape),
            atol=1e-14,
        )
        np.testing.assert_allclose(np.linalg.det(f), 1, atol=1e-14)
        np.testing.assert_allclose(
            rows["origins"] - c.petiole_m * f[:, :, 1], rows["nodes"], atol=1e-14
        )
        for node in c.tips:
            origin, frame = c.stem(int(node))
            np.testing.assert_array_equal(origin, c.positions[node])
            direction = c.positions[node] - c.positions[c.parents[node]]
            np.testing.assert_allclose(
                frame[:, 1], direction / np.linalg.norm(direction), atol=1e-14
            )

    def test_empty_partial_and_independent_source_arrays(self):
        c = small(np.array([], dtype=np.uint8))
        self.assertEqual(len(c.tables(0)["ids"]), 0)
        source = np.arange(17, dtype=np.uint8) % 3
        c = small(source)
        identity = c.identity
        source[:] = 63
        self.assertEqual(c.identity, identity)
        np.testing.assert_array_equal(c.variants, np.arange(17) % 3)

    def test_invalid_topology_zero_edges_variants_and_budgets(self):
        c = small()
        for variants in (np.array([-1]), np.array([64]), np.array([0.5])):
            with self.assertRaises(ValueError):
                small(variants)
        parents = c.parents.copy()
        parents[2] = 3
        with self.assertRaises(ValueError):
            CrownShoots(c.positions, parents, c.variants)
        positions = c.positions.copy()
        positions[2] = positions[1]
        with self.assertRaises(ValueError):
            CrownShoots(positions, c.parents, c.variants)
        for cap in (-1, True, 100001):
            with self.assertRaises(ValueError):
                c.tables(cap)
        with self.assertRaises(ValueError):
            c.group(1, 17)

    def test_distal_band_follows_skeleton_path_length_not_world_height(self):
        base = small()
        band = CrownShoots(
            base.positions, base.parents, base.variants, foliage_depth_m=1.5
        )
        np.testing.assert_array_equal(band.hosts, [1, 2, 3, 4])
        np.testing.assert_array_equal(band.counts, [5, 4, 4, 4])
        moved = base.positions[:, [2, 1, 0]] + np.array((7, 4, -2))
        transformed = CrownShoots(
            moved, base.parents, base.variants, foliage_depth_m=1.5
        )
        np.testing.assert_array_equal(transformed.hosts, band.hosts)
        self.assertEqual(len(band.tables(17)["ids"]), 17)


if __name__ == "__main__":
    unittest.main()
