"""Independent weighted normals, hard joins and unchanged geometry/attributes."""

import unittest
from unittest.mock import patch

import numpy as np
from fork_shading import contacts, shade
from join_normal_checks import check_weighting, scalar_angles


def fixture():
    vertices = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [-0.4, 0.2, 0]], float)
    faces = np.array([[0, 1, 2], [0, 2, 3]], np.int32)
    normals = np.array([[0.2, 0, 1], [-0.3, 0, 1]], float)
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": np.repeat(normals, 3, axis=0),
        "source_kind": np.array([0, 2], np.uint8),
        "source_node": np.array([40, 67], np.uint32),
    }


class ForkShading(unittest.TestCase):
    def test_scalar_angle_match_only_at_actual_source_contacts(self):
        original = fixture()
        mask = contacts(original)
        np.testing.assert_array_equal(np.flatnonzero(mask), [0, 2])
        result, work = shade(original)
        expected = scalar_angles(original)[original["faces"]].reshape(-1, 3)
        changed = mask[original["faces"].ravel()]
        np.testing.assert_allclose(
            result["normals"][changed], expected[changed], atol=1e-15
        )
        np.testing.assert_array_equal(
            result["normals"][~changed], original["normals"][~changed]
        )
        for name in original.keys() - {"normals"}:
            np.testing.assert_array_equal(result[name], original[name])
        self.assertEqual(work["unified_vertices"], 2)

    def test_same_vertex_gets_identical_normals_from_both_sources(self):
        mesh = fixture()
        result, _ = shade(mesh)
        for vertex in np.flatnonzero(contacts(mesh)):
            normals = result["normals"][mesh["faces"].ravel() == vertex]
            np.testing.assert_array_equal(
                normals, np.broadcast_to(normals[0], normals.shape)
            )

    def test_angle_limit_preserves_hard_boundary(self):
        original = fixture()
        a = np.array([np.sin(np.pi / 3), 0, 0.5])
        b = np.array([-np.sin(np.pi / 3), 0, 0.5])
        original["normals"] = np.repeat([a, b], 3, axis=0)
        result, work = shade(original)
        np.testing.assert_array_equal(result["normals"], original["normals"])
        self.assertEqual(work["rejected_local_vertices"], 2)

    def test_no_common_oriented_normal_is_refused_without_flipping(self):
        original = fixture()
        original["faces"][1] = original["faces"][1, ::-1]
        normals = np.array([[1, 0, 0.1], [1, 0, -0.2]], float)
        normals /= np.linalg.norm(normals, axis=1)[:, None]
        original["normals"] = np.repeat(normals, 3, axis=0)
        self.assertGreater(np.dot(*normals), np.cos(np.pi / 4))
        result, work = shade(original)
        np.testing.assert_array_equal(result["normals"], original["normals"])
        self.assertEqual(work["rejected_local_vertices"], 2)

    def test_no_contact_avoids_weighting_and_all_normal_copies(self):
        original = fixture()
        original["source_kind"][:] = 0
        with patch(
            "fork_shading.smooth_join_corners",
            side_effect=AssertionError("unnecessary work"),
        ):
            result, work = shade(original)
        self.assertIs(result, original)
        self.assertEqual(work["changed_corners"], 0)

    def test_existing_independent_split_and_hard_edge_controls(self):
        self.assertTrue(check_weighting()["split_invariant"])

    def test_invalid_ownership_and_weighting_are_refused(self):
        original = fixture()
        with self.assertRaises(ValueError):
            shade(original, "unknown")
        original["source_kind"] = np.zeros(2, float)
        with self.assertRaises(ValueError):
            contacts(original)


if __name__ == "__main__":
    unittest.main()
