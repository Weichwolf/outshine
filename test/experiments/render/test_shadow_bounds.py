"""Independent edge/counterexample controls for the native #819 proposal."""

import unittest

import numpy as np
from shadow_bounds import (
    cluster_sphere,
    outside,
    pack_sphere,
    transformed_sphere,
    world_sphere,
)
from shadow_bounds_oracle import ball_violations, containment, shader_points


class ShadowBoundsTests(unittest.TestCase):
    def test_complete_shadow_range_includes_all_levels_and_overlaps(self):
        spheres = [[0, 0, 0, 1], [40, 0, 0, 2], [0, 50, 0, 3]]
        result = cluster_sphere(spheres, [(0, 3), (3, 6), (6, 6)], 12)
        self.assertEqual(ball_violations(spheres, result), 0)
        for s in spheres:
            for axis in np.eye(3):
                self.assertEqual(
                    containment([np.array(s[:3]) + axis * s[3]], result)["outside"], 0
                )
        for ranges, count in [
            ([(3, 3)], 6),
            ([(0, 3), (6, 3)], 9),
            ([(0, 3)], 6),
            ([(0, 4)], 4),
            ([(0, 0)], 3),
            ([(0, 6)], 3),
        ]:
            self.assertIsNone(cluster_sphere(spheres[: len(ranges)], ranges, count))

    def test_invalid_bounds_never_cull(self):
        for value in [
            np.nan,
            np.inf,
            -np.inf,
            np.nextafter(np.float32(0), np.float32(1)),
        ]:
            for axis in range(4):
                sphere = np.array([0, 0, 0, 1], dtype=np.float32)
                sphere[axis] = value
                self.assertIsNone(cluster_sphere([sphere], [(0, 3)], 3))
                self.assertFalse(outside(sphere, np.eye(4), [0, 0, 0]))
        self.assertIsNone(pack_sphere([0, 0, 0], 0))
        self.assertFalse(outside(None, np.eye(4), [0, 0, 0]))

    def test_float_centre_rounding_is_not_optional(self):
        points = np.array([[16777216, 0, 0], [16777218, 0, 0]], dtype=np.float32)
        wrong = np.array([16777217, 0, 0, 1], dtype=np.float32)
        self.assertEqual(containment(points, wrong)["outside"], 1)
        fixed = pack_sphere([16777217, 0, 0], 1)
        self.assertEqual(containment(points, fixed)["outside"], 0)

    def test_shear_reflection_singular_and_changed_instances(self):
        rng = np.random.default_rng(819)
        points = rng.uniform(-1, 1, (1000, 3)).astype(np.float32)
        local = pack_sphere([0, 0, 0], np.sqrt(3))
        rows = []
        for i in range(32):
            row = np.eye(4)
            row[:3, :3] = rng.normal(size=(3, 3)) * 10 ** rng.uniform(-3, 3)
            row[:3, 3] = rng.uniform(-6.4e6, 6.4e6, 3)
            if i == 0:
                row[:3, :3] = 0
            sphere = transformed_sphere(local, row)
            self.assertIsNotNone(sphere)
            self.assertEqual(
                containment(shader_points(points, row)[:, :3], sphere)["outside"], 0
            )
            rows.append(row)
        self.assertEqual(
            containment(
                np.vstack([shader_points(points, r)[:, :3] for r in rows]),
                world_sphere(local, rows),
            )["outside"],
            0,
        )
        moved = np.eye(4)
        moved[0, 3] = 100
        self.assertTrue(outside(world_sphere(local, [moved]), np.eye(4), [0, 0, 0]))
        self.assertFalse(
            outside(world_sphere(local, [np.eye(4)]), np.eye(4), [0, 0, 0])
        )
        moved[3, 0] = 1
        self.assertIsNone(world_sphere(local, [moved]))
        self.assertIsNone(world_sphere(local, []))

    def test_shadow_has_no_camera_or_z_rejection_and_keeps_tangency(self):
        for z in (-1e10, 1e10):
            self.assertFalse(outside(np.array([0, 0, z, 0.5]), np.eye(4), [0, 0, 0]))
        self.assertFalse(outside(np.array([2, 0, 0, 1]), np.eye(4), [0, 0, 0]))
        self.assertTrue(outside(np.array([2.01, 0, 0, 1]), np.eye(4), [0, 0, 0]))
        self.assertFalse(outside(np.array([100, 0, 0, 1]), np.eye(4), [-100, 0, 0]))

    def test_producer_radius_is_a_required_contract_not_a_datatype_guarantee(self):
        declared = cluster_sphere([[0, 0, 0, 0.25]], [(0, 3)], 3)
        self.assertIsNotNone(declared)
        self.assertEqual(containment([[1, 0, 0]], declared)["outside"], 1)

    def test_overflow_and_later_uncertain_plane_keep_the_batch(self):
        self.assertIsNone(pack_sphere([0, 0, 0], np.finfo(np.float32).max))
        row = np.eye(4)
        row[0, 0] = 1e38
        self.assertIsNone(transformed_sphere(np.array([0, 0, 0, 100]), row))
        light = np.eye(4)
        light[1, 1] = np.float32(1e38)
        self.assertFalse(outside(np.array([100, 100, 0, 1]), light, [0, 0, 0]))

    def test_float_shader_clip_retains_every_observed_visible_point(self):
        rng = np.random.default_rng(820)
        points = rng.uniform(-1, 1, (64, 3)).astype(np.float32)
        local = pack_sphere([0, 0, 0], np.sqrt(3))
        visible_cases, rejected_cases = 0, 0
        for trial in range(512):
            row = np.eye(4)
            row[:3, :3] = rng.normal(size=(3, 3))
            row[:3, 3] = rng.uniform(-1, 1, 3) * (6.4e6 if trial % 2 else 10)
            shift = -row[:3, 3].astype(np.float32)
            if trial % 4 == 0:
                shift[0] += 100
            light = np.eye(4, dtype=np.float32)
            light[:3, :3] = rng.normal(size=(3, 3))
            if trial % 3 == 0:
                light[3] = [0, 0, -1, 0]
            world = shader_points(points, row, shift)
            clip = shader_points(world[:, :3], light)
            visible = (abs(clip[:, :2]) <= clip[:, 3:]).all(1)
            rejected = outside(transformed_sphere(local, row), light, shift)
            visible_cases += int(visible.any())
            rejected_cases += int(rejected)
            self.assertFalse(rejected and visible.any(), trial)
        self.assertGreater(visible_cases, 100)
        self.assertGreater(rejected_cases, 100)


if __name__ == "__main__":
    unittest.main()
