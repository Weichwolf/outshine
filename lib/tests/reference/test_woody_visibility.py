import unittest
from types import SimpleNamespace

import numpy as np
from outshine.rendering.water_reflection import ReflectionPlane
from outshine.rendering.woody_visibility import (
    reference_visibility_test,
    visible_woody_placements,
)


class WoodyVisibilityTest(unittest.TestCase):
    def setUp(self):
        self.world = SimpleNamespace(
            look=np.array([0.0, 1.0, 0.0]),
            eye=np.array([0, 0, 2]),
            focal=640,
            sun=np.array([0, -1, 1]) / np.sqrt(2),
        )

    def test_offscreen_crown_keeps_detail_if_its_shadow_can_enter_view(self):
        retain = reference_visibility_test(self.world, (), 1280, 720, 0.0093)
        centre, extent = np.zeros(3), np.full(3, 0.25)
        self.assertTrue(retain(centre, extent, np.array([0, 3, 2]), 0, 1))
        self.assertTrue(retain(centre, extent, np.array([3, 2, 3]), 0, 1))
        self.assertFalse(retain(centre, extent, np.array([10, 1, 1]), 0, 1))

    def test_reflection_keeps_a_crown_outside_the_primary_view(self):
        plane = ReflectionPlane.through(self.world.eye[:2], 0, (0, 0))
        primary = reference_visibility_test(self.world, (), 1280, 720, 0.0093)
        with_reflection = reference_visibility_test(
            self.world, (plane,), 1280, 720, 0.0093
        )
        crown = np.zeros(3), np.full(3, 0.25), np.array([0, 2, -2]), 0, 1
        self.assertFalse(primary(*crown))
        self.assertTrue(with_reflection(*crown))

    def test_placement_filter_uses_mesh_bounds_and_keeps_original_transforms(self):
        retain = reference_visibility_test(self.world, (), 1280, 720, 0.0093)
        meshes = {"foliage": ([[-0.25] * 3, [0.25] * 3], [])}
        placements = np.array([[3, 2, 3, 0, 1], [10, 1, 1, 0, 1], [0, 3, 2, 1, 2]])
        visible = visible_woody_placements(meshes, placements, retain)
        np.testing.assert_array_equal(visible, placements[[0, 2]])
