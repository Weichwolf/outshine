"""Analytic planar image, dynamic-light and transparent-filter controls."""

import unittest

import numpy as np
from source_billboard import bake_card, bilinear, card_samples, join_sources
from source_far_reprojection import prepare_source
from source_surface_fields import relight, surface_fields


def flat_source():
    return {
        "positions": np.array(
            [[-1.0, -1.0, 0.0], [1.0, -1.0, 0.0], [1.0, 1.0, 0.0], [-1.0, 1.0, 0.0]],
            np.float32,
        ),
        "indices": np.array([[0, 1, 2], [0, 2, 3]], np.uint32),
        "normal": np.full(2, 0x80008000, np.uint32),
        "code": np.zeros(2, np.int16),
        "palette": np.array([[0.6, 0.4, 0.2, 1]], np.float32),
        "colour_index": np.zeros(2, np.uint16),
        "native_bytes": 120,
        "metadata": {"original_parent": "synthetic", "original_source_ordinal": 0},
    }


class BillboardTests(unittest.TestCase):
    def setUp(self):
        self.source = join_sources([flat_source()])
        self.view = (np.array([0.0, 0.0, 4.0]), np.diag([1.0, 1.0, -1.0]), 32.0, 64, 64)
        self.sun = [0.0, 0.0, 1.0]
        self.card = bake_card(
            self.source,
            prepare_source(self.source),
            self.view,
            self.sun,
            texture_size=32,
        )

    def test_actual_card_is_two_triangles_and_central_plane_matches(self):
        self.assertEqual(self.card["mesh"]["indices"].shape, (2, 3))
        origin = np.array([[0.0, 0.0, 4.0]])
        direction = np.array([[0.0, 0.0, -1.0]])
        depth, alpha, matte, rgb = card_samples(self.card, origin, direction, self.sun)
        self.assertAlmostEqual(depth[0], 4)
        self.assertAlmostEqual(alpha[0], 1)
        expected = relight(surface_fields(self.source, np.array([0])), self.sun, depth)
        np.testing.assert_allclose(rgb, expected, atol=1 / 255)
        np.testing.assert_allclose(matte, expected, atol=1 / 255)

    def test_live_light_changes_and_static_matte_stays_baked(self):
        origin, direction = np.array([[0.0, 0.0, 4.0]]), np.array([[0.0, 0.0, -1.0]])
        a = card_samples(self.card, origin, direction, self.sun)
        b = card_samples(self.card, origin, direction, [0.0, 0.0, -1.0])
        np.testing.assert_array_equal(a[2], b[2])
        self.assertGreater(np.linalg.norm(a[3] - b[3]), 0.4)

    def test_premultiplied_transparency_has_no_black_colour_fringe(self):
        image = np.array([[[1.0, 0.5, 0.25, 1.0], [0.0, 0.0, 0.0, 0.0]]])
        sample = bilinear(image, np.array([[0.5, 0.5]]))[0]
        np.testing.assert_allclose(sample, [0.5, 0.25, 0.125, 0.5])
        np.testing.assert_allclose(sample[:3] / sample[3], [1, 0.5, 0.25])


if __name__ == "__main__":
    unittest.main()
