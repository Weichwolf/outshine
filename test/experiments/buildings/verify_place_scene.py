"""Actual generated geometry placement, procedural material binding and DEM winding controls."""

import unittest

import numpy as np
import shapely
from place_materials import Palette
from place_reference import read
from place_terrain import mesh as terrain
from tile_buildings import construct


class SceneControls(unittest.TestCase):
    def test_dem_placement_translates_actual_mesh_without_changing_source_heights(self):
        source = {
            "id": "control",
            "layer": "building",
            "polygon": shapely.box(0, 0, 10, 12),
            "properties": {"render_height": 9.0, "render_min_height": 0.0},
            "coordinate_step_m": 0.1,
            "clipped": False,
        }
        receipt = {"span_m": 100.0, "invalid": []}
        baseline, before = construct("control", prepared=([source], receipt))
        shifted, after = construct(
            "control",
            prepared=(
                [source | {"placement_base_enu_m": 102.75, "ground_asl_m": 102.75}],
                receipt,
            ),
        )
        np.testing.assert_allclose(
            shifted["vertices"] - baseline["vertices"],
            np.tile([0, 0, 102.75], (len(baseline["vertices"]), 1)),
        )
        for key in ("faces", "normals", "uv", "materials"):
            np.testing.assert_array_equal(shifted[key], baseline[key])
        self.assertEqual(
            after["members"][0]["source_properties"],
            before["members"][0]["source_properties"],
        )
        self.assertEqual(after["members"][0]["source_height_interval"], (0.0, 9.0))
        self.assertEqual(after["members"][0]["placement_base_enu_m"], 102.75)

    def test_tile_local_material_ids_remain_distinct_and_rebind_facade_parent(self):
        palette = Palette()
        sample = {"materials": np.array([12, 32768, 9], dtype=np.uint16)}
        a = palette.bind(
            sample, {"12": {"color.r": 0.1}}, {"32768": {"wall_material": 12}}
        )
        b = palette.bind(
            sample, {"12": {"color.r": 0.9}}, {"32768": {"wall_material": 12}}
        )
        self.assertNotEqual(a["materials"][0], b["materials"][0])
        self.assertNotEqual(a["materials"][1], b["materials"][1])
        self.assertEqual(a["materials"][2], 9)
        for result in (a, b):
            self.assertEqual(
                palette.facades[str(result["materials"][1])]["wall_material"],
                result["materials"][0],
            )
        again = palette.bind(
            sample, {"12": {"color.r": 0.1}}, {"32768": {"wall_material": 12}}
        )
        np.testing.assert_array_equal(again["materials"], a["materials"])

    def test_dem_context_has_upward_actual_triangles_in_native_frame(self):
        class Constant:
            def sample(self, latitude, _longitude):
                return np.full_like(latitude, 100.0)

        result = terrain(
            read("Flensburg"),
            [{"kind": "vector", "tile": [14, 8621, 5199]}],
            Constant(),
            12,
        )
        self.assertEqual(len(result["faces"]), 32768)
        self.assertGreater(result["normals"][:, 2].min(), 0.999)
        self.assertTrue(np.isfinite(result["vertices"]).all())


if __name__ == "__main__":
    unittest.main()
