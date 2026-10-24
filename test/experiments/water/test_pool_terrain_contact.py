"""Exact local partition, Float64 contact and explicit unresolved neighbor obligations."""

import unittest
from fractions import Fraction

import numpy as np
from outshine_ref.water.geodetic import local_points
from pool_body_experiment import FIXTURE, construct, original
from pool_cell_partition import _area, partition
from pool_terrain_contact import contact_patch


class PoolTerrainContact(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data, source, _, triangles, _ = original(FIXTURE)
        cls.body = construct(data, source, triangles)
        ll = np.array([[p["lon"], p["lat"]] for p in data["water"]["points"]])
        cls.terrain = local_points(
            data["water"]["meshAnchor"],
            ll,
            np.array(data["terrain"]["cellCornersM"]).ravel(),
        )
        cls.faces = np.array([[0, 2, 3], [0, 3, 1]])

    def test_exact_partition_area_and_shared_edge_incidence(self):
        xy, faces, points, work = partition(self.body, self.terrain, self.faces)
        generated = sum(_area([points[i] for i in f]) for f in faces)
        self.assertEqual(generated, Fraction(work["exact_doubled_area"]))
        self.assertTrue(all(_area([points[i] for i in f]) > 0 for f in faces))
        edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        edges, counts = np.unique(edges, axis=0, return_counts=True)
        self.assertTrue(np.all((counts == 1) | (counts == 2)))
        self.assertEqual(len(points) - len(edges) + len(faces), 1)
        self.assertTrue(
            all(tuple(p) in set(map(tuple, xy)) for p in self.terrain[:, :2])
        )

    def test_float_materialization_is_shared_and_reports_real_rounding(self):
        xy, _, points, work = partition(self.body, self.terrain, self.faces)
        self.assertEqual(len(np.unique(xy, axis=0)), len(points))
        for p, actual in zip(points, xy):
            for i in (0, 1):
                error = abs(Fraction.from_float(float(actual[i])) - p[i])
                half_ulp = Fraction.from_float(float(abs(np.spacing(actual[i])))) / 2
                self.assertLessEqual(error, half_ulp)
        self.assertTrue(
            any(Fraction(v) > 0 for v in work["maximum_xy_rounding_fraction_m"])
        )

    def test_whole_contact_is_precise_but_the_patch_is_not_falsely_world_ready(self):
        vertices, _, obligations, report = contact_patch(
            self.body, self.terrain, self.faces
        )
        self.assertLess(report["contact"]["maximum_above_contact_surface_m"], 1e-7)
        self.assertLess(report["contact"]["maximum_below_contact_surface_m"], 1e-7)
        self.assertAlmostEqual(
            report["contact"]["compared_area_m2"],
            report["expected_overlap_area_m2"],
            places=10,
        )
        self.assertTrue(report["neighbor_update_required"])
        self.assertEqual(report["admission"], "candidate_pending_neighbor_updates")
        self.assertGreater(report["maximum_outer_boundary_height_change_m"], 1.0)
        self.assertTrue(np.all(np.isfinite(vertices)))
        self.assertEqual(
            len(obligations), len(report["work"]["exact_boundary_parameters"])
        )
        for port in report["work"]["exact_boundary_parameters"]:
            t = Fraction(port["parameter"])
            self.assertTrue(0 <= t <= 1)
            a, b = self.terrain[port["input_edge"], :2]
            expected = [
                float(
                    Fraction.from_float(float(a[i]))
                    + t
                    * (
                        Fraction.from_float(float(b[i]))
                        - Fraction.from_float(float(a[i]))
                    )
                )
                for i in (0, 1)
            ]
            np.testing.assert_array_equal(vertices[port["output_vertex"], :2], expected)

    def test_cancellation_and_multiple_outer_edges_are_not_unqualified_successes(self):
        saved = self.terrain.copy()
        with self.assertRaisesRegex(ValueError, "cancelled"):
            partition(self.body, self.terrain, self.faces, cancelled=lambda: True)
        low = self.body["vertices"][:, :2].min(axis=0) - 1
        high = self.body["vertices"][:, :2].max(axis=0) + 1
        xy = np.array(
            [[low[0], high[1]], [high[0], high[1]], [low[0], low[1]], [high[0], low[1]]]
        )
        with self.assertRaisesRegex(ValueError, "exactly one crossing"):
            partition(self.body, np.column_stack((xy, np.ones(4))), self.faces)
        np.testing.assert_array_equal(saved, self.terrain)


if __name__ == "__main__":
    unittest.main()
