"""Physical counterexamples remain rejected despite closed/manifold process success."""

import json
import unittest
from unittest.mock import patch

import numpy as np
from outshine_ref.contracts import Mesh
from stair_junction_body import build
from stair_junction_layout import layout as road_layout
from stair_junction_solve import solve
from stair_network_geometry import vertex_links
from stair_outer_inputs import FIXTURE, load
from stair_terrain_audit import body_proof, clearance, top_surface
from stair_terrain_layout import gentle_candidate, partition, selected_surfaces
from stair_terrain_solid import protected_excavation, retaining_body, split_roots


def crossing_layout():
    xy = np.array([[0.0, 0], [1, 0], [2, 0], [0, 2], [1, 2], [2, 2]])
    return {
        "xy": xy,
        "faces": np.array([[0, 1, 4], [0, 4, 3], [1, 2, 5], [1, 5, 4]]),
        "ground": np.array([True, True, False, False]),
        "targets": np.zeros(6),
        "bed": xy[:, 1] - 1,
    }


class TerrainCounters(unittest.TestCase):
    def test_unsplit_cut_to_fill_seam_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "zero root"):
            retaining_body(crossing_layout())

    def test_true_zero_root_closes_both_sides_without_a_bow_tie_wall(self):
        layout = split_roots(crossing_layout())
        self.assertEqual(int(layout["root_count"]), 1)
        np.testing.assert_array_equal(layout["xy"][-1], [1, 1])
        body, data = retaining_body(layout)
        proof = body_proof(body)
        self.assertEqual(proof["zero_area_faces"], 0)
        self.assertGreater(proof["closed_oriented_volume_m3"], 0)
        self.assertTrue(data["retaining_wall"].any())
        self.assertEqual(
            vertex_links(body.vertices, body.triangles)[
                "cyclic_connected_vertex_links"
            ],
            len(body.vertices),
        )

    def test_float32_root_fixture_still_has_real_nonzero_faces(self):
        body, _ = retaining_body(split_roots(crossing_layout()))
        exported = Mesh(body.vertices.astype(np.float32).astype(float), body.triangles)
        self.assertEqual(body_proof(exported)["zero_area_faces"], 0)

    def test_height_surface_excludes_actual_vertical_retaining_faces(self):
        body, data = retaining_body(split_roots(crossing_layout()))
        cap = top_surface(body)
        self.assertEqual(len(cap.triangles), len(data["top_faces"]))

    def test_invalid_grid_is_rejected_before_any_dem_or_geometry_work(self):
        ground = Mesh(
            np.array([[0.0, 0, 0], [1, 0, 0], [0, 1, 0]]), np.array([[0, 1, 2]])
        )
        with patch(
            "stair_terrain_layout.sample_xy",
            side_effect=AssertionError("unexpected DEM work"),
        ):
            for spacing in (0, 1, 100, float("nan")):
                with self.assertRaises(ValueError):
                    partition(ground, {}, {}, spacing_m=spacing)


class RealTerrainCounters(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _case, rows, nodes, stairs, context, _recipe = load()
        plan = road_layout(rows, "width", junctions=nodes, stairs=stairs)
        heights, note = solve(plan)
        if heights is None:
            raise ValueError(note)
        cls.road, data, _geometry = build(plan, heights)
        cls.ground, cls.upper = selected_surfaces(data, rows)
        recipe = json.loads(FIXTURE.read_text())
        with np.load(FIXTURE.with_name(recipe["elevation"]["window_file"])) as raster:
            raster = dict(raster)
        cls.layout, _note = partition(cls.ground, context, raster)
        cls.retaining, _parts = retaining_body(split_roots(cls.layout))
        cls.excavated, cls.exported, cls.excavation = protected_excavation(
            cls.retaining, cls.ground, cls.upper
        )

    def test_fixed_terrain_triangles_cannot_satisfy_the_existing_soil_grade(self):
        note = gentle_candidate(self.layout)
        self.assertFalse(note["has_checked_surface"])
        self.assertGreater(note["fully_fixed_face_grade"], note["soil_grade_recipe"])
        self.assertIn("infeasible", note["fit"]["reason"])

    def test_closed_retaining_body_still_intersects_the_actual_bridge(self):
        self.assertEqual(body_proof(self.retaining)["zero_area_faces"], 0)
        proof = clearance(top_surface(self.retaining), self.upper)
        self.assertLess(proof["minimum_clearance_m"], -1)

    def test_boolean_success_does_not_validate_a_naively_cast_float32_mesh(self):
        naive = Mesh(
            self.excavated.vertices.astype(np.float32).astype(float),
            self.excavated.triangles,
        )
        self.assertGreater(body_proof(naive)["zero_area_faces"], 0)
        self.assertGreater(self.excavation["removed_volume_m3"], 0)

    def test_actual_float32_reconstruction_is_saved_separately(self):
        np.testing.assert_array_equal(
            self.exported.vertices, self.exported.vertices.astype(np.float32)
        )
        self.assertLess(len(self.exported.triangles), len(self.excavated.triangles))
        self.assertEqual(body_proof(self.exported)["zero_area_faces"], 0)


if __name__ == "__main__":
    unittest.main()
