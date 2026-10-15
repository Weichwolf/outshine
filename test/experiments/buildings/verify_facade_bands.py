"""Actual source controls: opening-safe bands, unchanged source mesh and neighbor clearance."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from tile_facade_bands import BandDetails, specifications
from verify_recess_consumer import camera, source_facades
from verify_urban_roofs import sources


class FacadeBands(unittest.TestCase):
    def test_actual_sources_keep_all_original_mesh_and_window_free_heights(self):
        for row in sources():
            original = copy.deepcopy(row)
            mesh, facades, parameters = source_facades(row)
            frame = max(facades, key=lambda f: f["width_m"])
            details = BandDetails([row])
            result = details(
                mesh, row, parameters, facades, camera(frame, 25), 1, "residential"
            )
            self.assertTrue(details.completed, details.rejected)
            self.assertEqual(row, original)
            self.assertLess(len(mesh["faces"]), len(result["faces"]))
            for key in mesh:
                np.testing.assert_array_equal(mesh[key], result[key][: len(mesh[key])])
            for record in details.completed:
                z0, z1 = record["base_m"], record["base_m"] + record["profile"][-1][1]
                self.assertGreaterEqual(z0, parameters["minHeight"])
                for f in facades:
                    floor = f["height_m"] / f["rows"]
                    self.assertLessEqual(z1, parameters["minHeight"] + f["height_m"])
                    for j in range(f["rows"]):
                        low = parameters["minHeight"] + floor * (
                            j + 0.5 - f["fractions"][1] / 2
                        )
                        high = parameters["minHeight"] + floor * (
                            j + 0.5 + f["fractions"][1] / 2
                        )
                        self.assertTrue(z1 <= low or z0 >= high)

    def test_far_needs_no_geometry_buffers_or_profile_expansion(self):
        row = sources()[1]
        _, facades, p = source_facades(row)
        details = BandDetails([row])

        class Unreadable(dict):
            def __getitem__(self, _key):
                raise AssertionError("Far geometry read")

        mesh = Unreadable()
        with patch(
            "tile_facade_bands.edge_footprints", side_effect=AssertionError("expanded")
        ):
            result = details(
                mesh, row, p, facades, camera(facades[0], 100000), 1, "residential"
            )
        self.assertIs(result, mesh)
        self.assertFalse(details.completed)

    def test_neighbor_blocks_only_intersecting_edges_at_actual_height(self):
        row = sources()[1]
        mesh, facades, p = source_facades(row)
        frame = max(facades, key=lambda f: f["width_m"])
        a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
        t = (b - a) / frame["width_m"]
        n = np.array([t[1], -t[0]])
        polygon = shapely.Polygon([a, b, b + n * 2, a + n * 2])
        neighbor = {
            "id": "neighbor",
            "polygon": polygon,
            "properties": {"render_height": 100.0},
        }
        details = BandDetails([row, neighbor])
        result = details(mesh, row, p, facades, camera(frame, 25), 1, "residential")
        self.assertTrue(details.completed or details.rejected)
        index = facades.index(frame)
        self.assertFalse(any(index in r["edges"] for r in details.completed))
        from outshine_ref.buildings.profiled_bands import BandPlan, edge_footprints

        for r in details.completed:
            supports = edge_footprints(
                BandPlan(row["polygon"], r["profile"], r["base_m"])
            )
            for i in r["edges"]:
                self.assertLess(supports[i].intersection(polygon).area, 1e-9)
        overhead = neighbor | {
            "properties": {"render_min_height": 100.0, "render_height": 110.0}
        }
        above = BandDetails([row, overhead])
        above(mesh, row, p, facades, camera(frame, 25), 1, "residential")
        self.assertTrue(any(index in r["edges"] for r in above.completed))
        self.assertGreaterEqual(len(result["faces"]), len(mesh["faces"]))

    def test_failed_late_expansion_does_not_report_nonexistent_geometry(self):
        row = sources()[1]
        mesh, facades, p = source_facades(row)
        from outshine_ref.buildings.profiled_bands import band_surface

        count = 0

        def fail(plan, *args, **kwargs):
            nonlocal count
            count += 1
            if count == 2:
                raise ValueError("controlled late failure")
            return band_surface(plan, *args, **kwargs)

        details = BandDetails([row])
        with patch("tile_facade_bands.band_surface", side_effect=fail):
            result = details(
                mesh, row, p, facades, camera(facades[0], 25), 1, "residential"
            )
        self.assertIs(result, mesh)
        self.assertFalse(details.completed)
        self.assertTrue(details.rejected)

    def test_special_clipped_and_no_safe_gap_keep_original(self):
        row = sources()[1]
        mesh, facades, p = source_facades(row)
        details = BandDetails([row])
        for family in ("religious", "industrial", "utility", "unknown"):
            self.assertIs(
                details(mesh, row, p, facades, camera(facades[0], 25), 1, family), mesh
            )
        self.assertIs(
            details(
                mesh,
                row | {"clipped": True},
                p,
                facades,
                camera(facades[0], 25),
                1,
                "residential",
            ),
            mesh,
        )
        self.assertFalse(
            specifications([f | {"fractions": [0.48, 1.0]} for f in facades], 0)
        )


if __name__ == "__main__":
    unittest.main()
