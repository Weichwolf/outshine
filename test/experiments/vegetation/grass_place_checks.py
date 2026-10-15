"""Landcover/form separation, physical tuft geometry and stable child ancestry."""

import unittest
from dataclasses import replace
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
from grass_tuft import tuft
from place_scene_checks import camera, feature
from place_stand import sites


class HighGround:
    def sample(self, xy):
        return SimpleNamespace(
            points_enu_m=np.column_stack((xy, np.full(len(xy), 1300.0))),
            reasons=("",) * len(xy),
        )


class GrassPlaceChecks(unittest.TestCase):
    def test_high_grass_is_not_a_conifer_and_keeps_physical_source_ids(self):
        ring = [[0, 0], [4096, 0], [4096, 4096], [0, 4096], [0, 0]]
        wood = [[0, 0], [2048, 0], [2048, 4096], [0, 4096], [0, 0]]
        data = {
            "landcover": {
                "extent": 4096,
                "features": [
                    feature({"type": "Polygon", "coordinates": [wood]}, "wood"),
                    feature({"type": "Polygon", "coordinates": [ring]}, "grass"),
                ],
            }
        }
        receipt = {"kind": "vector", "tile": [14, 8192, 5745], "sha256": "source"}
        cam = replace(camera(), latitude_deg=47, origin_latitude_deg=47)
        with patch("place_stand.decode_vector", return_value=data):
            a, report = sites(cam, [receipt], HighGround())
            b, _ = sites(replace(cam, bearing_deg=180), [receipt], HighGround())
        for k in a:
            np.testing.assert_array_equal(a[k], b[k])
        grass = a["source"] == 1
        self.assertTrue(grass.any() and (~grass).any())
        self.assertTrue((a["kind"][grass] == 2).all())
        self.assertTrue((a["kind"][~grass] == 1).all())
        self.assertEqual(report["plan_format"], "source-site-plan-2")
        self.assertEqual(len(np.unique(a["ids"], axis=0)), len(a["ids"]))
        self.assertEqual(report["sources"][1]["properties"]["class"], "grass")

    def test_seeded_tuft_has_positive_faces_unit_normals_and_submetre_bounds(self):
        a, b = tuft(17), tuft(17)
        for k in a:
            np.testing.assert_array_equal(a[k], b[k])
        p = a["vertices"][a["faces"]]
        cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        self.assertTrue((np.linalg.norm(cross, axis=1) > 0).all())
        n = a["normals"].reshape(-1, 3, 3)
        self.assertTrue((np.sum(n * cross[:, None, :], axis=2) > 0).all())
        np.testing.assert_allclose(np.linalg.norm(n, axis=2), 1, atol=1e-12)
        self.assertTrue(np.max(a["vertices"][:, 2]) < 0.5)
        self.assertTrue(np.max(a["vertices"][:, 2]) > 0.2)
        self.assertGreater(a["vertices"][:, 2].min(), -0.02)
        self.assertFalse(np.array_equal(tuft(18)["vertices"], a["vertices"]))

    def test_shared_blade_ids_and_surface_points_survive_subdivision(self):
        coarse, fine = tuft(17, segments=4), tuft(17, segments=8)
        lookup = {
            (int(i), *uv): p
            for i, uv, p in zip(
                fine["blade_index"], fine["uv"], fine["vertices"], strict=True
            )
        }
        for i, uv, p in zip(
            coarse["blade_index"], coarse["uv"], coarse["vertices"], strict=True
        ):
            np.testing.assert_allclose(p, lookup[(int(i), *uv)], atol=1e-15, rtol=0)
        with self.assertRaises(ValueError):
            tuft(-1)
        with self.assertRaises(ValueError):
            tuft(1, blades=1000000)


if __name__ == "__main__":
    unittest.main()
