"""Independent opaque shadow fixtures and existing thin-sheet front/back response."""

import unittest
from unittest.mock import patch

import numpy as np
from grass_light_model import require_light_support
from grass_shadow import GrassShadowIndex, query_origins
from grass_shadow_reference import direct_visibility
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response


def quad(x0, x1, z):
    return np.array(((x0, -1, z), (x1, -1, z), (x1, 1, z), (x0, 1, z))), np.array(
        ((0, 1, 2), (0, 2, 3))
    )


class GrassLightChecks(unittest.TestCase):
    def test_layered_visibility_and_packet_partition(self):
        v, f = quad(0, 1, 0.4)
        points = np.c_[[-0.3, -0.1, 0.1, 0.3], np.full(4, 0.13), np.zeros(4)]
        normals = np.tile((0, 0, 1.0), (4, 1))
        index = GrassShadowIndex(v, f)
        for batch in (1, 3, 8192):
            visible, _ = index.visibility(points, normals, (0, 0, 1), batch=batch)
            np.testing.assert_array_equal(visible, (True, True, False, False))
        origins = query_origins(points, normals, np.array((0, 0, 1.0)), 2e-5)
        exact, _ = direct_visibility(v, f, origins, np.array((0, 0, 1.0)), np.ones(4))
        np.testing.assert_array_equal(exact, visible)
        # Same global view/light marginals, opposite conditional illumination.
        self.assertEqual(visible[:2].mean(), 1)
        self.assertEqual(visible[2:].mean(), 0)
        self.assertEqual(visible.mean(), 0.5)

    def test_local_surface_is_not_its_own_occluder(self):
        v, f = quad(-1, 1, 0.4)
        p = np.array(((0.13, 0.27, 0.4),))
        n = np.array(((0, 0, 1.0),))
        index = GrassShadowIndex(v, f)
        for d in ((0, 0, 1), (np.sqrt(3) / 2, 0, 0.5)):
            for bias in (1e-5, 2e-5, 4e-5):
                visible, _ = index.visibility(p, n, d, bias)
                self.assertTrue(visible.all())

    def test_front_back_and_zero_transmission_are_existing_reference(self):
        optical = np.array(((0.1, 0.2, 0.05, 0.04, 0.08, 0.02),))
        n, view = np.array(((0, 0, 1.0),)), np.array((0, 0, 1.0))
        np.testing.assert_allclose(
            leaf_diffuse_response(optical, n, view, view), optical[:, :3] / np.pi
        )
        np.testing.assert_allclose(
            leaf_diffuse_response(optical, n, view, -view), optical[:, 3:] / np.pi
        )
        np.testing.assert_allclose(
            leaf_diffuse_response(optical, -n, view, -view), optical[:, 3:] / np.pi
        )

    def test_empty_and_unsupported_guard(self):
        index = GrassShadowIndex(np.empty((0, 3)), np.empty((0, 3), int))
        visible, work = index.visibility(
            np.array(((0, 0, 0.0),)), np.array(((0, 0, 1.0),)), (0, 0, 1)
        )
        self.assertTrue(visible.all())
        self.assertEqual(work["packets"], 0)
        recipe = {
            "root_bounds_m": [[-0.5, -0.5], [0.5, 0.5]],
            "maximum_radius_m": 0.1,
            "maximum_height_m": 0.5,
        }
        with self.assertRaises(ValueError):
            require_light_support(
                recipe,
                (-0.5, -0.5),
                (0.5, 0.5),
                np.array((0, 0, 1.0)),
                [np.array((np.sqrt(3) / 2, 0, 0.5))],
                2e-5,
            )
        with patch.object(index, "scene"), self.assertRaises(ValueError):
            index.visibility(
                np.array(((0, 0, 0.0),)), np.array(((0, 0, 0.0),)), (0, 0, 1)
            )


if __name__ == "__main__":
    unittest.main()
