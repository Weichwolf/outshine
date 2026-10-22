"""Analytic limits and relighting of the experimental statistical consumer."""

import subprocess
import sys
import unittest

import numpy as np
from test_urban_parent_stats import example
from urban_parent_light import shade
from urban_parent_volume import render_volume


def daylight(direction):
    return {"kind": "sun", "direction": direction, "intensity": 1, "ambient": 0}


class ParentVolumeTest(unittest.TestCase):
    def test_vertical_visibility_preserves_filtered_roof_area_in_both_models(self):
        parent = example()
        extent = parent.frame[2] * parent.frame[3]
        origins = np.array([[extent / 2, extent / 2, 100]])
        directions = np.array([[0, 0, -1]], float)
        expected = parent.surface_measures()[0, 0, 4]
        for phase in [False, True]:
            colour, transmission, _ = render_volume(
                parent, origins, directions, daylight([0, 0, 1]), phase_mixture=phase
            )
            np.testing.assert_allclose(1 - transmission, expected, rtol=0, atol=1e-15)
            self.assertTrue(np.isfinite(colour).all())

    def test_empty_statistics_transmit_and_light_changes_do_not_rebuild_the_asset(self):
        parent = example()
        origin = np.array([[256, 256, 100]], float)
        direction = np.array([[0.1, 0.2, -1]], float)
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        before = {k: v.tobytes() for k, v in parent.arrays().items()}
        a = render_volume(parent, origin, direction, daylight([0.3, 0.2, 1]))
        b = render_volume(parent, origin, direction, daylight([-0.3, -0.2, 1]))
        self.assertGreater(float(np.abs(a[0] - b[0]).max()), 0)
        for k, v in parent.arrays().items():
            self.assertEqual(v.tobytes(), before[k])
        parent.centre_fractions[:] = 0
        colour, transmission, _ = render_volume(
            parent, origin, direction, daylight([0, 0, 1])
        )
        np.testing.assert_array_equal(colour, np.zeros((1, 3)))
        np.testing.assert_array_equal(transmission, [1])

    def test_shared_ggx_direct_brdf_is_reciprocal(self):
        a, b = np.array([[0.2, 0.3, 1.0]]), np.array([[-0.4, 0.1, 1.0]])
        a /= np.linalg.norm(a, axis=1)[:, None]
        b /= np.linalg.norm(b, axis=1)[:, None]
        point, normal = np.zeros((1, 3)), np.array([[0, 0, 1]])
        first = (
            shade(point, normal, b, 0.6, np.ones(1), daylight(a[0].tolist())) / a[0, 2]
        )
        second = (
            shade(point, normal, a, 0.6, np.ones(1), daylight(b[0].tolist())) / b[0, 2]
        )
        np.testing.assert_allclose(first, second, rtol=0, atol=1e-14)

    def test_runtime_volume_import_does_not_load_geometry_or_source(self):
        code = """
import builtins
original = builtins.__import__
def checked(name, *args, **kwargs):
    if name.split('.')[0] in {'shapely', 'trimesh', 'coarse_city_domains', 'urban_patch_bake'}:
        raise AssertionError(name)
    return original(name, *args, **kwargs)
builtins.__import__ = checked
import urban_parent_volume
import urban_parent_conditional
import urban_parent_phase
import urban_parent_transfer
"""
        subprocess.run([sys.executable, "-B", "-W", "error", "-c", code], check=True)


if __name__ == "__main__":
    unittest.main()
