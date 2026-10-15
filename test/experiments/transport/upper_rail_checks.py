"""Counterexamples for whole patch selection and real old-body separation."""

import unittest

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from rail_bed_patches import contact_patches, cut_to_patches
from upper_rail_contacts import UpperContact


class UpperRailChecks(unittest.TestCase):
    def bed(self, z=None):
        xy = np.array([[0, 0], [10, 0], [10, 4], [0, 4]], float)
        heights = np.zeros(4) if z is None else np.asarray(z, float)
        return Mesh(np.c_[xy, heights], [[0, 1, 2], [0, 2, 3]])

    def test_remove_only_interior_coplanar_edges(self):
        mesh = self.bed([1, 2, 2.8, 1.8])
        patches, note = contact_patches(mesh)
        self.assertEqual(len(patches), 1)
        self.assertEqual(patches[0]["faces"], [0, 1])
        self.assertLess(note["maximum_plane_error_m"], 1e-12)
        self.assertAlmostEqual(patches[0]["domain"].area, 40)
        triangles, _ = contact_patches(mesh, method="triangle")
        self.assertEqual(len(triangles), 2)

    def test_finite_crease_remains_split(self):
        patches, _ = contact_patches(self.bed([0, 0, 0, 0.1]))
        self.assertEqual(len(patches), 2)

    def test_no_transitive_plane_drift(self):
        xy = np.array(
            [[0, 0], [1, 0], [2, 0], [3, 0], [0, 1], [1, 1], [2, 1], [3, 1]], float
        )
        z = np.array([0, 0, 0.75e-8, 2.25e-8] * 2)
        mesh = Mesh(
            np.c_[xy, z],
            [[0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6]],
        )
        patches, _ = contact_patches(mesh)
        self.assertGreater(len(patches), 1)
        for patch in patches:
            p = mesh.vertices[mesh.triangles[patch["plane_face"]]]
            g = np.linalg.solve(p[1:, :2] - p[0, :2], p[1:, 2] - p[0, 2])
            q = mesh.vertices[mesh.triangles[patch["faces"]]]
            self.assertLessEqual(
                np.max(abs(q[..., 2] - p[0, 2] - (q[..., :2] - p[0, :2]) @ g)), 1e-8
            )

    def test_same_plane_separate_islands_remain_separate(self):
        m = self.bed()
        mesh = Mesh(
            np.vstack([m.vertices, m.vertices + [20, 0, 0]]),
            np.vstack([m.triangles, m.triangles + 4]),
        )
        patches, _ = contact_patches(mesh)
        self.assertEqual(len(patches), 2)

    def test_whole_inset_not_centreline_only(self):
        mesh = self.bed()
        patches, _ = contact_patches(mesh)
        piece = {
            "xy": np.array([[-1, 2], [11, 2]]),
            "chain": 0,
            "sources": ["original"],
        }
        parts, ledger, _ = cut_to_patches([piece], patches)
        self.assertEqual(len(parts), 1)
        self.assertAlmostEqual(ledger[0]["candidate_length_m"], 7.3)
        self.assertTrue(
            shapely.box(0, 0, 10, 4).covers(
                shapely.LineString(parts[0]["xy"]).buffer(1.3)
            )
        )

    def test_interior_old_body_peak_is_rejected(self):
        mesh = self.bed()
        peak = Mesh([[4, 1, 0], [6, 1, 0], [5, 3, 1]], [[0, 1, 2]])
        contact = UpperContact(mesh, peak)
        with self.assertRaisesRegex(ValueError, "not certified below"):
            contact.verify(shapely.box(4, 1, 6, 3), mesh.vertices[mesh.triangles[0]])

    def test_vertical_old_wall_is_not_dropped(self):
        mesh = self.bed()
        wall = Mesh([[5, 1, 0], [5, 3, 0], [5, 2, 1]], [[0, 1, 2]])
        contact = UpperContact(mesh, wall)
        with self.assertRaisesRegex(ValueError, "not certified below"):
            contact.verify(shapely.box(4, 1, 6, 3), mesh.vertices[mesh.triangles[0]])

    def test_old_contact_at_or_below_support_is_allowed(self):
        mesh = self.bed()
        contact = UpperContact(mesh, mesh)
        proof = contact.verify(
            shapely.box(2, 1, 8, 3), mesh.vertices[mesh.triangles[0]]
        )
        self.assertEqual(proof["maximum_baseline_above_bed_m"], 0)

    def test_reverse_orientation_and_invalid_budget_rejected(self):
        m = self.bed()
        with self.assertRaises(ValueError):
            contact_patches(Mesh(m.vertices, m.triangles[:, ::-1]))
        with self.assertRaises(ValueError):
            contact_patches(m, method="smooth")


if __name__ == "__main__":
    unittest.main()
