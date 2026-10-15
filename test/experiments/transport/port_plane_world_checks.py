"""Joint soil/road variables, all red/green cases and whole original port ancestry."""

import io
import unittest
from contextlib import redirect_stdout

import numpy as np
from port_plane_adaptive import solve_adaptive
from port_plane_checks import shared_strip
from port_plane_layout import prepare_plane_field
from port_plane_refinement import children, refine
from port_plane_system import fit_planes
from port_plane_world import assemble_world, finite_ports


def strip_and_soil():
    product = shared_strip()
    xy = np.vstack([product["vertices"], [[110, 12], [-10, 12]]])
    faces = np.vstack(
        [product["indices"], [[9, 1, 3], [9, 3, 5], [9, 5, 7], [9, 7, 8]]]
    )
    parts = np.r_[product["face_owners"], [-1, -1, -1, -1]]
    frames = np.tile(np.eye(2) / (2 / 3), (len(faces), 1, 1))
    frames[:6] = np.diag([1 / 0.08, 1 / 0.025])
    frames[
        np.flatnonzero(parts >= 0)[product["part_roles"][parts[parts >= 0]] == 1]
    ] = np.eye(2) / 0.025
    return product, {
        "xy": xy,
        "faces": faces,
        "parts": parts,
        "frames": frames,
        "targets": 7 + xy @ [0.02, 0.01],
        "roots": np.arange(len(faces)),
        "port_roots": {(2, 3): 0, (4, 5): 1},
    }


class JointPlaneChecks(unittest.TestCase):
    def test_complete_adaptive_pipeline_on_a_joint_affine_tile(self):
        product, mesh = strip_and_soil()
        field, _ = prepare_plane_field(product)
        captured = {
            "ground_xy": mesh["xy"],
            "ground_faces": mesh["faces"],
            "ground_frames": mesh["frames"],
            "ground_targets": mesh["targets"],
            "road_face_count": 6,
            "road_vertex_map": np.arange(8),
            "road_frames": mesh["frames"][:6],
        }
        with redirect_stdout(io.StringIO()):
            best, records = solve_adaptive(
                field, product, captured, seconds=1, rounds=1
            )
        self.assertIsNotNone(best)
        final, _, _, audit, _ = best
        self.assertTrue(audit["whole_world_valid"])
        self.assertTrue(audit["geometry_complete"])
        np.testing.assert_allclose(final["heights"], mesh["targets"], atol=1e-7, rtol=0)
        self.assertEqual(len(records), 1)

    def test_curved_full_width_transition_is_refined_with_matching_soil(self):
        product, mesh = strip_and_soil()
        x, y = mesh["xy"].T
        target = np.where(x <= 0, 0.02 * x, 3 - 0.01 * (x - 100)) + 0.012 * y
        field, _ = prepare_plane_field(product)
        captured = {
            "ground_xy": mesh["xy"],
            "ground_faces": mesh["faces"],
            "ground_frames": mesh["frames"],
            "ground_targets": target,
            "road_face_count": 6,
            "road_vertex_map": np.arange(8),
            "road_frames": mesh["frames"][:6],
        }
        with redirect_stdout(io.StringIO()):
            best, _ = solve_adaptive(field, product, captured, seconds=1, rounds=3)
        self.assertIsNotNone(best)
        final, _, _, audit, _ = best
        self.assertTrue(audit["whole_world_valid"])
        self.assertTrue(audit["geometry_complete"], audit)
        self.assertLessEqual(audit["maximum_height_chord_bound_m"], 0.01)
        self.assertLessEqual(
            audit["finite_contacts"]["maximum_finite_contact_grade_jump"], 0.005
        )
        self.assertGreater(len(final["faces"]), len(mesh["faces"]))
        np.testing.assert_array_equal(final["xy"][: len(mesh["xy"])], mesh["xy"])

    def test_all_green_templates_preserve_oriented_parent_area(self):
        xy = np.array([[0, 0], [1, 0], [0, 1], [0.5, 0], [0.5, 0.5], [0, 0.5]])
        for mask in range(8):
            mids = [3 + side if mask & (1 << side) else -1 for side in range(3)]
            result = np.array(children([0, 1, 2], mids))
            p = xy[result]
            a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
            area = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
            self.assertTrue(np.all(area > 0))
            self.assertAlmostEqual(area.sum(), 1)

    def test_joint_fit_and_refinement_keep_soil_and_complete_shared_ports(self):
        product, mesh = strip_and_soil()
        field, _ = prepare_plane_field(product)
        initial_xy = mesh["xy"].copy()
        for step in range(2):
            h, g, equal, _vertex_parts, system = assemble_world(
                field, product, mesh["xy"], mesh["faces"], mesh["parts"], mesh["frames"]
            )
            self.assertGreater(system["soil_height_unknowns"], 0)
            coefficients, report = fit_planes(h, g, equal, mesh["targets"])
            self.assertTrue(report["valid"], report)
            result = h @ coefficients + report["datum_m"]
            np.testing.assert_allclose(result, mesh["targets"], atol=1e-7, rtol=0)
            _, ports = finite_ports(
                mesh["xy"],
                mesh["faces"],
                mesh["parts"],
                result,
                mesh["port_roots"],
                product["part_roles"],
            )
            self.assertEqual(ports["original_ports"], 2)
            self.assertLess(ports["maximum_finite_contact_grade_jump"], 1e-7)
            if step == 0:
                marked = mesh["parts"] == 1
                refined, note = refine(**mesh, marked=marked)
                self.assertTrue(note["valid"], note)
                mesh = refined
        np.testing.assert_array_equal(mesh["xy"][: len(initial_xy)], initial_xy)
        self.assertGreater(len(mesh["port_roots"]), 2)

    def test_refinement_budget_does_not_return_a_partial_changed_mesh(self):
        _, mesh = strip_and_soil()
        result, report = refine(
            **mesh,
            marked=np.ones(len(mesh["faces"]), bool),
            max_vertices=len(mesh["xy"]),
        )
        self.assertIsNone(result)
        self.assertEqual(report["reason"], "refinement_vertex_budget")


if __name__ == "__main__":
    unittest.main()
