"""Whole affine profiles, exact boundary controls and actual narrow contact."""

import unittest
from fractions import Fraction

import numpy as np
from ground_contact import ContactLimits, build_contact, corner_witness
from ground_contact_checks import qualify_contact
from ground_contact_inputs import load_context
from ground_contact_profiles import WholeProfileGuard, exact_squared_excess
from outshine_ref.contracts import Mesh
from replay_ground_contact import signature


def fixture():
    road = Mesh(
        np.array([[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]]),
        np.array([[0, 1, 2]], dtype=np.uint32),
    )
    corners = np.array([[-10.0, -10.0], [10.0, -10.0], [10.0, 10.0], [-10.0, 10.0]])
    return road, corners


class AffineProfileControls(unittest.TestCase):
    def test_corners_alone_miss_a_whole_edge_contradiction(self):
        road, corners = fixture()
        heights = np.array([11.0, 11.0, 0.0, 0.0])
        self.assertLess(corner_witness(road, corners, heights, 1.0)["excess_m"], 0)
        guard = WholeProfileGuard(road, 1.0)
        proof = guard(corners, heights)
        self.assertEqual(proof["state"], "contradiction")
        self.assertGreater(Fraction(proof["exact_squared_excess_m2"]), 0)
        outer = np.c_[corners, heights]
        edge = proof["outer_edge"]
        checked = exact_squared_excess(
            road.vertices[proof["road_edge"]],
            outer[[edge, (edge + 1) % 4]],
            *proof["parameters"],
            1.0,
        )
        self.assertEqual(checked, Fraction(proof["exact_squared_excess_m2"]))
        self.assertTrue(any(0 < v < 1 for v in proof["parameters"]))
        self.assertEqual(guard.work["exact_road_faces"], 1)

    def test_exact_boundary_and_one_ulp_on_each_side(self):
        road, corners = fixture()
        for height in (np.nextafter(10.0, 0), 10.0, np.nextafter(10.0, np.inf)):
            proof = WholeProfileGuard(road, 1.0)(corners, [height, height, 0, 0])
            self.assertEqual(proof["state"] == "contradiction", height > 10)

    def test_known_feasible_affine_and_piecewise_fields_are_not_rejected(self):
        road, corners = fixture()
        for field in (
            lambda p: 0.3 * p[:, 0] - 0.4 * p[:, 1],
            lambda p: 0.5 * abs(p[:, 0]),
        ):
            vertices = road.vertices.copy()
            vertices[:, 2] = field(vertices)
            mesh = Mesh(vertices, road.triangles)
            proof = WholeProfileGuard(mesh, 1.0)(corners, field(corners))
            self.assertEqual(proof["state"], "no_conclusion")

    def test_reordering_and_metric_transform_preserve_negative_witness(self):
        road, corners = fixture()
        for scale, offset in ((1.0, (0.0, 0.0, 0.0)), (8.0, (32.0, -16.0, 128.0))):
            vertices = road.vertices * scale + offset
            mesh = Mesh(vertices[[2, 0, 1]], np.array([[0, 1, 2]], dtype=np.uint32))
            outer = np.c_[corners, [11.0, 11.0, 0.0, 0.0]] * scale + offset
            for order in (np.arange(4), np.array([2, 1, 0, 3])):
                proof = WholeProfileGuard(mesh, 1.0)(outer[order, :2], outer[order, 2])
                self.assertEqual(proof["state"], "contradiction")

    def test_budget_end_is_no_conclusion_and_does_not_mutate_source(self):
        road, corners = fixture()
        before = road.vertices.tobytes(), road.triangles.tobytes()
        for options in (
            {"maximum_pairs": 0},
            {"maximum_exact_checks": 0},
            {"maximum_road_faces": 0},
        ):
            guard = WholeProfileGuard(road, 1.0, **options)
            self.assertEqual(guard(corners, [11, 11, 0, 0])["state"], "no_conclusion")
            self.assertEqual(guard.work["exact_road_faces"], 0)
        self.assertEqual(before, (road.vertices.tobytes(), road.triangles.tobytes()))

    def test_excessive_road_gradient_cannot_prove_global_profile_conflict(self):
        road, corners = fixture()
        vertices = road.vertices.copy()
        vertices[1, 2] = 2
        guard = WholeProfileGuard(Mesh(vertices, road.triangles), 1.0)
        proof = guard(corners, [11, 11, 0, 0])
        self.assertEqual(proof["state"], "no_conclusion")
        self.assertEqual(proof["reason"], "uniform_road_gradient_not_proven")

    def test_exact_integer_road_condition_matches_fraction_oracle(self):
        road, _ = fixture()
        for gradient in (1.0, np.nextafter(1.0, 0), np.nextafter(1.0, np.inf), 0.6):
            vertices = road.vertices.copy()
            vertices[1, 2] = gradient
            guard = WholeProfileGuard(Mesh(vertices, road.triangles), 1.0)
            self.assertEqual(
                guard.road_condition(), Fraction(float(gradient)) ** 2 <= 1
            )
            old = guard.work.copy()
            guard.road_condition()
            self.assertEqual(old, guard.work)

    def test_invalid_limits_and_numeric_domain_do_not_admit_proofs(self):
        road, corners = fixture()
        for options in (
            {"maximum_pairs": -1},
            {"maximum_exact_checks": True},
            {"maximum_road_faces": 65537},
        ):
            with self.assertRaises(ValueError):
                WholeProfileGuard(road, 1.0, **options)
        for grade in (0.0, np.inf, np.nan):
            with self.assertRaises(ValueError):
                WholeProfileGuard(road, grade)
        for heights in ([np.nan] * 4, [1e10] * 4):
            self.assertEqual(
                WholeProfileGuard(road, 1.0)(corners, heights)["state"], "no_conclusion"
            )


class ActualWholeContactProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.context = load_context()

    def build(self, **options):
        c = self.context
        return build_contact(c.road, c.road_dem_z, c.sample_z, **options)

    def test_actual_ten_metre_conflict_precedes_mesh_and_has_exact_road_condition(self):
        result = self.build(
            limits=ContactLimits(margins_m=(10.0,)), whole_profiles=True
        )
        self.assertEqual(result["status"], "pending")
        candidate = result["candidates"][0]
        self.assertLess(candidate["corner_height_witness"]["excess_m"], 0)
        self.assertEqual(candidate["reason"], "fixed_whole_profile_conflict")
        self.assertNotIn("work", candidate)
        self.assertGreater(
            Fraction(candidate["whole_profile_preflight"]["exact_squared_excess_m2"]), 4
        )
        self.assertEqual(result["profile_proof_work"]["exact_road_faces"], 154)
        self.assertEqual(result["profile_proof_work"]["edge_pairs"], 360)

    def test_actual_tighter_family_has_identical_geometry_with_or_without_guard(self):
        limits = ContactLimits(margins_m=(5.0, 10.0, 10.5, 11.0, 12.0, 15.0, 20.0))
        old = self.build(limits=limits)
        new = self.build(limits=limits, whole_profiles=True)
        self.assertEqual(signature(old), signature(new))
        checked = qualify_contact(self.context.road, new)
        self.assertEqual(checked["road_vertices_exact"], 123)
        self.assertEqual(checked["road_faces_exact"], 154)
        self.assertEqual(checked["whole_road_soil_edges"], 90)
        self.assertEqual(new["candidates"][-1]["margin_m"], 10.5)
        self.assertLess(checked["domain_area_m2"], 2377)
        self.assertEqual(new["profile_proof_work"]["exact_witness_checks"], 2)

    def test_default_geometry_and_old_default_report_are_preserved(self):
        old = self.build()
        guarded = self.build(whole_profiles=True)
        self.assertEqual(signature(old), signature(guarded))
        self.assertNotIn("profile_proof_work", old)
        self.assertEqual(old["candidates"][-1]["margin_m"], 15.0)


if __name__ == "__main__":
    unittest.main()
