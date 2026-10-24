"""Transport partitions, exact-node relighting and explicit unknown fallback."""

import unittest

import numpy as np
from test_urban_parent_phase import homogeneous_parent
from urban_parent_phase import render_phase
from urban_parent_transfer import ShadowTransfer, collect_transfer


class ParentTransferTest(unittest.TestCase):
    def test_transfer_is_unlit_partition_and_preserves_vertical_height_cdf(self):
        parent = homogeneous_parent()
        origins = np.array([[253.0, 255.0, 100.0]])
        directions = np.array([[0.0, 0.0, -1.0]])
        profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        transfer, _ = collect_transfer(parent, origins, directions, profile, side=4)
        np.testing.assert_allclose(transfer[..., 0].sum(axis=1), 1, atol=2e-15)
        np.testing.assert_allclose(transfer[..., 1], transfer[..., 0], atol=2e-15)
        np.testing.assert_allclose(
            transfer[0, np.arange(4) * 5, 0],
            0.85 * 0.75**2 * parent.probabilities,
            atol=2e-15,
        )
        np.testing.assert_allclose(transfer[0, -1, 0], 1 - 0.85 * 0.75**2, atol=2e-15)
        changed, _ = collect_transfer(
            parent,
            origins,
            directions,
            profile | {"intensity": 123.0, "ambient": 456.0},
            side=4,
        )
        np.testing.assert_array_equal(transfer, changed)

    def test_node_transfer_preserves_phase_integral_for_directional_ggx(self):
        parent = homogeneous_parent()
        origins = np.array([[235.0, 231.0, 100.0], [275.0, 271.0, 100.0]])
        directions = np.tile([0.1, 0.2, -1.0], (2, 1))
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        profile = {
            "kind": "sun",
            "direction": [0.4, -0.2, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        moments, _ = collect_transfer(parent, origins, directions, profile, side=4)
        streamed, _ = collect_transfer(
            parent, origins, directions, profile, side=4, batch=1
        )
        np.testing.assert_array_equal(moments, streamed)
        np.testing.assert_allclose(moments[..., 0].sum(axis=1), 1, atol=2e-15)
        self.assertTrue(np.all(moments[..., 1] <= moments[..., 0]))
        lookup = ShadowTransfer(moments.reshape(1, 2, -1, 2), (2, 1), 4)
        exact, alpha, _ = render_phase(parent, origins, directions, profile)
        approx, coverage, work = render_phase(
            parent, origins, directions, profile, shadow_transfer=lookup
        )
        np.testing.assert_allclose(approx, exact, rtol=0, atol=2e-14)
        np.testing.assert_array_equal(coverage, alpha)
        self.assertGreater(work["shadow_lookup_samples"], 0)
        self.assertEqual(work["shadow_fallback_rays"], 0)
        brighter, _, _ = render_phase(
            parent,
            origins,
            directions,
            profile | {"intensity": 6, "ambient": 0.2},
            shadow_transfer=lookup,
        )
        np.testing.assert_allclose(brighter, 2 * approx, rtol=0, atol=2e-14)

    def test_unknown_transfer_falls_back_to_exact_shadow_queries(self):
        parent = homogeneous_parent()
        origins = np.array([[235.0, 231.0, 100.0]])
        directions = np.array([[0.0, 0.0, -1.0]])
        profile = {
            "kind": "sun",
            "direction": [0.4, -0.2, 1],
            "intensity": 3,
            "ambient": 0.1,
        }
        moments = np.zeros((1, 1, len(parent.heights) * 5 + 1, 2))
        exact, alpha, _ = render_phase(parent, origins, directions, profile)
        approx, coverage, work = render_phase(
            parent,
            origins,
            directions,
            profile,
            shadow_transfer=ShadowTransfer(moments, (1, 1), 4),
        )
        np.testing.assert_array_equal(approx, exact)
        np.testing.assert_array_equal(coverage, alpha)
        self.assertGreater(work["shadow_fallback_rays"], 0)


if __name__ == "__main__":
    unittest.main()
