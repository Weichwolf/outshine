"""Ground partition, common height plan and an independently infeasible old plan."""

import unittest

import numpy as np
import shapely

from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import HeightBudget, fit_surface_heights
from ground_surface import ground_layout, fit_ground
from surface_height_checks import normal_gradients


class Ground(unittest.TestCase):
    def setUp(self):
        self.xy = np.array([[-5., -1], [5, -1], [5, 0], [-5, 0], [-5, 1], [5, 1], [5, 2], [-5, 2]])
        self.faces = np.array([[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]])
        self.domain = shapely.box(-7, -3, 7, 4)

    def test_methods_same_layout_full_coverage_and_original_road_indices(self):
        a, report = ground_layout(self.xy, self.faces, self.domain)
        b, _ = ground_layout(self.xy, self.faces, self.domain, method="general")
        np.testing.assert_array_equal(a["xy"], b["xy"])
        np.testing.assert_array_equal(a["faces"], b["faces"])
        np.testing.assert_array_equal(a["xy"][a["road_vertex_map"]], self.xy)
        np.testing.assert_array_equal(a["faces"][:4], a["road_vertex_map"][self.faces])
        self.assertLessEqual(report["coverage_error_m2"], 1e-10)
        self.assertEqual(report["boundary_edges"], 4)

    def test_shared_replanning_avoids_steep_gap_without_silently_pinning_old_roads(self):
        layout, _ = ground_layout(self.xy, self.faces, self.domain)
        targets = np.clip(layout["xy"][:, 1], 0, 1) * 2
        frames = np.broadcast_to(np.eye(2) / .025, (len(self.faces), 2, 2))
        result, all_frames = fit_ground(layout, targets, frames)
        self.assertIsNotNone(result.mesh)
        gradient = normal_gradients(layout["xy"], layout["faces"], result.mesh.vertices[:, 2])
        self.assertLessEqual(np.linalg.norm(np.einsum("fki,fi->fk", all_frames, gradient), axis=1).max(), 1.0000001)
        self.assertGreaterEqual(result.report["maximum_height_change_m"], 2 / 3 - 1e-7)
        pinned = {int(i): float(targets[i]) for i in layout["road_vertex_map"]}
        failed = fit_surface_heights(Mesh(np.c_[layout["xy"], targets], layout["faces"]), all_frames, fixed=pinned)
        self.assertIsNone(failed.mesh)
        self.assertEqual(failed.report["solver_status"], "PrimalInfeasible")
        contact = SurfaceSampler(result.mesh).sample(np.array([[0., .5], [-6., .5]]))
        self.assertEqual(contact.reasons, ("", ""))

    def test_hole_and_selected_bridge_contact(self):
        domain = self.domain.difference(shapely.box(-1., -2.8, 1., -1.2))
        layout, report = ground_layout(self.xy, self.faces, domain)
        mesh = Mesh(np.c_[layout["xy"], np.full(len(layout["xy"]), 100.)], layout["faces"])
        floor = SurfaceSampler(mesh).sample(np.array([[0., .5], [0., -2.]]))
        self.assertEqual(floor.reasons, ("", "outside_surface"))
        bridge = Mesh(np.array([[-1., 0., 106.], [1., 0., 106.], [0., 1., 106.]]), np.array([[0, 1, 2]]))
        deck = SurfaceSampler(bridge).sample(np.array([[0., .5]]))
        self.assertEqual(floor.points_enu_m[0, 2], 100.)
        self.assertEqual(deck.points_enu_m[0, 2], 106.)
        self.assertEqual(report["boundary_edges"], 8)

    def test_bad_coverage_domain_and_budgets_explained(self):
        for faces in (np.vstack((self.faces, self.faces[:1])),):
            with self.assertRaises(ValueError):
                ground_layout(self.xy, faces, self.domain)
        with self.assertRaises(ValueError):
            ground_layout(self.xy, self.faces, shapely.box(-1, -1, 1, 1))
        with self.assertRaises(ValueError):
            ground_layout(self.xy, self.faces, self.domain, max_faces=4)
        layout, _ = ground_layout(self.xy, self.faces, self.domain)
        result, _ = fit_ground(layout, np.zeros(len(layout["xy"])), np.broadcast_to(np.eye(2), (4, 2, 2)),
                               budget=HeightBudget(time_limit_s=0))
        self.assertIsNone(result.mesh)
        self.assertEqual(result.report["reason"], "time_budget_exhausted")


if __name__ == "__main__":
    unittest.main()
