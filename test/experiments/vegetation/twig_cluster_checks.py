"""Experiment wood batching consumes the same installable short-shoot core."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from outshine_ref.vegetation import TwigClusters
from outshine_ref.vegetation.axis_surface import axis_mesh
from twig_cluster_mesh import add_twigs


def plan(counts=(25, 26, 0, 1)):
    return TwigClusters(
        np.array((7, 13, 22, 39), np.uint64),
        np.array(counts),
        np.arange(sum(counts), dtype=np.uint8) % 8,
        np.array(((0, 0, 1), (1, 0, 1), (0, 1, 1), (1, 1, 1)), float),
        np.tile(np.eye(3), (4, 1, 1)),
        "a" * 64,
    )


class TwigClusterChecks(unittest.TestCase):
    def test_extra_wood_preflights_and_preserves_source_exactly(self):
        p = plan()
        source = axis_mesh(1, 0.2, 0.01, 0.005, 3, 4)
        source["surface"] = np.zeros(len(source["faces"]), np.uint8)
        source["corner_uv"] = np.zeros((source["faces"].size, 2))
        with patch(
            "twig_cluster_mesh.axis_mesh", side_effect=AssertionError("premature wood")
        ):
            self.assertIsNone(add_twigs(p, source, 10))
        result = add_twigs(p, source, 11)
        self.assertFalse(p._prototypes or p._groups)
        for key in ("vertices", "faces", "normals", "corner_uv", "surface"):
            np.testing.assert_array_equal(result[key][: len(source[key])], source[key])
        self.assertEqual(len(result["faces"]) - len(source["faces"]), 11 * 32)

    def test_batched_wood_matches_scalar_at_actual_radii(self):
        p = plan()
        source = axis_mesh(1, 0.2, 0.01, 0.005, 3, 4)
        source["surface"] = np.zeros(len(source["faces"]), np.uint8)
        source["corner_uv"] = np.zeros((source["faces"].size, 2))
        a, b = add_twigs(p, source, 11), add_twigs(p, source, 11, method="scalar")
        for key in ("faces", "surface", "corner_uv"):
            np.testing.assert_array_equal(a[key], b[key])
        np.testing.assert_allclose(a["vertices"], b["vertices"], atol=1e-12, rtol=0)
        np.testing.assert_allclose(a["normals"], b["normals"], atol=1e-10, rtol=0)
        wider = replace(p, carrier_radii=(0.003, 0.001))
        twig = wider.twigs(7, 32)
        np.testing.assert_allclose(
            twig["radii"], 0.65 * (0.003 - 0.002 * twig["parameters"]), atol=1e-18
        )
        self.assertNotEqual(wider.identity, p.identity)
        with self.assertRaises(ValueError):
            replace(p, carrier_radii=(0.02, 0.01))


if __name__ == "__main__":
    unittest.main()
