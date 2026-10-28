"""Contained finite channels, bounded failure and preserved real terrace contacts."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from shapely.affinity import affine_transform
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_mass_admission import inspect
from terraced_route_mesh import CoreRouteMesh, RouteLimits

CORE = SimpleNamespace(origin_xy=(0.0, 0.0), across_xy=(1.0, 0.0), along_xy=(0.0, 1.0))


class RouteMeshTests(unittest.TestCase):
    def setUp(self):
        self.space = shapely.box(0, 0, 12, 12).difference(shapely.box(3, 3, 9, 9))

    def test_channel_avoids_hole_and_reuses_one_mesh(self):
        router = CoreRouteMesh(self.space, (1, 5), CORE)
        path = router.route((11, 5))
        self.assertIsNotNone(path)
        self.assertTrue(self.space.covers(path))
        self.assertGreater(path.length, 10)
        self.assertEqual(tuple(path.coords[0]), (1, 5))
        self.assertEqual(tuple(path.coords[-1]), (11, 5))
        self.assertEqual(router.work["mesh_builds"], 1)
        self.assertIsNotNone(router.route((11, 7)))
        self.assertEqual(router.work["mesh_builds"], 1)
        self.assertLessEqual(router.work["triangles"], router.limits.triangles)

    def test_direct_route_builds_no_triangulation(self):
        with patch(
            "terraced_route_mesh.shapely.constrained_delaunay_triangles",
            side_effect=AssertionError("unnecessary mesh"),
        ):
            router = CoreRouteMesh(self.space, (1, 1), CORE)
            self.assertIsNotNone(router.route((11, 1)))
            self.assertEqual(router.work["mesh_builds"], 0)

    def test_outside_and_coincident_anchors_are_local_failures(self):
        router = CoreRouteMesh(self.space, (1, 5), CORE)
        self.assertIsNone(router.route((6, 6)))
        self.assertIsNone(router.route((1, 5)))
        self.assertEqual(router.work["mesh_builds"], 0)
        self.assertIsNotNone(router.route((1, 6)))

    def test_vertex_budget_rejects_before_frame_copy_or_mesh(self):
        with patch(
            "terraced_route_mesh.affine_transform",
            side_effect=AssertionError("unneeded frame copy"),
        ):
            router = CoreRouteMesh(
                self.space, (1, 5), CORE, limits=RouteLimits(vertices=4)
            )
            self.assertIsNone(router.route((11, 5)))
            self.assertEqual(router.reason, "route_vertex_budget")

    def test_triangle_budget_is_not_rebuilt_for_each_target(self):
        router = CoreRouteMesh(
            self.space, (1, 5), CORE, limits=RouteLimits(triangles=1)
        )
        self.assertIsNone(router.route((11, 5)))
        self.assertIsNone(router.route((11, 7)))
        self.assertEqual(router.work["mesh_builds"], 1)
        self.assertIsNotNone(router.route((1, 6)))

    def test_path_budget_keeps_failure_explicit(self):
        router = CoreRouteMesh(
            self.space, (1, 5), CORE, limits=RouteLimits(path_points=2)
        )
        self.assertIsNone(router.route((11, 5)))
        self.assertEqual(router.reason, "route_path_point_budget")

    def test_disconnected_regions_do_not_get_an_invented_connection(self):
        space = shapely.MultiPolygon([shapely.box(0, 0, 2, 2), shapely.box(4, 0, 6, 2)])
        router = CoreRouteMesh(space, (1, 1), CORE)
        self.assertIsNone(router.route((5, 1)))
        self.assertEqual(router.reason, "route_disconnected_triangle_channel")

    def test_rigid_core_frame_keeps_actual_world_containment(self):
        angle = 0.37
        across, along = (np.cos(angle), np.sin(angle)), (-np.sin(angle), np.cos(angle))
        origin = (1000.0, -3000.0)
        core = SimpleNamespace(origin_xy=origin, across_xy=across, along_xy=along)
        world = affine_transform(
            self.space, [across[0], along[0], across[1], along[1], *origin]
        )
        root, target = np.asarray(origin) + np.asarray([(1, 5), (11, 5)]) @ np.array(
            [across, along]
        )
        path = CoreRouteMesh(world, root, core).route(target)
        self.assertIsNotNone(path)
        self.assertTrue(world.covers(path))
        np.testing.assert_allclose(
            np.asarray(path.coords)[[0, -1]], [root, target], rtol=0, atol=1e-9
        )


class ActualTerraceRouteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = [plan(row, kind) for row, kind in cases()]
        cls.owners = [
            RoofedProducts(p, terrace_routing="triangulated") for p in cls.original
        ]

    def test_all_previous_portals_have_priority_and_four_new_ones_are_real(self):
        for old, owner, count in zip(
            self.original, self.owners, (2, 3, 4), strict=True
        ):
            before = RoofedProducts(old).body.plan
            current = {p.identifier: p for p in owner.body.plan.portals}
            self.assertIs(owner.plan.body, old.body)
            self.assertEqual(len(current), count)
            for portal in before.portals:
                self.assertEqual(current[portal.identifier], portal)
        work = [dict(row[2]) for row in self.owners[-1].body.plan.route_work]
        self.assertEqual(sum(row["mesh_builds"] for row in work), 2)
        self.assertEqual(sum(row["mesh_reused"] for row in work), 2)
        self.assertEqual(
            sum(t.portal is None for t in self.owners[-1].body.plan.terraces), 2
        )

    def test_actual_new_portals_and_routes_pass_existing_finite_actor_checks(self):
        proof = inspect(self.owners[-1].body)
        self.assertEqual(len(proof["terraces"]), 4)
        self.assertGreater(proof["sampled_stair_poses"], 100)
        self.assertTrue(all(len(p["routes"]) == 2 for p in proof["terraces"]))


if __name__ == "__main__":
    unittest.main()
