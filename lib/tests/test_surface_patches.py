"""Exact finite replacement, stable local identities and bounded spatial extraction."""

import unittest

import numpy as np
from outshine_ref.contracts import LocalFrame, Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches

FRAME = LocalFrame("test-ENU-up-zero-m")


def patch(
    name, xy=((0, 0), (2, 0), (2, 2), (0, 2)), *, frame=FRAME, kind="terrain", z=None
):
    xyz = np.c_[xy, np.zeros(len(xy)) if z is None else z]
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    return SurfacePatch(
        name,
        frame,
        kind,
        Mesh(xyz, faces),
        {"source": np.full(2, int(name[-1]) if name[-1].isdigit() else 0)},
    )


def refined(name, *, border_shift=0, kind="terrain", frame=FRAME):
    xy = np.array([[0, 0], [2, 0], [2, 2], [0, 2], [1, 1]], float)
    xyz = np.c_[xy, [border_shift, 0, 0, 0, 0.5]]
    faces = np.array([[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]])
    return SurfacePatch(name, frame, kind, Mesh(xyz, faces), {"source": np.full(4, 2)})


class ExactSurfacePatches(unittest.TestCase):
    def test_a_real_hole_cannot_be_filled_by_replacement(self):
        xy = np.array([[0, 0], [4, 0], [4, 4], [0, 4], [1, 1], [3, 1], [3, 3], [1, 3]])
        faces = np.array(
            [
                [0, 1, 5],
                [0, 5, 4],
                [1, 2, 6],
                [1, 6, 5],
                [2, 3, 7],
                [2, 7, 6],
                [3, 0, 4],
                [3, 4, 7],
            ]
        )
        root = SurfacePatch(
            "hole1",
            FRAME,
            "terrain",
            Mesh(np.c_[xy, np.zeros(8)], faces),
            {"source": np.zeros(8, int)},
        )
        view = SurfacePatches(root)
        self.assertEqual(view.query([1.5, 1.5, 2.5, 2.5]).reason, "outside_surface")
        solid = patch("solid2", xy=xy[:4])
        with self.assertRaises(ValueError):
            view.replace(solid, {"hole1": list(range(8))})

    def test_more_than_64_distinct_patch_and_source_labels_need_no_global_bitmask(self):
        root = patch("root0")
        view = SurfacePatches(root)
        previous = root.patch_id
        for source in range(1, 131):
            new = SurfacePatch(
                "asset-" + str(source),
                FRAME,
                "terrain",
                root.mesh,
                {"source": np.full(2, source)},
            )
            view, _note = view.replace(new, {previous: [0, 1]})
            previous = new.patch_id
        result = view.query([-1, -1, 3, 3])
        self.assertEqual(len(view.patches), 131)
        np.testing.assert_array_equal(result.face_data["source"], [130, 130])
        self.assertEqual(result.face_keys, ((previous, 0), (previous, 1)))
        self.assertTrue(all(p.mesh is root.mesh for p in view.patches))

    def test_local_replacement_shares_old_geometry_and_old_snapshots(self):
        original = patch("root1")
        first = SurfacePatches(original)
        second, note = first.replace(refined("new2"), {"root1": [0, 1]})
        self.assertIs(first.patches[0], second.patches[0])
        self.assertIs(first.patches[0].mesh, second.patches[0].mesh)
        self.assertTrue(first.active_faces["root1"].all())
        self.assertFalse(second.active_faces["root1"].any())
        self.assertEqual(note["retained_mesh_copy_bytes"], 0)
        self.assertEqual(note["exact_boundary_edges"], 4)
        with self.assertRaises(ValueError):
            second.active_faces["root1"].setflags(write=True)
        query = second.query([-1, -1, 3, 3])
        result = SurfaceSampler(query.mesh).sample([[1, 1], [0, 0], [1, 0]])
        np.testing.assert_array_equal(result.points_enu_m[:, 2], [0.5, 0, 0])
        self.assertEqual(result.reasons, ("", "", ""))

    def test_changed_boundary_frame_kind_and_stale_or_duplicate_faces_rejected(self):
        first = SurfacePatches(patch("root1"))
        for new in (
            refined("new2", border_shift=1e-10),
            refined("new2", kind="deck"),
            refined("new2", frame=LocalFrame("other")),
        ):
            with self.assertRaises(ValueError):
                first.replace(new, {"root1": [0, 1]})
        for removed in ({"root1": [0, 0]}, {"unknown": [0]}, {"root1": [99]}, {}):
            with self.assertRaises(ValueError):
                first.replace(refined("new2"), removed)
        second, _ = first.replace(refined("new2"), {"root1": [0, 1]})
        with self.assertRaises(ValueError):
            second.replace(refined("new3"), {"root1": [0, 1]})
        with self.assertRaises(ValueError):
            second.replace(refined("new2"), {"new2": [0, 1, 2, 3]})
        self.assertTrue(first.active_faces["root1"].all())

    def test_partial_regions_cannot_overwrite_unselected_neighbours(self):
        first = SurfacePatches(patch("root1"))
        with self.assertRaises(ValueError):
            first.replace(refined("new2"), {"root1": [0]})

    def test_cross_patch_seams_canonicalise_exact_vertices_for_sampling(self):
        first = SurfacePatches(patch("root1"))
        tri = Mesh(
            np.array([[0, 0, 0], [2, 0, 0], [2, 2, 0], [1.5, 0.5, 0.1]]),
            np.array([[0, 1, 3], [1, 2, 3], [2, 0, 3]]),
        )
        second, _ = first.replace(
            SurfacePatch("new2", FRAME, "terrain", tri, {"source": np.full(3, 2)}),
            {"root1": [0]},
        )
        a = second.query([-1, -1, 3, 3])
        b = second.query([-1, -1, 3, 3], method="scan")
        self.assertEqual(a.face_keys, b.face_keys)
        np.testing.assert_array_equal(a.mesh.vertices, b.mesh.vertices)
        np.testing.assert_array_equal(a.mesh.triangles, b.mesh.triangles)
        self.assertEqual(SurfaceSampler(a.mesh).sample([[1, 1]]).reasons, ("",))
        third, note = second.replace(
            refined("final3"), {"root1": [1], "new2": [0, 1, 2]}
        )
        self.assertEqual(note["exact_boundary_edges"], 4)
        self.assertEqual(len(third.query([-1, -1, 3, 3]).face_keys), 4)

    def test_budget_exhaustion_is_explicit_and_produces_no_partial_surface(self):
        first = SurfacePatches(patch("root1"), max_patches=1)
        with self.assertRaises(ValueError):
            first.replace(refined("new2"), {"root1": [0, 1]})
        first = SurfacePatches(patch("root1"))
        for kwargs in ({"max_removed": 1}, {"max_new_faces": 2}):
            with self.assertRaises(ValueError):
                first.replace(refined("new2"), {"root1": [0, 1]}, **kwargs)
        for kwargs, reason in (
            ({"max_faces": 1}, "local_face_budget_exhausted"),
            ({"max_candidates": 1}, "candidate_budget_exhausted"),
        ):
            result = first.query([-1, -1, 3, 3], **kwargs)
            self.assertIsNone(result.mesh)
            self.assertEqual(result.reason, reason)
        self.assertEqual(first.query([10, 10, 11, 11]).reason, "outside_surface")

    def test_attribute_inputs_are_owned_and_schemas_cannot_change(self):
        values = np.array([1, 2])
        base = patch("root1")
        root = SurfacePatch("root2", FRAME, "terrain", base.mesh, {"owner": values})
        values[:] = 99
        np.testing.assert_array_equal(root.face_data["owner"], [1, 2])
        with self.assertRaises(ValueError):
            root.face_data["owner"].setflags(write=True)
        with self.assertRaises(ValueError):
            SurfacePatches(root).replace(refined("new3"), {"root2": [0, 1]})

    def test_height_jump_overlap_and_wrong_orientation_are_not_initial_surfaces(self):
        base = patch("root1")
        for mesh in (
            Mesh(base.mesh.vertices, base.mesh.triangles[:, ::-1]),
            Mesh(
                base.mesh.vertices,
                np.vstack([base.mesh.triangles, base.mesh.triangles]),
            ),
        ):
            with self.assertRaises(ValueError):
                SurfacePatch("bad", FRAME, "terrain", mesh)
        vertices = np.r_[base.mesh.vertices, base.mesh.vertices + [0, 0, 0.1]]
        with self.assertRaises(ValueError):
            SurfacePatch(
                "bad",
                FRAME,
                "terrain",
                Mesh(
                    vertices,
                    np.r_[base.mesh.triangles[:1], base.mesh.triangles[1:] + 4],
                ),
            )


if __name__ == "__main__":
    unittest.main()
