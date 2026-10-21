"""Actual transverse-envelope regression and constructed station ownership."""

import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from mvt_t_connection_inputs import inputs
from mvt_t_connections import plan_connections
from mvt_t_profile_audit import audit_profiles
from mvt_t_profile_layout import prepare_profiles
from mvt_t_profile_products import shared_products
from mvt_t_profile_solve import solve_profiles
from mvt_t_stair_loft import prepare_stair_lofts
from stair_junction_body import build
from stair_junction_layout import cross_sections
from surface_height_checks import normal_gradients


def prepared(recipe="station_loft"):
    case, sources = inputs()
    return prepare_profiles(
        case,
        sources,
        plan_connections(sources),
        mesh_recipe="outward_binary_grid",
        stair_recipe=recipe,
    )


class PreparedStationLoft(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plan = prepared()
        cls.heights, cls.note = solve_profiles(cls.plan)
        if cls.heights is None:
            raise ValueError(cls.note)
        cls.body, cls.data, cls.geometry = build(cls.plan, cls.heights)

    def test_recorded_old_facet_has_real_crossfall_and_fails_new_audit(self):
        path = (
            Path(__file__).with_name("fixtures")
            / "mvt_stair_nonlevel_envelope_face.json"
        )
        record = json.loads(path.read_text())
        xyz, axis = (
            np.array(record["triangle_xyz_m"]),
            np.array(record["source_axis_xy_m"]),
        )
        tangent = axis[-1] - axis[0]
        tangent /= np.linalg.norm(tangent)
        gradient = normal_gradients(xyz[:, :2], np.array([[0, 1, 2]]), xyz[:, 2])
        self.assertGreater(abs(gradient[0] @ [-tangent[1], tangent[0]]), 0.16)
        row = {
            "xy": axis,
            "gradient": record["declared_max_gradient"],
            "original": {"source": {"feature": {"properties": {"subclass": "steps"}}}},
        }
        plan = {"xy": xyz[:, :2], "rows": [row], "parts": [np.array([[0, 1, 2]])]}
        with self.assertRaisesRegex(ValueError, "transverse"):
            audit_profiles(plan, xyz[:, 2], {})

    def test_old_world_xy_relookup_finds_no_declared_sections_in_fixed_input(self):
        plan = prepared("legacy_sections")
        owner = plan["stairs"][0][0]
        vertices = np.unique(plan["parts"][owner])
        counts = [
            int(shapely.dwithin(line, shapely.points(plan["xy"][vertices]), 1e-7).sum())
            for line in cross_sections(plan["rows"][owner])
        ]
        self.assertEqual(counts, [0] * 7)

    def test_every_actual_flight_facet_has_level_transverse_gradient(self):
        proof = audit_profiles(self.plan, self.heights, self.data)
        self.assertLess(max(proof["whole_flight_transverse_gradients"].values()), 1e-7)
        self.assertLess(self.note["variables"], self.note["unreduced_variables"])
        self.assertGreater(self.plan["station_refinement"]["edge_intersections"], 0)

    def test_shared_cuts_keep_source_coverage_and_declared_geometry_budget(self):
        note = self.plan["station_refinement"]
        self.assertLess(max(note["source_coverage_errors_m2"]), 1e-7)
        self.assertLessEqual(note["maximum_extra_flat_m"], 2**-10)
        for faces, domain in zip(
            self.plan["parts"], self.plan["prepared_domains"], strict=True
        ):
            polygons = shapely.polygons(self.plan["xy"][faces])
            self.assertTrue(shapely.coverage_is_valid(polygons))
            self.assertLess(
                shapely.union_all(polygons).symmetric_difference(domain).area, 1e-7
            )

    def test_explicit_equality_formulation_has_same_quality_and_finite_objective(self):
        heights, note = solve_profiles(self.plan, height_formulation="equalities")
        self.assertIsNotNone(heights, note)
        self.assertAlmostEqual(note["objective"], self.note["objective"], places=7)
        _body, data, _geometry = build(self.plan, heights)
        audit_profiles(self.plan, heights, data)
        self.assertGreater(note["variables"], self.note["variables"])

    def test_budget_refusal_preserves_the_original_prepared_plan(self):
        plan = prepared("legacy_sections")
        xy, faces = plan["xy"].copy(), [p.copy() for p in plan["parts"]]
        with self.assertRaisesRegex(ValueError, "vertex budget"):
            prepare_stair_lofts(plan, max_vertices=len(xy))
        np.testing.assert_array_equal(plan["xy"], xy)
        for before, after in zip(faces, plan["parts"], strict=True):
            np.testing.assert_array_equal(before, after)

    def test_consumer_plan_carries_the_same_flight_frame_and_cut_stations(self):
        product = shared_products(self.plan, self.heights, self.data)
        self.assertEqual(len(product["stair_flights"]), len(self.plan["stair_lofts"]))
        for record, source in zip(
            product["stair_flights"], self.plan["stair_lofts"], strict=True
        ):
            self.assertEqual(record["source_id"], source["source_id"])
            self.assertEqual(record["start_cut_m"], source["start_cut_m"])
            self.assertEqual(record["end_cut_m"], source["end_cut_m"])


if __name__ == "__main__":
    unittest.main()
