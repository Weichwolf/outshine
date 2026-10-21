"""Attribute-bit identity, complete groups and an independent disocclusion case."""

import unittest
from pathlib import Path

import numpy as np
from full_view_reference import load, views
from motion_surface_capture import (
    decoded,
    front_layers,
    intrinsic_cell_box,
    select_groups,
    select_intrinsic_cell,
    shared_mesh,
    source_groups,
)
from native_capture_meshes import independent_depth
from triangle_image_reference import intersect_source, prepare_source


def fixture():
    triangles = np.array(
        [
            [[-1, -1, 1], [0, 1, 1], [1, -1, 1]],
            [[-1, -1, 2], [0, 1, 2], [1, -1, 2]],
            [[-10, -10, 3], [0, 10, 3], [10, -10, 3]],
        ],
        np.float32,
    )
    return {
        "positions": triangles.reshape(-1, 3),
        "indices": np.arange(9).reshape(-1, 3),
        "uv": np.zeros((9, 2), np.float32),
        "normal": np.full(9, 0x80008000, np.uint32),
        "colour": np.ones((9, 4), np.float32),
        "metres": np.zeros((9, 2), np.float32),
        "binding": np.repeat(np.arange(3, dtype=np.int32)[:, None], 6, axis=1),
    }


class MotionCaptureTests(unittest.TestCase):
    def test_intrinsic_frame_contains_geometry_and_handles_both_poles(self):
        points = fixture()["positions"]
        for anchor in (
            [0.0, 0.0, 1.0],
            [0.0, 0.0, -1.0],
            [1.0, 2.0, 3.0],
            [1e-300, 0.0, 1.0],
        ):
            axes, bounds = intrinsic_cell_box(points, anchor)
            np.testing.assert_allclose(axes.T @ axes, np.eye(3), atol=1e-15, rtol=0)
            self.assertGreater(np.linalg.det(axes), 0.99999999999999)
            local = points @ axes
            self.assertTrue((local >= bounds[0]).all() and (local <= bounds[1]).all())
        with self.assertRaises(ValueError):
            intrinsic_cell_box(points, [0.0, 0.0, 0.0])
        with self.assertRaises(ValueError):
            intrinsic_cell_box(np.full((1, 3), 1e300), [1.0, 0.0, 0.0])

    def test_one_intrinsic_bound_matches_every_original_and_side_pose_group_cut(self):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/render/building-full-view/cell.json.gz"
        )
        source, document, context, _receipt = load(path)
        poses = views(document, context)
        boxes, _groups = source_groups(source)
        axes, bounds = intrinsic_cell_box(source["positions"], document["anchor_ecef"])
        original = axes.tobytes(), bounds.tobytes()
        eye, _basis, *projection = poses["static-001"]
        up = np.asarray(document["anchor_ecef"], float)
        up /= np.linalg.norm(up)
        forward = (source["positions"].min(0) + source["positions"].max(0)) / 2 - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, up)
        right /= np.linalg.norm(right)
        basis = np.array([right, np.cross(right, forward), forward])
        for delta in (-30.0, 6.0, 30.0):
            poses[f"side{delta}"] = eye + delta * right, basis, *projection
        for view in poses.values():
            self.assertEqual(
                select_intrinsic_cell(axes, bounds, view),
                bool(len(select_groups(boxes, view))),
            )
        self.assertEqual(original, (axes.tobytes(), bounds.tobytes()))

    def test_complete_archived_native_cell_replays_every_corner_and_all_source_groups(
        self,
    ):
        path = (
            Path(__file__).parents[2]
            / "outshine/data/render/building-full-view/cell.json.gz"
        )
        source, _document, _context, receipt = load(path)
        self.assertEqual(receipt["sources"], 576)
        self.assertEqual(receipt["triangles"], 14169)
        product = decoded(shared_mesh(source, np.arange(14169)))
        for key in ("positions", "uv", "normal", "colour", "metres"):
            self.assertEqual(
                source[key][source["indices"]].tobytes(),
                product[key][product["indices"]].tobytes(),
            )
        np.testing.assert_array_equal(source["binding"], product["binding"])
        np.testing.assert_array_equal(product["original_faces"], np.arange(14169))
        _boxes, groups = source_groups(source)
        np.testing.assert_array_equal(np.sort(np.concatenate(groups)), np.arange(14169))
        for group in groups:
            self.assertLessEqual(len(np.unique(source["binding"][group, 1])), 32)

    def test_all_native_fields_keep_exact_corner_bytes_and_sparse_constants(self):
        source = fixture()
        source["uv"][3] = [-0.0, 2.0]
        source["uv"][6] = [0.0, 2.0]
        mesh = shared_mesh(source, np.arange(3))
        self.assertEqual(mesh["indices"].dtype, np.uint16)
        self.assertEqual(mesh["colour"].nbytes, 0)
        self.assertEqual(mesh["metres"].nbytes, 0)
        out = decoded(mesh)
        for key in ("positions", "uv", "normal", "colour", "metres"):
            self.assertEqual(
                source[key][source["indices"]].tobytes(),
                out[key][out["indices"]].tobytes(),
            )
        np.testing.assert_array_equal(mesh["binding"], source["binding"])
        source["colour"][0, 3] = 0.5
        source["metres"][0, 0] = 3
        other = decoded(shared_mesh(source, np.arange(3)))
        for key in ("colour", "metres"):
            self.assertEqual(
                source[key][source["indices"]].tobytes(),
                other[key][other["indices"]].tobytes(),
            )

    def test_equal_positions_with_different_fields_are_not_welded(self):
        source = fixture()
        source["positions"][3:6] = source["positions"][:3]
        source["uv"][3:6] = [256, 0.25]
        mesh = shared_mesh(source, np.array([0, 1]))
        self.assertEqual(len(mesh["positions"]), 6)
        source["uv"][3:6] = source["uv"][:3]
        mesh = shared_mesh(source, np.array([0, 1]))
        self.assertEqual(len(mesh["positions"]), 3)

    def test_front_two_layers_do_not_admit_a_hidden_third_surface_after_side_motion(
        self,
    ):
        source = fixture()
        index = prepare_source(source)
        origins = np.zeros((1, 3))
        directions = np.array([[0.0, 0.0, 1.0]])
        faces, records, work = front_layers(source, index, origins, directions)
        np.testing.assert_array_equal(faces, [0, 1])
        np.testing.assert_array_equal(records[:, 3], [1.0, 2.0])
        self.assertEqual(work["overflow_rays"], 1)
        clipped = decoded(shared_mesh(source, faces))
        side = np.array([[2.0, 0.0, 0.0]])
        original, _ = intersect_source(source, side, directions, prepared=index)
        candidate, _ = intersect_source(clipped, side, directions)
        control = independent_depth(
            source["positions"][source["indices"]], side, directions
        )
        np.testing.assert_array_equal(original, control)
        self.assertEqual(original[0], 3.0)
        self.assertFalse(np.isfinite(candidate[0]))

    def test_empty_rays_exact_depth_ties_and_bounds(self):
        source = fixture()
        source["positions"][3:6] = source["positions"][:3]
        index = prepare_source(source)
        faces, records, work = front_layers(
            source, index, np.zeros((1, 3)), np.array([[0.0, 0.0, 1.0]])
        )
        np.testing.assert_array_equal(faces, [0, 2])
        np.testing.assert_array_equal(records[:, 3], [1.0, 3.0])
        self.assertEqual(work["overflow_rays"], 0)
        faces, records, _ = front_layers(
            source, index, np.array([[50.0, 0.0, 0.0]]), np.array([[0.0, 0.0, 1.0]])
        )
        self.assertFalse(len(faces))
        self.assertEqual(records.shape, (0, 4))
        for rays, directions in (
            (np.zeros((0, 3)), np.zeros((0, 3))),
            (np.zeros((1, 3)), np.zeros((1, 3))),
        ):
            with self.assertRaises(ValueError):
                front_layers(source, index, rays, directions)
        with self.assertRaises(ValueError):
            front_layers(
                dict(source), index, np.zeros((1, 3)), np.array([[0.0, 0.0, 1.0]])
            )

    def test_source_groups_are_complete_disjoint_and_frustum_retains_boundary(self):
        source = fixture()
        boxes, groups = source_groups(source, maximum_sources=2)
        np.testing.assert_array_equal(np.sort(np.concatenate(groups)), np.arange(3))
        for group in groups:
            self.assertLessEqual(len(np.unique(source["binding"][group, 1])), 2)
        view = (np.zeros(3), np.eye(3), 10.0, 20, 20)
        self.assertEqual(set(select_groups(boxes, view)), set(range(len(groups))))
        behind = boxes - np.array([0, 0, 100])
        self.assertFalse(len(select_groups(behind, view)))
        with self.assertRaises(ValueError):
            source_groups(source, maximum_sources=True)

    def test_invalid_selections_reject_before_materialization(self):
        source = fixture()
        for selected in (
            np.array([], int),
            np.array([0, 0]),
            np.array([-1]),
            np.array([3]),
            np.array([0.0]),
        ):
            with self.assertRaises(ValueError):
                shared_mesh(source, selected)


if __name__ == "__main__":
    unittest.main()
