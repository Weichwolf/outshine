"""Independent whole-contact and source-preserving support-placement controls."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface, contact_prism, replay_contact
from bridge_support_plan import plan_supports, stations
from outshine_ref.contracts import Mesh
from surface_solid_checks import audit_closed
from wien_support_experiment import packed
from wien_support_verify import caps_match_body, unpack


def grid(height):
    xy = np.array([[x, y] for y in (-2.0, 0, 2) for x in (-2.0, 0, 2)])
    first = np.array([0, 1, 3, 4])
    faces = np.vstack(
        (np.c_[first, first + 1, first + 4], np.c_[first, first + 4, first + 3])
    )
    return Mesh(np.c_[xy, height(xy)], faces)


class WholeSupportContacts(unittest.TestCase):
    def test_real_vienna_shared_boundary_is_noded_once(self):
        row = json.loads(
            (
                Path(__file__).parent / "fixtures/bridge-support-contact-vienna.json"
            ).read_text()
        )
        lower = ContactSurface(
            Mesh(np.array(row["lower_vertices"]), np.array(row["lower_faces"]))
        )
        upper = ContactSurface(
            Mesh(np.array(row["upper_vertices"]), np.array(row["upper_faces"]))
        )
        body, contacts, note = contact_prism(
            shapely.Polygon(row["footprint"]), lower, upper
        )
        self.assertGreater(audit_closed(body), 0)
        self.assertLess(note["coverage_error_m2"], 1e-8)
        self.assertEqual(note["conformance"]["added_triangles"], 0)
        self.assertLess(replay_contact(contacts[0], lower), 1e-7)
        self.assertLess(replay_contact(contacts[1], upper), 1e-7)

    def test_crossing_creases_have_exact_whole_contacts_and_volume(self):
        lower = ContactSurface(grid(lambda p: abs(p[:, 0])))
        upper = ContactSurface(grid(lambda p: 6 + 0.3 * abs(p[:, 1])))
        footprint = shapely.box(-1, -1, 1, 1)
        body, contacts, note = contact_prism(footprint, lower, upper)
        lo, hi = contacts
        np.testing.assert_allclose(
            lo.vertices[:, 2], abs(lo.vertices[:, 0]), atol=1e-12
        )
        np.testing.assert_allclose(
            hi.vertices[:, 2], 6 + 0.3 * abs(hi.vertices[:, 1]), atol=1e-12
        )
        self.assertAlmostEqual(audit_closed(body), 4 * (6 + 0.15 - 0.5))
        self.assertGreater(note["contact_faces"], 2)
        self.assertLess(replay_contact(lo, lower), 1e-12)
        self.assertLess(replay_contact(hi, upper), 1e-12)

    def test_interior_ground_peak_is_not_missed_by_four_corners(self):
        lower = ContactSurface(grid(lambda p: 8 - 4 * abs(p[:, 0])))
        upper = ContactSurface(grid(lambda p: np.full(len(p), 5.0)))
        with self.assertRaisesRegex(ValueError, "insufficient whole contact height"):
            contact_prism(shapely.box(-1, -1, 1, 1), lower, upper)

    def test_finite_contact_is_not_extrapolated(self):
        lower = ContactSurface(grid(lambda p: np.zeros(len(p))))
        upper = ContactSurface(grid(lambda p: np.full(len(p), 5.0)))
        with self.assertRaisesRegex(ValueError, "outside selected contact"):
            contact_prism(shapely.box(-1, -1, 2.01, 1), lower, upper)

    def test_hole_keeps_an_oriented_inner_wall(self):
        lower = ContactSurface(grid(lambda p: np.zeros(len(p))))
        upper = ContactSurface(grid(lambda p: np.full(len(p), 5.0)))
        footprint = shapely.box(-1, -1, 1, 1).difference(
            shapely.box(-0.2, -0.2, 0.2, 0.2)
        )
        body, contacts, _note = contact_prism(footprint, lower, upper)
        self.assertAlmostEqual(audit_closed(body), footprint.area * 5)
        self.assertLess(replay_contact(contacts[0], lower), 1e-12)

    def test_replay_rejects_wrong_interior_contact(self):
        lower = ContactSurface(grid(lambda p: np.zeros(len(p))))
        upper = ContactSurface(grid(lambda p: np.full(len(p), 5.0)))
        _body, contacts, _note = contact_prism(shapely.box(-1, -1, 1, 1), lower, upper)
        wrong = Mesh(contacts[1].vertices + [0, 0, 0.001], contacts[1].triangles)
        with self.assertRaisesRegex(ValueError, "whole contact differs"):
            replay_contact(wrong, upper)


class SupportStations(unittest.TestCase):
    def test_shift_preserves_source_and_avoids_an_actual_crossing(self):
        lower_mesh = grid(lambda p: np.zeros(len(p)))
        lower_mesh = Mesh(lower_mesh.vertices * [100, 100, 1], lower_mesh.triangles)
        lower = ContactSurface(lower_mesh)
        upper = ContactSurface(
            Mesh(lower_mesh.vertices + [0, 0, 6], lower_mesh.triangles)
        )
        candidates = stations(
            [{"source_id": "original-bridge", "xy": [[0, 0], [70, 0]]}]
        )
        crossing = shapely.box(11, -10, 13, 10)
        fixed = plan_supports(candidates, lower, upper, crossing, method="fixed")
        shifted = plan_supports(candidates, lower, upper, crossing, method="shifted")
        self.assertEqual(len(fixed[0]), 2)
        self.assertEqual(len(shifted[0]), 3)
        self.assertTrue(
            all(row["source_id"] == "original-bridge" for row in shifted[3]["stations"])
        )
        self.assertTrue(all(not shape.intersects(crossing) for shape in shifted[2]))
        self.assertEqual(shifted[3]["stations"][0]["shift_m"], -3)

    def test_whole_axis_has_an_explicit_time_budget_omission(self):
        contact = ContactSurface(grid(lambda p: np.zeros(len(p))))
        candidates = stations(
            [{"source_id": "original-bridge", "xy": [[0, 0], [70, 0]]}]
        )
        result = plan_supports(
            candidates,
            contact,
            contact,
            shapely.Polygon(),
            method="shifted",
            seconds=1e-20,
        )
        self.assertFalse(result[0])
        self.assertEqual(len(result[3]["stations"]), 3)
        self.assertTrue(
            all(
                row["reason"] == "support_time_or_trial_budget"
                for row in result[3]["stations"]
            )
        )


class SavedSupportProducts(unittest.TestCase):
    def setUp(self):
        self.lower = ContactSurface(grid(lambda p: abs(p[:, 0])))
        self.upper = ContactSurface(grid(lambda p: 6 + 0.3 * abs(p[:, 1])))
        self.body, self.caps, _note = contact_prism(
            shapely.box(-1, -1, 1, 1), self.lower, self.upper
        )

    def test_variable_sized_owned_products_replay_after_serialization(self):
        other, caps, _note = contact_prism(
            shapely.box(0.2, 0.2, 1, 1), self.lower, self.upper
        )
        arrays = packed([self.body, other], [self.caps, caps])
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "piers.npz"
            np.savez_compressed(path, **arrays)
            bodies, pairs, _arrays = unpack(path)
        for original, body, pair in zip([self.body, other], bodies, pairs, strict=True):
            np.testing.assert_array_equal(original.vertices, body.vertices)
            np.testing.assert_array_equal(original.triangles, body.triangles)
            caps_match_body(body, *pair)

    def test_replay_rejects_a_body_that_differs_from_its_valid_contact_caps(self):
        vertices = self.body.vertices.copy()
        vertices[0, 2] += 0.01
        wrong = Mesh(vertices, self.body.triangles)
        with self.assertRaisesRegex(ValueError, "body differs"):
            caps_match_body(wrong, *self.caps)

    def test_replay_rejects_missing_side_geometry(self):
        wrong = Mesh(self.body.vertices, self.body.triangles[:-1])
        with self.assertRaisesRegex(ValueError, "missing walls"):
            caps_match_body(wrong, *self.caps)


if __name__ == "__main__":
    unittest.main()
