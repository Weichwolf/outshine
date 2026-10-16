"""Actual retained near products, metric glass fields and bounded presentation choice."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from opening_details import expand, slots
from stepped_facade_details import DETAIL_RADIUS_M, demand, present
from stepped_profile_probe import sources
from stepped_source import near, plan_and_near
from stepped_source_probe import overview
from tile_stepped import SteppedDetails


class SteppedPresentation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.row = sources()[0]
        cls.plan, cls.payload = plan_and_near(cls.row, "corner")
        points = cls.payload[0]["vertices"]
        cls.view = overview(points, [1.0, -1.25, 0.8])

    def test_capture_matches_the_fresh_full_near_assembly_and_access(self):
        fresh = near(self.plan, 1)
        for key in self.payload[0]:
            self.assertEqual(self.payload[0][key].dtype, fresh[0][key].dtype)
            np.testing.assert_array_equal(self.payload[0][key], fresh[0][key])
        self.assertEqual(self.payload[1:], fresh[1:])

    def test_presentation_shares_geometry_and_preserves_the_physical_source(self):
        physical = self.payload[0]
        previous_uv, previous_materials = (
            physical["uv"].copy(),
            physical["materials"].copy(),
        )
        mesh = present(physical, 12, 32768)
        for key in ("vertices", "faces", "normals"):
            self.assertIs(mesh[key], physical[key])
        np.testing.assert_array_equal(physical["uv"], previous_uv)
        np.testing.assert_array_equal(physical["materials"], previous_materials)
        expected = previous_materials.copy()
        expected[previous_materials == 1] = 12
        expected[previous_materials == 3] = 32768
        np.testing.assert_array_equal(mesh["materials"], expected)
        from blender_city_mesh import cell_sizes

        cells = cell_sizes(mesh, [32768])
        np.testing.assert_allclose(
            cells[mesh["materials"] == 32768, :2], 1.0, atol=1e-6
        )

    def test_component_radius_covers_actual_physical_frames_and_terrace_sill_omissions(
        self,
    ):
        openings = slots(self.plan)
        framing = expand(openings)
        physical = self.payload[0]
        actual = physical["vertices"][
            physical["faces"][np.isin(physical["materials"], (10, 11))]
        ]
        np.testing.assert_array_equal(actual, framing["vertices"][framing["faces"]])
        self.assertTrue(any(o["kind"] == "window" and not o["sill"] for o in openings))
        for index, opening in enumerate(openings):
            points = framing["vertices"][
                framing["faces"][framing["opening_indices"] == index]
            ].reshape(-1, 3)
            delta = points - np.r_[opening["centre"], opening["bottom_z_m"]]
            tangent = np.asarray(opening["tangent"])
            u = delta[:, :2] @ tangent
            normal = delta[:, :2] @ np.array([tangent[1], -tangent[0]])
            x = np.maximum(abs(u) - opening["width_m"] / 2, 0)
            z = np.maximum.reduce(
                [-delta[:, 2], delta[:, 2] - opening["height_m"], np.zeros(len(delta))]
            )
            self.assertLessEqual(
                np.sqrt(x * x + z * z + normal * normal).max(), DETAIL_RADIUS_M
            )

    def test_near_far_lookaway_and_inside_choice_precedes_projection(self):
        self.assertTrue(demand(self.plan, self.view)[0])
        far = replace(
            self.view, eye=self.view.target + (self.view.eye - self.view.target) * 100
        )
        self.assertFalse(demand(self.plan, far)[0])
        away = replace(
            self.view, target=self.view.eye + (self.view.eye - self.view.target)
        )
        self.assertFalse(demand(self.plan, away)[0])
        room = self.plan["storey_plans"][0]["rooms"][0]
        xy = np.asarray(room.representative_point().coords[0])
        eye = np.r_[xy, self.plan["floor_z_m"] + 1.5]
        inside = replace(self.view, eye=eye, target=eye + [0, 1, 0])
        needed, proof = demand(self.plan, inside)
        self.assertTrue(needed)
        self.assertTrue(proof["inside_source"])
        with (
            patch(
                "stepped_facade_details.coordinates",
                side_effect=AssertionError("expanded"),
            ),
            self.assertRaisesRegex(ValueError, "capacity"),
        ):
            present(self.payload[0], 1, 32768, max_triangles=1)

    def test_actual_tile_consumer_keeps_the_stepped_body_on_local_detail_failure(self):
        builder = SteppedDetails([], self.view)
        builder.selected.add(self.row["id"])
        with (
            patch("tile_stepped.plan_and_near", return_value=(self.plan, self.payload)),
            patch(
                "tile_stepped.present", side_effect=ValueError("injected detail limit")
            ),
        ):
            mesh, metadata = builder(self.row, {}, 12)
        self.assertTrue(len(mesh["faces"]))
        self.assertEqual(builder.rejected, [])
        self.assertEqual(len(builder.completed), 1)
        self.assertEqual(
            builder.completed[0]["facade_presentation"]["mode"], "compact-fallback"
        )
        self.assertEqual(metadata["room_ids"], self.plan["storey_room_ids"])


if __name__ == "__main__":
    unittest.main()
