"""Independent phase ordering, solid bud topology, exact anchors and cached subsets."""

import copy
import json
import unittest
from unittest.mock import patch

import numpy as np
from flower_bud import bud_mesh
from outshine_ref.vegetation import Bellflower, Blade
from outshine_ref.vegetation.curve_frame import arc_frame
from raceme_mesh import pedicel, phase_mesh
from raceme_phase import prepare, states


class RacemeChecks(unittest.TestCase):
    def test_phase_birth_order_older_base_and_monotone_opening(self):
        recipe = prepare(121)
        births = np.asarray(recipe["births"])
        prior = np.zeros(len(births))
        for phase in np.linspace(-0.1, 2, 43):
            state = states(recipe, float(phase))
            self.assertTrue((np.diff(state["growth"]) <= 0).all())
            self.assertTrue((state["growth"] >= prior).all())
            prior = state["growth"]
        for index, birth in enumerate(births):
            self.assertFalse(states(recipe, float(birth))["active"][index])
            self.assertTrue(states(recipe, float(birth + 1e-8))["active"][index])
        self.assertTrue(states(recipe, 3)["open"].all())
        for mutate in ("births", "side_angles"):
            bad = copy.deepcopy(recipe)
            bad[mutate][-1] = np.nan
            with self.assertRaises(ValueError):
                states(bad, 1)

    def test_bud_is_closed_outward_ellipsoid_with_no_degenerate_pole_faces(self):
        for segments, sides in ((4, 6), (8, 16), (16, 32)):
            length, radius = 0.005, 0.0018
            mesh = bud_mesh(length, radius, segments, sides)
            v, f = mesh["vertices"], mesh["faces"]
            centred = v + (0, 0, length / 2)
            equation = (centred**2 / (radius**2, radius**2, (length / 2) ** 2)).sum(1)
            np.testing.assert_allclose(equation, 1, rtol=0, atol=8e-16)
            edges = np.sort(np.vstack((f[:, :2], f[:, 1:], f[:, (2, 0)])), axis=1)
            unique, counts = np.unique(edges, axis=0, return_counts=True)
            self.assertTrue((counts == 2).all())
            self.assertEqual(len(v) - len(unique) + len(f), 2)
            points = v[f]
            area = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
            self.assertTrue(
                (
                    np.einsum("ij,ikj->ik", area, mesh["normals"].reshape(-1, 3, 3)) > 0
                ).all()
            )
            np.testing.assert_allclose(
                np.linalg.norm(mesh["normals"], axis=1), 1, atol=2e-15
            )
            self.assertLessEqual(
                np.ptp(mesh["corner_uv"][:, 1].reshape(-1, 3), axis=1).max(), 0.5
            )

    def test_arched_pedicel_hits_both_saved_endpoints_and_frames_are_rigid(self):
        recipe = prepare(59)
        for phase in (0.3, 0.95, 1.3):
            state = states(recipe, phase)
            for i in np.flatnonzero(state["active"]):
                node, end, growth = (
                    state["nodes"][i],
                    state["ends"][i],
                    state["growth"][i],
                )
                _, frame, length, bend = pedicel(node, end, growth)
                endpoints = (
                    arc_frame(length, bend, 0, np.array([0.0, 1.0]))[0] @ frame.T + node
                )
                np.testing.assert_allclose(endpoints, (node, end), rtol=0, atol=4e-17)
                for f in (frame, state["frames"][i]):
                    np.testing.assert_allclose(f.T @ f, np.eye(3), atol=2e-15)
                    self.assertAlmostEqual(np.linalg.det(f), 1)

    def test_saved_partial_products_are_exact_and_budget_precedes_geometry(self):
        recipe = json.loads(json.dumps(prepare(121)))
        with patch.object(
            np.random, "Generator", side_effect=AssertionError("RNG after save")
        ):
            full, ranges, _ = phase_mesh(recipe, 0.95)
            for row in reversed(ranges):
                part, one, _ = phase_mesh(recipe, 0.95, selected=[tuple(row["id"])])
                first, last = row["vertex_range"]
                np.testing.assert_array_equal(
                    part["vertices"], full["vertices"][first:last]
                )
                self.assertEqual(len(one), 1)
        with (
            patch.object(Bellflower, "mesh", side_effect=AssertionError("over budget")),
            patch.object(Blade, "mesh", side_effect=AssertionError("over budget")),
        ):
            self.assertIsNone(phase_mesh(recipe, 0.95, max_organs=1))
            self.assertIsNone(phase_mesh(recipe, -0.1, selected=[(3, 0)]))


if __name__ == "__main__":
    unittest.main()
