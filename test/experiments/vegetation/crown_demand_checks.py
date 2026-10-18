"""Demand before work, exact source poses and independent scalar lens controls."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
from connected_leaf import build_connected
from crown_demand import CrownDemand, host_bounds, projected_spans
from crown_draw_batches import batch_geometry
from demand import Camera
from spatial_group_source import SpatialGroupSource
from spatial_source_checks import fixture


class CrownDemandChecks(unittest.TestCase):
    def test_diagnostic_batches_keep_independent_original_transforms_and_indices(self):
        source = fixture()
        product = build_connected(source.blades[0])["mesh"]
        group = source.crown.group(int(source.crown.hosts[0]), 128)
        result = batch_geometry(product, group["origins"], group["frames"])
        v, f = len(product["vertices"]), len(product["faces"])
        for i, frame in enumerate(group["frames"]):
            np.testing.assert_allclose(
                result["vertices"][i * v : (i + 1) * v],
                product["vertices"] @ frame.T + group["origins"][i],
                rtol=0,
                atol=2e-15,
            )
            np.testing.assert_array_equal(
                result["faces"][i * f : (i + 1) * f], product["faces"] + i * v
            )
            np.testing.assert_array_equal(
                result["uv"][i * v : (i + 1) * v], product["uv"]
            )

    def test_zero_and_negative_bends_keep_actual_source_vertices_inside(self):
        original = fixture()
        for bend in (-1.0, 0.0, 1.0):
            source = SpatialGroupSource(
                replace(original.crown, bend_rad=bend), original.blades
            )
            low, high = host_bounds(source)
            for i, node in enumerate(source.crown.hosts):
                mesh = source.geometry(int(node))
                world = (
                    mesh["vertices"] @ mesh["anchor_frame"].T + mesh["anchor_origin"]
                )
                self.assertTrue(np.all(world >= low[i]))
                self.assertTrue(np.all(world <= high[i]))

    def test_empty_camera_has_no_pose_work_even_with_zero_capacity(self):
        source = fixture()
        plan = CrownDemand(source)
        with patch(
            "crown_shoots.CrownShoots.group",
            side_effect=AssertionError("unneeded poses"),
        ):
            query, poses = plan.prepare(
                (Camera((100, 0, 1), (101, 0, 1)),), max_leaves=0
            )
        self.assertTrue(query["accepted"])
        self.assertEqual(query["requested_leaf_poses"], 0)
        self.assertEqual(poses["frames"].shape, (0, 3, 3))

    def test_envelopes_need_no_source_poses_or_meshes(self):
        source = fixture()
        with (
            patch(
                "outshine_ref.vegetation.Shoot.detail",
                side_effect=AssertionError("poses"),
            ),
            patch(
                "outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError("mesh")
            ),
        ):
            root = host_bounds(source, method="root")
            arc = host_bounds(source)
        self.assertFalse(source.crown._groups)
        self.assertTrue(np.all(arc[0] >= root[0]))
        self.assertTrue(np.all(arc[1] <= root[1]))

    def test_vector_projection_matches_existing_scalar_and_near_plane(self):
        low, high = host_bounds(fixture())
        for eye in ((2, -3, 1), (0, 0, 0.5)):
            camera = Camera(eye, (0, 1, 1))
            expected = np.array(
                [camera.projected_span(a, b) for a, b in zip(low, high, strict=True)]
            )
            np.testing.assert_allclose(
                projected_spans(camera, low, high), expected, rtol=1e-14, atol=1e-11
            )

    def test_refusal_is_before_poses_and_multiple_views_are_a_union(self):
        source = fixture()
        plan = CrownDemand(source)
        views = (Camera((0, -2, 1), (0, 0, 1)), Camera((0, 2, 1), (0, 0, 1)))
        with patch(
            "crown_shoots.CrownShoots.group", side_effect=AssertionError("poses")
        ):
            query, poses = plan.prepare(views, max_leaves=0)
        self.assertIsNone(poses)
        np.testing.assert_array_equal(
            query["visible"],
            plan.query(views[:1], max_leaves=10000)["visible"]
            | plan.query(views[1:], max_leaves=10000)["visible"],
        )

    def test_demanded_ids_and_poses_are_exact_without_whole_tables(self):
        source = fixture()
        plan = CrownDemand(source)
        view = (Camera((0, -2, 1), (0, 0, 1)),)
        query = plan.query(view, max_leaves=10000)
        expected = {
            int(n): dict(source.crown.group(int(n), 10000))
            for n in source.crown.hosts[query["detail"]]
        }
        with patch(
            "crown_shoots.CrownShoots.tables", side_effect=AssertionError("whole table")
        ):
            _, poses = plan.prepare(view, max_leaves=10000)
        for key in poses:
            np.testing.assert_array_equal(
                poses[key], np.concatenate([g[key] for g in expected.values()])
            )
        self.assertLessEqual(len(source.crown._groups), 1)


if __name__ == "__main__":
    unittest.main()
