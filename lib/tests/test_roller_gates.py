"""Physical visible sheets, metric attributes, early omission and state bounds."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.buildings.roller_gates import RollerGate, leaf_surface, lift, profile


class RollerGates(unittest.TestCase):
    def test_closed_partial_and_open_keep_exact_state_bounds_and_volume(self):
        gate = RollerGate(3.8, 3)
        for opened in (0, 0.35, 0.5, 0.9, 0.999999, 1):
            for detail in (False, True):
                mesh = leaf_surface(gate, opened, detail=detail)
                if opened == 1:
                    self.assertEqual(mesh["vertices"].shape, (0, 3))
                    self.assertEqual(len(mesh["faces"]), 0)
                    continue
                section = profile(gate, opened, detail)
                p = mesh["vertices"][mesh["faces"]]
                volume = (
                    np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
                )
                expected = gate.width_m * np.trapezoid(section[:, 1], section[:, 0])
                self.assertAlmostEqual(volume, expected, places=11)
                np.testing.assert_allclose(
                    p.min(axis=(0, 1)), [0, lift(gate, opened), 0]
                )
                self.assertEqual(p[:, :, 1].max(), gate.height_m)
                self.assertLessEqual(
                    p[:, :, 2].max(), gate.depth_m + gate.rib_m + 1e-15
                )
                _, roots = np.unique(mesh["vertices"], axis=0, return_inverse=True)
                faces = roots[mesh["faces"]]
                edges = np.concatenate(
                    [faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]
                )
                _, inverse, counts = np.unique(
                    np.sort(edges, axis=1),
                    axis=0,
                    return_counts=True,
                    return_inverse=True,
                )
                np.testing.assert_array_equal(counts, 2)
                np.testing.assert_array_equal(
                    np.bincount(
                        inverse, weights=np.where(edges[:, 0] < edges[:, 1], 1, -1)
                    ),
                    0,
                )

    def test_every_actual_triangle_has_geometric_normals_and_metric_uv_edges(self):
        for opened in (0, 0.35, 0.5, 0.9):
            for detail in (False, True):
                mesh = leaf_surface(
                    RollerGate(3.8, 3), opened, detail=detail, material=42
                )
                p, uv = mesh["vertices"][mesh["faces"]], mesh["uv"][mesh["faces"]]
                n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                n /= np.linalg.norm(n, axis=1, keepdims=True)
                np.testing.assert_allclose(mesh["normals"], np.repeat(n, 3, axis=0))
                for a, b in ((0, 1), (1, 2), (2, 0)):
                    np.testing.assert_allclose(
                        np.linalg.norm(p[:, a] - p[:, b], axis=1),
                        np.linalg.norm(uv[:, a] - uv[:, b], axis=1),
                        atol=1e-12,
                    )
                np.testing.assert_array_equal(mesh["materials"], 42)

    def test_fully_open_does_not_triangulate_or_model_hidden_winding(self):
        with patch(
            "outshine_ref.buildings.roller_gates.extrusion",
            side_effect=AssertionError("hidden sheet"),
        ):
            self.assertEqual(len(leaf_surface(RollerGate(3.8, 3), 1)["faces"]), 0)

    def test_zero_rib_omits_invisible_slats_before_triangle_capacity(self):
        gate = RollerGate(3.8, 3, rib_m=0)
        mesh = leaf_surface(gate, max_triangles=12)
        self.assertEqual(len(mesh["faces"]), 12)
        self.assertEqual(len(profile(gate, 0)), 2)

    def test_capacity_rejected_before_profile_and_flat_direct_demand(self):
        gate = RollerGate(3.8, 3)
        with (
            patch(
                "outshine_ref.buildings.roller_gates.profile",
                side_effect=AssertionError("late capacity"),
            ),
            self.assertRaises(ValueError),
        ):
            leaf_surface(gate, max_triangles=10)
        mesh = leaf_surface(gate, detail=False, max_triangles=12)
        self.assertEqual(len(mesh["faces"]), 12)
        self.assertEqual(len(np.unique(mesh["vertices"][:, 2])), 2)

    def test_dimensions_states_material_and_capacity_fail_explicitly(self):
        gate = RollerGate(3.8, 3)
        for kwargs in (
            {"width_m": True},
            {"height_m": float("nan")},
            {"pitch_m": 0.031},
            {"depth_m": 0},
            {"rib_m": 0.1},
        ):
            with self.assertRaises((ValueError, TypeError)):
                replace(gate, **kwargs)
        for state in (-0.1, 1.1, True, "0.5", float("inf")):
            with self.assertRaises((ValueError, TypeError)):
                leaf_surface(gate, state)
        for kwargs in (
            {"detail": 1},
            {"material": True},
            {"material": 65536},
            {"max_triangles": True},
            {"max_triangles": -1},
        ):
            with self.assertRaises((ValueError, TypeError)):
                leaf_surface(gate, **kwargs)
        self.assertEqual(lift(gate, 1 - 1e-9), 3)
        self.assertLess(lift(gate, 1 - 1e-6), 3)


if __name__ == "__main__":
    unittest.main()
