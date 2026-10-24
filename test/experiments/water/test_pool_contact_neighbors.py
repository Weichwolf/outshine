"""A shared seam needs common knots as well as matching endpoint heights."""

import unittest
from itertools import pairwise

import numpy as np
from outshine_ref.water.geodetic import local_points
from pool_body_experiment import FIXTURE, construct, original
from pool_cell_partition import partition
from pool_contact_neighbors import edge_profile, reconcile_pair
from pool_region_experiment import controls
from pool_terrain_contact import contact_patch


class PoolContactNeighbors(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.body, cls.source, cls.faces = next(
            c for c in controls(FIXTURE) if c[0] == "original_outline_rectangle"
        )
        cls.initial, cls.updated, cls.report = reconcile_pair(
            cls.body, cls.source, cls.faces[:1], cls.faces[1:]
        )

    def test_existing_outline_control_exposes_and_closes_the_shared_seam(self):
        before = [edge_profile(self.source, p, (0, 3)) for p in self.initial]
        after = [edge_profile(self.source, p, (0, 3)) for p in self.updated]
        self.assertEqual(list(map(len, before)), [13, 11])
        self.assertGreater(self.report["maximum_initial_seam_gap_m"], 3)
        self.assertEqual(list(map(len, after)), [15, 15])
        self.assertEqual(after[0].keys(), after[1].keys())
        self.assertEqual(self.report["maximum_reconciled_seam_gap_m"], 0)
        self.assertEqual(self.report["shared_internal_knots"], 13)
        self.assertEqual(self.report["requested_missing_knots_per_side"], [2, 4])
        self.assertEqual(self.report["rebuilt_patches"], 2)
        for t in after[0]:
            np.testing.assert_array_equal(after[0][t], after[1][t])
        for _, _, _, report in self.updated:
            self.assertEqual(report["admission"], "candidate_pending_neighbor_updates")
            self.assertLess(report["contact"]["maximum_above_contact_surface_m"], 1e-7)
            self.assertLess(report["contact"]["maximum_below_contact_surface_m"], 1e-7)

    def test_reconciled_components_form_one_indexed_disk_without_epsilon_welding(self):
        vertices = np.vstack([p[0] for p in self.updated])
        xy, inverse = np.unique(vertices[:, :2], axis=0, return_inverse=True)
        faces = inverse[
            np.vstack(
                (self.updated[0][1], self.updated[1][1] + len(self.updated[0][0]))
            )
        ]
        edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        unique, counts = np.unique(edges, axis=0, return_counts=True)
        self.assertTrue(np.all((counts == 1) | (counts == 2)))
        self.assertEqual(len(xy) - len(unique) + len(faces), 1)
        profile = edge_profile(self.source, self.updated[0], (0, 3))
        slots = {tuple(p): i for i, p in enumerate(xy)}
        incidence = dict(zip(map(tuple, unique), counts))
        keys = sorted(profile)
        for a, b in pairwise(keys):
            edge = tuple(
                sorted((slots[tuple(profile[a][:2])], slots[tuple(profile[b][:2])]))
            )
            self.assertEqual(incidence[edge], 2)

    def test_requests_preserve_exact_source_area_and_reversed_edge_binding(self):
        source = self.source[self.faces[0]]
        faces = np.array([[0, 1, 2]])
        requests = [{"input_edge": [2, 0], "parameter": "1/3"}]
        xy, result, points, work = partition(
            self.body, source, faces, boundary_requests=requests
        )
        reverse = [{"input_edge": [0, 2], "parameter": "2/3"}]
        other = partition(self.body, source, faces, boundary_requests=reverse)
        np.testing.assert_array_equal(xy, other[0])
        np.testing.assert_array_equal(result, other[1])
        self.assertEqual(points, other[2])
        self.assertEqual(work["exact_doubled_area"], other[3]["exact_doubled_area"])
        self.assertEqual(work["boundary_requested_nodes"], 1)

    def test_original_cell_reuses_matching_first_passes_without_rebuilding(self):
        data, source, _, water, _ = original(FIXTURE)
        body = construct(data, source, water)
        lonlat = np.array([[p["lon"], p["lat"]] for p in data["water"]["points"]])
        terrain = local_points(
            data["water"]["meshAnchor"],
            lonlat,
            np.array(data["terrain"]["cellCornersM"]).ravel(),
        )
        initial, updated, report = reconcile_pair(
            body, terrain, np.array([[0, 2, 3]]), np.array([[0, 3, 1]])
        )
        self.assertEqual(report["requested_missing_knots_per_side"], [0, 0])
        self.assertEqual(report["rebuilt_patches"], 0)
        self.assertTrue(all(a is b for a, b in zip(initial, updated)))

    def test_invalid_internal_outside_unbounded_and_geos_requests_decline(self):
        for request in (
            {"input_edge": [0, 3], "parameter": "1/2"},
            {"input_edge": [0, 1], "parameter": "2"},
            {"input_edge": [0, 1], "parameter": "1/0"},
            {"input_edge": [0, 1], "parameter": "1e999999999"},
            {"input_edge": [0, 1], "parameter": "1" * 513},
        ):
            with self.subTest(request=request), self.assertRaises(ValueError):
                partition(
                    self.body, self.source, self.faces, boundary_requests=[request]
                )
        request = {"input_edge": [0, 1], "parameter": "1/2"}
        with self.assertRaisesRegex(ValueError, "bounded exact boundary requests"):
            partition(
                self.body, self.source, self.faces, boundary_requests=[request] * 1025
            )
        with self.assertRaisesRegex(ValueError, "node budget"):
            partition(
                self.body,
                self.source,
                self.faces,
                boundary_requests=[
                    {"input_edge": [0, 1], "parameter": f"{i}/1025"}
                    for i in range(1, 1025)
                ],
            )
        with self.assertRaisesRegex(ValueError, "collapse during Float64"):
            partition(
                self.body,
                self.source,
                self.faces,
                boundary_requests=[
                    {"input_edge": [0, 1], "parameter": "1/1" + "0" * 400}
                ],
            )
        with self.assertRaisesRegex(ValueError, "require the exact"):
            contact_patch(
                self.body,
                self.source,
                self.faces,
                method="geos",
                boundary_requests=[request],
            )
        with self.assertRaisesRegex(ValueError, "nonoverlapping"):
            reconcile_pair(self.body, self.source, self.faces[:1], self.faces[:1])


if __name__ == "__main__":
    unittest.main()
