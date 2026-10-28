"""Noncentral edge hats, coupled splits and one unchanged original relief case."""

import unittest
from unittest.mock import Mock

import numpy as np
import shapely
from ground_contact import build_contact
from ground_contact_checks import qualify_contact
from ground_contact_inputs import ContactContext
from ground_multicycle import generate
from ground_refinement import _candidates, _quality_delta, _split, refine_ground
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from replay_ground_multicycle import prepare_inputs


def square():
    return Mesh(
        np.array([[0, 0, 0], [10, 0, 0], [10, 10, 0], [0, 10, 0]], float),
        np.array([[0, 1, 2], [0, 2, 3]]),
    )


def controls(mesh, first=0):
    bary = (
        np.array([[i, j, 8 - i - j] for i in range(9) for j in range(9 - i)], float) / 8
    )
    return np.einsum(
        "sj,fji->fsi", bary, mesh.vertices[mesh.triangles[first:]]
    ).reshape(-1, 3)


class NoncentralEdgeFields(unittest.TestCase):
    def test_piecewise_hat_matches_independent_triangle_interpolation(self):
        mesh = square()
        candidates = _candidates(mesh, 0, "quartiles")
        np.testing.assert_array_equal(candidates["parameter"], [0.25, 0.5, 0.75])
        bary = (
            np.array([[i, j, 8 - i - j] for i in range(9) for j in range(9 - i)], float)
            / 8
        )
        for chosen, parameter in enumerate(candidates["parameter"]):
            refined, _ = _split(
                mesh, np.arange(2), candidates, [chosen], np.ones(3), "quartiles"
            )
            sampler = SurfaceSampler(refined)
            for face, endpoint_column in ((0, 2), (1, 1)):
                points = bary @ mesh.vertices[mesh.triangles[face]]
                actual = sampler.sample(points[:, :2])
                self.assertTrue(all(not reason for reason in actual.reasons))
                expected = np.minimum(
                    bary[:, 0] / (1 - parameter), bary[:, endpoint_column] / parameter
                )
                np.testing.assert_allclose(
                    actual.points_enu_m[:, 2], expected, atol=1e-12, rtol=0
                )
            polygons = shapely.polygons(refined.vertices[refined.triangles, :2])
            self.assertTrue(shapely.coverage_is_valid(polygons))
            self.assertAlmostEqual(float(shapely.area(polygons).sum()), 100)
            np.testing.assert_array_equal(refined.vertices[:4], mesh.vertices)

    def test_quality_interval_uses_noncentral_hat_peak_and_keeps_zero_fallback(self):
        mesh = square()
        candidates = _candidates(mesh, 0, "quartiles")
        for chosen in (0, 2):
            for requested, expected in ((10, 2), (-10, 0)):
                delta = np.full(3, requested, float)
                adjusted, queries = _quality_delta(
                    mesh,
                    candidates,
                    [chosen],
                    delta,
                    lambda p: np.ones(len(p)),
                    "quartiles",
                )
                self.assertAlmostEqual(float(adjusted[0]), expected, places=12)
                self.assertEqual(queries, 90)

    def test_existing_midpoint_misses_relief_while_noncentral_split_remains_conforming(
        self,
    ):
        mesh = square()
        target = lambda p: np.sin(2 * np.pi * p[:, 0] / 10) ** 2
        old, _, old_report = refine_ground(mesh, target, max_new_vertices=1)
        self.assertIs(old, mesh)
        self.assertEqual(old_report["new_vertices"], 0)
        for method in ("quartiles",):
            result, _, report = refine_ground(
                mesh, target, method=method, max_new_vertices=1
            )
            self.assertEqual(report["new_vertices"], 1)
            self.assertEqual(len(result.triangles), 4)
            q = result.vertices[result.triangles]
            normal = np.cross(q[:, 1] - q[:, 0], q[:, 2] - q[:, 0])
            self.assertLessEqual(
                float(np.linalg.norm(normal[:, :2] / normal[:, 2, None], axis=1).max()),
                (2 / 3) * 1.0000001,
            )
            self.assertLessEqual(report["total_target_queries"], 65536)

    def test_query_capacity_stops_before_expansion_or_target_and_preserves_valid_mesh(
        self,
    ):
        target = Mock(side_effect=AssertionError("target work"))
        for method in ("quartiles",):
            mesh = square()
            result, _, report = refine_ground(
                mesh, target, method=method, max_quality_queries=2
            )
            self.assertIs(result, mesh)
            self.assertEqual(report["reason"], "candidate_query_budget_exhausted")
            self.assertEqual(report["total_target_queries"], 0)
        target.assert_not_called()


class OriginalOffcentreRelief(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        index, cases, terrain, budget, metadata = prepare_inputs()
        case = next(c for c in cases if c["seed"] == (2061326783104604389, 0))
        product = generate(index, case["selected"], terrain, budget)
        cls.context = ContactContext(
            product["mesh"], product["anchor"], terrain, np.empty(0), {}, metadata, 0
        )
        cls.context.road_dem_z = cls.context.sample_z(cls.context.road.vertices[:, :2])

    def test_common_frozen_xy_controls_improve_without_changing_road_or_outer_profiles(
        self,
    ):
        context = self.context
        results = {
            method: build_contact(
                context.road,
                context.road_dem_z,
                context.sample_z,
                refinement_method=method,
            )
            for method in ("midpoint", "quartiles")
        }
        common = np.unique(
            np.vstack(
                [controls(result["mesh"], 142)[:, :2] for result in results.values()]
            ),
            axis=0,
        )
        truth = context.sample_z(common)
        errors = {}
        for method, result in results.items():
            checked = qualify_contact(context.road, result)
            self.assertEqual(checked["whole_road_soil_edges"], 132)
            self.assertEqual(checked["outer_xyz_exact"], 4)
            sampled = SurfaceSampler(result["mesh"]).sample(common)
            self.assertTrue(all(not reason for reason in sampled.reasons))
            errors[method] = float(abs(sampled.points_enu_m[:, 2] - truth).max())
        self.assertGreater(errors["midpoint"], 0.28)
        self.assertLess(errors["quartiles"], 0.25)
        self.assertEqual(results["midpoint"]["refinement"]["new_vertices"], 0)


if __name__ == "__main__":
    unittest.main()
