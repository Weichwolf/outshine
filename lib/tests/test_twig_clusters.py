"""Standalone attached-shoot hierarchy and independent curved-radius controls."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import Shoot, TwigClusters
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.twig_radius_basis import apply_basis, radius_basis
from scipy.spatial.transform import Rotation


def plan(counts=(25, 26, 0, 1)):
    return TwigClusters(
        np.array((7, 13, 22, 39), np.uint64),
        np.array(counts),
        np.arange(sum(counts), dtype=np.uint8) % 8,
        np.array(((0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)), float),
        np.tile(np.eye(3), (4, 1, 1)),
        "a" * 64,
    )


class TwigClusterTests(unittest.TestCase):
    def test_counts_before_any_leaf_poses_and_capacity_refusal(self):
        p = plan()
        with patch.object(
            Shoot, "detail", side_effect=AssertionError("unrequested leaves")
        ):
            self.assertEqual(p.summary()["leaves"], 52)
            self.assertEqual(p.summary()["twigs"], 11)
            self.assertIsNone(p.group(7, 24))
            self.assertIsNone(p.twigs(7, 4))
            self.assertIsNone(p.tables(51))
            self.assertEqual(len(p.twigs(7, 5)["ids"]), 5)
        self.assertFalse(p._prototypes or p._groups)

    def test_balanced_partition_canonical_ids_and_zero_remainder(self):
        p = plan()
        for i, node in enumerate(p.host_ids):
            t = p.twigs(int(node), 32)
            counts = t["counts"]
            self.assertEqual(int(counts.sum()), int(p.host_counts[i]))
            if len(counts):
                self.assertLessEqual(int(counts.max()), 6)
                self.assertLessEqual(int(np.ptp(counts)), 1)
            np.testing.assert_array_equal(
                t["ids"], int(node) * 32 + np.arange(len(counts))
            )
        rows = p.tables(52)
        np.testing.assert_array_equal(rows["ids"], np.arange(52))
        np.testing.assert_array_equal(rows["variants"], p.variants)
        for node in p.host_ids:
            a = p.group(int(node), 100)
            for key in ("ids", "variants", "origins", "frames", "nodes"):
                np.testing.assert_array_equal(a[key], rows[key][a["ids"].astype(int)])

    def test_twigs_attach_to_actual_carrier_and_fit_parent_radius(self):
        p = plan()
        t = p.twigs(7, 32)
        fraction = t["parameters"]
        angle = p.carrier_bend * fraction
        expected = (
            np.c_[
                np.zeros(len(fraction)),
                p.carrier_length / p.carrier_bend * np.sin(angle),
                p.carrier_length / p.carrier_bend * (1 - np.cos(angle)),
            ]
            + p.origins[0]
        )
        np.testing.assert_allclose(t["origins"], expected, atol=5e-17)
        np.testing.assert_allclose(
            t["radii"], 0.65 * (0.0005 + (0.00012 - 0.0005) * fraction), atol=1e-19
        )
        np.testing.assert_allclose(np.linalg.det(t["frames"]), 1, atol=1e-15)
        for n, v in zip(t["counts"], t["variants"]):
            self.assertEqual(p.shoot(int(n), int(v)).count, int(n))

    def test_shared_and_individual_shoots_match_and_partial_order_is_stable(self):
        p = plan()
        expected = plan().tables(52, reuse=False)
        for node in (39, 7, 22, 13):
            p.group(node, 100)
        actual = p.tables(52)
        for key in actual:
            np.testing.assert_array_equal(actual[key], expected[key])
        self.assertLessEqual(len(p._prototypes), 16 * p.leaf_limit)
        repeated = p.prototype(5, 2)
        self.assertIs(repeated, p.prototype(5, 2))

    def test_rigid_world_motion_and_physical_petiole_anchors(self):
        p = plan()
        q = Rotation.from_rotvec((0.7, 0.2, -0.4)).as_matrix()
        offset = (2, -3, 7)
        other = replace(p, origins=p.origins @ q.T + offset, frames=q @ p.frames)
        a, b = p.tables(52), other.tables(52)
        np.testing.assert_allclose(
            b["origins"], a["origins"] @ q.T + offset, atol=3e-15
        )
        np.testing.assert_allclose(b["frames"], q @ a["frames"], atol=2e-15)
        np.testing.assert_allclose(
            a["origins"] - p.petiole * a["frames"][:, :, 1], a["nodes"], atol=3e-16
        )
        np.testing.assert_allclose(np.linalg.det(a["frames"]), 1, atol=2e-15)

    def test_immutable_inputs_and_invalid_hierarchy_budget(self):
        p = plan()
        for a in (p.host_ids, p.host_counts, p.origins, p.frames, p.variants):
            with self.assertRaises(ValueError):
                a.flat[0] = 0
        with self.assertRaises(TypeError):
            p.group(7, 100)["ids"] = np.arange(25)
        for changes in (
            {"leaf_limit": 3},
            {"host_ids": np.array((7, 7, 22, 39))},
            {"host_counts": np.array((25, 25, 0, 1))},
            {"frames": np.zeros((4, 3, 3))},
        ):
            with self.assertRaises(ValueError):
                replace(p, **changes)
        with self.assertRaises(ValueError):
            p.prototype(7, 2)
        with self.assertRaises(ValueError):
            p.prototype(5, 16)
        with self.assertRaises(ValueError):
            p.group(8, 100)

    def test_affine_radius_basis_matches_original_kernel_and_hard_cap_normals(self):
        radii = np.array((0.000065, 0.00011, 0.0002, 0.000325, 0.0025, 0.012))
        for length in (0.12, 0.25):
            for bend in (0.0, 0.35, -0.8):
                basis = radius_basis(length, bend)
                vertices, normals = apply_basis(basis, radii)
                for i, radius in enumerate(radii):
                    expected = axis_mesh(length, bend, radius, radius * 0.3, 3, 4)
                    np.testing.assert_array_equal(basis["faces"], expected["faces"])
                    np.testing.assert_allclose(
                        vertices[i], expected["vertices"], atol=1e-12, rtol=0
                    )
                    np.testing.assert_allclose(
                        normals[i], expected["normals"], atol=1e-10, rtol=0
                    )
                np.testing.assert_allclose(
                    np.linalg.norm(normals, axis=2), 1, atol=3e-16
                )


if __name__ == "__main__":
    unittest.main()
