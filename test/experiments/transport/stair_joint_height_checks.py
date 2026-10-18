"""Free procedural levels solve contradictory priors without relaxing grades."""

import copy
import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, fit_surface_heights
from stair_joint_height_fit import bound_levels, fit_joint_levels, level_groups
from stair_seed_audit import audit
from stair_seed_crossing_checks import crossing_example
from stair_seed_geometry import build
from surface_height_checks import normal_gradients


def strip():
    xy = np.array([[x, y] for x in range(5) for y in (0, 1)], float)
    faces = np.array(
        [[2 * x, 2 * x + 2, 2 * x + 1] for x in range(4)]
        + [[2 * x + 1, 2 * x + 2, 2 * x + 3] for x in range(4)]
    )
    mesh = Mesh(
        np.c_[xy, np.where(xy[:, 0] <= 1, 100, np.where(xy[:, 0] >= 3, 110, 105))],
        faces,
    )
    plans = [
        {"shape": shapely.box(-1, -1, 1, 2), "height_m": 100},
        {"shape": shapely.box(3, -1, 5, 2), "height_m": 110},
    ]
    frames = np.broadcast_to(np.eye(2) / 0.2, (len(faces), 2, 2)).copy()
    return mesh, plans, frames


class JointLevelHeights(unittest.TestCase):
    def test_free_levels_fit_whole_faces_where_fixed_priors_are_infeasible(self):
        surface, plans, frames = strip()
        groups = level_groups(surface.vertices[:, :2], plans)
        pins = {
            int(i): p["height_m"]
            for p, group in zip(plans, groups, strict=True)
            for i in group
        }
        fixed = fit_surface_heights(surface, frames, fixed=pins)
        self.assertIsNone(fixed.mesh)
        before = surface.vertices.copy()
        result, heights = fit_joint_levels(surface, frames, plans, HeightBudget())
        self.assertIsNotNone(result.mesh)
        np.testing.assert_array_equal(surface.vertices, before)
        np.testing.assert_array_equal(
            result.mesh.vertices[:, :2], surface.vertices[:, :2]
        )
        np.testing.assert_array_equal(heights, heights.astype("f4").astype(float))
        self.assertLess(abs(heights[1] - heights[0]), 0.401)
        self.assertLess(abs(heights.mean() - 105), 0.01)
        # Endpoint priors differ by 10 m; 2 m at grade .2 permits only .4 m.
        # Hence any admissible L-infinity fit needs at least (10-.4)/2 = 4.8 m.
        error = float(abs(result.mesh.vertices[:, 2] - surface.vertices[:, 2]).max())
        self.assertGreaterEqual(error, 4.8 - 1e-7)
        self.assertLessEqual(error, 4.80001)
        report = result.report
        self.assertLessEqual(
            report["free_levels"]["iterations"] + report["fixed_levels"]["iterations"],
            100,
        )
        for cast in (False, True):
            vertices = (
                result.mesh.vertices.astype("f4").astype(float)
                if cast
                else result.mesh.vertices
            )
            for group, h in zip(groups, heights, strict=True):
                self.assertLessEqual(np.max(abs(vertices[group, 2] - h)), 1e-7)
            gradient = normal_gradients(
                vertices[:, :2], surface.triangles, vertices[:, 2]
            )
            self.assertLessEqual(np.linalg.norm(gradient, axis=1).max(), 0.2 + 1e-7)

    def test_time_input_iteration_and_group_limits_do_not_publish_a_mesh(self):
        surface, plans, frames = strip()
        for budget in (
            HeightBudget(time_limit_s=0),
            HeightBudget(max_vertices=1),
            HeightBudget(max_iterations=1),
        ):
            result, _levels = fit_joint_levels(surface, frames, plans, budget)
            self.assertIsNone(result.mesh)
        for wrong in ([], plans * 2):
            with self.assertRaises(ValueError):
                level_groups(surface.vertices[:, :2], wrong)
        for wrong in ([100], [np.nan, 100], [100.00000001, 110]):
            with self.assertRaises(ValueError):
                bound_levels(plans, wrong)

    def test_actual_crossing_reader_binds_declared_levels_and_all_physical_profiles(
        self,
    ):
        sources, rows, recipe, state, empty = crossing_example()
        recipe["junction_height_mode"] = "joint"
        data, _network, planning = build(
            rows, sources, empty, lambda xy: 100 + xy @ [0.01, 0.02], recipe=recipe
        )
        proof = audit(state, recipe, data, rows, planning, empty, sources=sources)
        self.assertTrue(
            all(
                c["maximum_height_error_m"] <= c["z_guard_m"]
                for c in proof["new_junction_heights"]
            )
        )
        changed = dict(data, joint_level_heights_m=data["joint_level_heights_m"] + 1)
        with self.assertRaises(ValueError):
            audit(state, recipe, changed, rows, planning, empty, sources=sources)
        undeclared = copy.deepcopy(recipe)
        del undeclared["junction_height_mode"]
        with self.assertRaisesRegex(ValueError, "undeclared"):
            audit(state, undeclared, data, rows, planning, empty, sources=sources)


if __name__ == "__main__":
    unittest.main()
