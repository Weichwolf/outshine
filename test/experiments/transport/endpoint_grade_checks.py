"""Selected exterior planes must cover a full contact, including its gradients."""

import unittest

import numpy as np
from endpoint_grade_contacts import ContactSurface
from outshine_ref.contracts import Mesh


def rectangle(x0, x1, y0, y1, *, longitudinal=0.02, crossfall=0.01, height=3):
    xy = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]])
    return Mesh(
        np.c_[xy, height + longitudinal * xy[:, 0] + crossfall * xy[:, 1]],
        [[0, 1, 2], [0, 2, 3]],
    )


def combine(*meshes):
    offsets = np.cumsum([0] + [len(m.vertices) for m in meshes[:-1]])
    return Mesh(
        np.vstack([m.vertices for m in meshes]),
        np.vstack([m.triangles + offset for m, offset in zip(meshes, offsets)]),
    )


class CompleteContactChecks(unittest.TestCase):
    def test_whole_width_plane_across_internal_mesh_edges(self):
        surface = ContactSurface(rectangle(-10, 10, -4, 4))
        result = surface.strip([0, 0], [1, 0], 8, exterior_side=-1)
        self.assertTrue(result["valid"], result)
        self.assertEqual(len(result["face_indices"]), 2)
        self.assertAlmostEqual(result["center_height_m"], 3)
        self.assertAlmostEqual(result["longitudinal_grade"], 0.02)
        self.assertAlmostEqual(result["crossfall"], 0.01)

    def test_side_selects_actual_parent_at_a_shared_contact_edge(self):
        surface = ContactSurface(
            combine(
                rectangle(-10, 0, -4, 4), rectangle(0, 10, -4, 4, longitudinal=0.07)
            )
        )
        for side, expected in ((-1, 0.02), (1, 0.07)):
            result = surface.strip([0, 0], [1, 0], 8, exterior_side=side)
            self.assertTrue(result["valid"], result)
            self.assertAlmostEqual(result["longitudinal_grade"], expected)

    def test_missing_full_width_coverage_is_not_replaced_by_a_center_hit(self):
        surface = ContactSurface(rectangle(-10, 10, -1, 1))
        self.assertEqual(
            surface.strip([0, 0], [1, 0], 8, exterior_side=-1)["reason"],
            "missing_contact_coverage",
        )
        gap = ContactSurface(
            combine(rectangle(-10, 10, -4, -0.01), rectangle(-10, 10, 0.01, 4))
        )
        self.assertEqual(
            gap.strip([0, 0], [1, 0], 8, exterior_side=-1)["reason"],
            "missing_contact_coverage",
        )

    def test_stacked_or_duplicate_surface_is_ambiguous(self):
        surface = ContactSurface(
            combine(rectangle(-10, 10, -4, 4), rectangle(-10, 10, -4, 4, height=8))
        )
        self.assertEqual(
            surface.strip([0, 0], [1, 0], 8, exterior_side=-1)["reason"],
            "ambiguous_contact_surface",
        )

    def test_matching_heights_do_not_hide_a_normal_jump_across_the_width(self):
        surface = ContactSurface(
            combine(
                rectangle(-10, 10, -4, 0), rectangle(-10, 10, 0, 4, longitudinal=0.04)
            )
        )
        self.assertEqual(
            surface.strip([0, 0], [1, 0], 8, exterior_side=-1)["reason"],
            "nonplanar_contact_strip",
        )

    def test_face_and_candidate_budgets_precede_query_emission(self):
        mesh = rectangle(-10, 10, -4, 4)
        with self.assertRaises(ValueError):
            ContactSurface(mesh, max_faces=1)
        self.assertEqual(
            ContactSurface(mesh).strip(
                [0, 0], [1, 0], 8, exterior_side=-1, max_candidates=1
            )["reason"],
            "contact_candidate_budget",
        )


if __name__ == "__main__":
    unittest.main()
