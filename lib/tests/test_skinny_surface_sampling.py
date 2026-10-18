"""Actual thin shared-ground contact, finite shared segments and retained ambiguity."""

import json
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


def fixture():
    return json.loads(
        Path(__file__)
        .with_name("data")
        .joinpath("skinny-shared-ground-contact.json")
        .read_text()
    )


class SkinnyContacts(unittest.TestCase):
    def test_actual_shared_edge_remains_valid_when_another_edge_is_closer(self):
        row = fixture()
        mesh = Mesh(np.array(row["vertices_enu_m"]), np.array(row["triangles"]))
        for faces in (mesh.triangles, mesh.triangles[::-1], mesh.triangles[:, ::-1]):
            result = SurfaceSampler(
                Mesh(mesh.vertices, faces), xy_tolerance_m=row["xy_tolerance_m"]
            ).sample(row["query_xy_m"])
            self.assertEqual(result.reasons, ("",))
            self.assertEqual(result.face_indices.tolist(), [0])
            self.assertAlmostEqual(
                result.points_enu_m[0, 2], row["expected_height_m"], places=10
            )

    def test_unshared_geometrically_equal_vertices_do_not_resolve_overlap(self):
        row = fixture()
        original = np.array(row["vertices_enu_m"])[row["triangles"]]
        mesh = Mesh(original.reshape(-1, 3), np.arange(6).reshape(2, 3))
        result = SurfaceSampler(mesh, xy_tolerance_m=row["xy_tolerance_m"]).sample(
            row["query_xy_m"]
        )
        self.assertEqual(result.reasons, ("ambiguous_projected_contact",))

    def test_height_disagreement_and_budget_still_reject_actual_shared_edge(self):
        row = fixture()
        vertices = np.array(row["vertices_enu_m"])
        # Amplify the thin face's gradient while retaining shared source vertices.
        vertices[3, 2] += 1
        result = SurfaceSampler(
            Mesh(vertices, np.array(row["triangles"])),
            xy_tolerance_m=row["xy_tolerance_m"],
        ).sample(row["query_xy_m"])
        self.assertEqual(result.reasons, ("ambiguous_projected_contact",))
        mesh = Mesh(np.array(row["vertices_enu_m"]), np.array(row["triangles"]))
        result = SurfaceSampler(mesh, xy_tolerance_m=row["xy_tolerance_m"]).sample(
            row["query_xy_m"], max_candidates=1
        )
        self.assertEqual(result.reasons, ("candidate_budget_exhausted",))

    def test_near_infinite_extension_does_not_mean_near_shared_finite_edge(self):
        vertices = np.array(
            [[0, 0, 10.0], [1, 0, 10.0], [6, 1e-9, 10.0], [6, -1e-9, 10.0]]
        )
        mesh = Mesh(vertices, np.array([[0, 1, 2], [0, 1, 3]]))
        result = SurfaceSampler(mesh, xy_tolerance_m=1e-7).sample([[5, 0]])
        self.assertEqual(result.reasons, ("ambiguous_projected_contact",))


if __name__ == "__main__":
    unittest.main()
