"""Independent rigid-pose, dense sweep and state/input counterexamples for hinged leaves."""

import unittest
from dataclasses import FrozenInstanceError

import numpy as np
import shapely
from outshine_ref.buildings.interiors.doors import DoorLeaf, pose, sweep


class DoorGeometry(unittest.TestCase):
    def leaf(self, side=-1):
        return DoorLeaf(
            "door/leaf", (0.0, 0.0), (1.0, 0.0), (0.0, 1.0), 0.16, hinge_side=side
        )

    def test_closed_and_open_leaves_keep_dimensions_and_clear_portal_width(self):
        for side in (-1, 1):
            leaf = self.leaf(side)
            self.assertAlmostEqual(pose(leaf, 0).area, 1.15 * 0.04)
            for angle in (0.0, 13.7, 45.0, 89.999, 90.0):
                footprint = pose(leaf, angle)
                self.assertAlmostEqual(footprint.area, 1.15 * 0.04)
                distance = np.sort(
                    np.linalg.norm(
                        np.asarray(footprint.exterior.coords)[:-1] - leaf.hinge, axis=1
                    )
                )
                np.testing.assert_allclose(
                    distance,
                    [0.02, 0.02, np.hypot(1.15, 0.02), np.hypot(1.15, 0.02)],
                    rtol=1e-12,
                    atol=1e-12,
                )
            central = shapely.box(-0.55, -0.1, 0.55, 1.4)
            self.assertLess(pose(leaf, 90).intersection(central).area, 1e-10)
            self.assertGreater(pose(leaf, 0).intersection(central).area, 0.04)

    def test_conservative_envelope_contains_dense_intermediate_poses(self):
        for side in (-1, 1):
            leaf = self.leaf(side)
            for method in ("single-hull", "segmented"):
                envelope, report = sweep(leaf, method=method)
                self.assertLessEqual(report["segments"], 128)
                if method == "segmented":
                    self.assertLessEqual(
                        report["maximum_point_chord_deviation_m"], 0.001
                    )
                for angle in np.linspace(0, 90, 1001):
                    self.assertTrue(
                        envelope.covers(pose(leaf, float(angle))),
                        str((side, method, angle)),
                    )

    def test_endpoint_hull_without_the_required_margin_misses_mid_motion(self):
        leaf = self.leaf()
        endpoints = shapely.union_all([pose(leaf, 0), pose(leaf, 90)]).convex_hull
        self.assertGreater(pose(leaf, 45).difference(endpoints).area, 0.001)
        conservative, report = sweep(leaf, method="single-hull")
        self.assertTrue(conservative.covers(pose(leaf, 45)))
        self.assertGreater(report["maximum_point_chord_deviation_m"], 0.3)
        refined, _ = sweep(leaf)
        self.assertLess(refined.area, conservative.area)

    def test_rigid_frame_change_keeps_dense_containment(self):
        angle = np.deg2rad(31.0)
        rotation = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        leaf = DoorLeaf(
            "transformed",
            (112.0, -418.0),
            tuple(rotation @ [1.0, 0.0]),
            tuple(rotation @ [0.0, 1.0]),
            0.16,
        )
        envelope, _ = sweep(leaf)
        for angle in np.linspace(0, 90, 101):
            self.assertTrue(envelope.covers(pose(leaf, float(angle))))
        self.assertEqual(sweep(leaf)[0].wkb, sweep(leaf)[0].wkb)

    def test_plan_owns_its_frame_and_state_does_not_replace_identity(self):
        centre = [1.0, 2.0]
        leaf = DoorLeaf("owned", centre, [1.0, 0.0], [0.0, 1.0], 0.16)
        before = pose(leaf, 0).wkb
        centre[0] = 99.0
        self.assertEqual(pose(leaf, 0).wkb, before)
        self.assertEqual(leaf.id, "owned")
        self.assertNotEqual(pose(leaf, 0).wkb, pose(leaf, 90).wkb)
        with self.assertRaises(FrozenInstanceError):
            leaf.hinge_side = 1

    def test_limits_and_invalid_frames_do_not_relax_the_sweep_bound(self):
        for angle in (-1.0, 91.0, float("nan"), float("inf"), True):
            with self.assertRaises(ValueError):
                pose(self.leaf(), angle)
        for error in (0.0, -1.0, float("nan"), 0.1):
            with self.assertRaises(ValueError):
                sweep(self.leaf(), error)
        with self.assertRaises(ValueError):
            sweep(self.leaf(), 1e-8)
        with self.assertRaises(ValueError):
            sweep(self.leaf(), max_segments=1)
        with self.assertRaises(ValueError):
            DoorLeaf("bad", (0, 0), (1, 0), (1, 0), 0.16)
        with self.assertRaises(ValueError):
            DoorLeaf("bad", (0, 0), (1, 0), (0, 1), 0.16, hinge_side=True)


if __name__ == "__main__":
    unittest.main()
