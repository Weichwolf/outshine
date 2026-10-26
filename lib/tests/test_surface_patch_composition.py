"""Real public replacement preserves holes, touching islands and thin domains."""

import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.contracts import LocalFrame, Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches, _field


def part(name, xy, faces):
    mesh = Mesh(np.c_[xy, np.zeros(len(xy))], np.asarray(faces, dtype=np.uint32))
    return SurfacePatch(
        name,
        LocalFrame("composition-ENU"),
        "terrain",
        mesh,
        {"ground": np.ones(len(faces), dtype=bool)},
    )


def refined(root):
    mesh = root.mesh
    centres = mesh.vertices[mesh.triangles].mean(axis=1)
    vertices = np.vstack([mesh.vertices, centres])
    faces = []
    for i, (a, b, c) in enumerate(mesh.triangles):
        centre = len(mesh.vertices) + i
        faces.extend([[a, b, centre], [b, c, centre], [c, a, centre]])
    return SurfacePatch(
        "refined",
        root.frame,
        root.kind,
        Mesh(vertices, np.asarray(faces)),
        {"ground": np.ones(len(faces), dtype=bool)},
    )


def ring():
    xy = np.array([[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]])
    faces = [
        [0, 1, 5],
        [0, 5, 4],
        [1, 2, 6],
        [1, 6, 5],
        [2, 3, 7],
        [2, 7, 6],
        [3, 0, 4],
        [3, 4, 7],
    ]
    return part("original", xy, faces)


class CoverageComposition(unittest.TestCase):
    def replace_and_compare(self, root, points):
        original = SurfacePatches(root)
        before = original.active_faces[root.patch_id].copy()
        replacement = refined(root)
        with patch.object(
            _field,
            "_same_coverage",
            lambda old, new: (
                shapely.union_all(old).symmetric_difference(shapely.union_all(new)).area
                <= 1e-7
            ),
        ):
            legacy, _ = original.replace(
                replacement, {root.patch_id: list(range(len(root.mesh.triangles)))}
            )
        result, note = original.replace(
            replacement, {root.patch_id: list(range(len(root.mesh.triangles)))}
        )
        self.assertTrue(original.active_faces[root.patch_id].all())
        np.testing.assert_array_equal(before, original.active_faces[root.patch_id])
        self.assertFalse(result.active_faces[root.patch_id].any())
        self.assertTrue(result.active_faces[replacement.patch_id].all())
        self.assertIs(result.patches[0].mesh, root.mesh)
        self.assertEqual(note["retained_mesh_copy_bytes"], 0)
        actual = result.query(root.bounds_xy_m, max_faces=65536)
        old = legacy.query(root.bounds_xy_m, max_faces=65536)
        np.testing.assert_array_equal(actual.mesh.vertices, old.mesh.vertices)
        np.testing.assert_array_equal(actual.mesh.triangles, old.mesh.triangles)
        expected = SurfaceSampler(old.mesh).sample(points)
        sampled = SurfaceSampler(actual.mesh).sample(points)
        self.assertEqual(sampled.reasons, expected.reasons)
        np.testing.assert_array_equal(sampled.points_enu_m, expected.points_enu_m)
        np.testing.assert_array_equal(sampled.normals_enu, expected.normals_enu)
        return result

    def test_hole_is_preserved_by_a_different_valid_interior_mesh(self):
        result = self.replace_and_compare(ring(), [[0.5, 0.5], [3.5, 3.5]])
        self.assertEqual(result.query([1.1, 1.1, 2.9, 2.9]).reason, "outside_surface")

    def test_point_touching_components_remain_separate_valid_regions(self):
        xy = np.array([[0, 0], [1, 0], [1, 1], [0, 1], [1, 1], [2, 1], [2, 2], [1, 2]])
        root = part("original", xy, [[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]])
        result = self.replace_and_compare(root, [[0.25, 0.25], [1.75, 1.75]])
        self.assertEqual(result.query([0.1, 1.1, 0.9, 1.9]).reason, "outside_surface")

    def test_disconnected_components_keep_their_uncovered_gap(self):
        xy = np.array([[0, 0], [1, 0], [1, 1], [0, 1], [3, 0], [4, 0], [4, 1], [3, 1]])
        root = part("original", xy, [[0, 1, 2], [0, 2, 3], [4, 5, 6], [4, 6, 7]])
        result = self.replace_and_compare(root, [[0.25, 0.25], [3.75, 0.75]])
        self.assertEqual(result.query([1.1, 0.1, 2.9, 0.9]).reason, "outside_surface")

    def test_thin_valid_domain_keeps_its_exact_outer_edges(self):
        height = 2**-40
        xy = np.array([[0, 0], [3, 0], [3, height], [0, height]])
        root = part("original", xy, [[0, 1, 2], [0, 2, 3]])
        self.replace_and_compare(root, [[0.75, height / 2], [2.25, height / 2]])

    def test_a_valid_new_hole_cannot_remove_old_active_coverage(self):
        root = ring()
        candidate = part(
            "hole-changed", root.mesh.vertices[:, :2], root.mesh.triangles[1:]
        )
        view = SurfacePatches(root)
        with self.assertRaises(ValueError):
            view.replace(candidate, {root.patch_id: list(range(8))})
        self.assertTrue(view.active_faces[root.patch_id].all())


if __name__ == "__main__":
    unittest.main()
