import unittest

import numpy as np
from compare_stair_route_sections import construct_routes
from outshine_ref.contracts import Mesh
from stair_box_sweep import sweep_surfaces
from stair_box_witness import penetration_witness


def mesh(triangles):
    vertices = np.asarray(triangles, float).reshape(-1, 3)
    return Mesh(vertices, np.arange(len(vertices)).reshape(-1, 3))


class ContinuousBoxSweep(unittest.TestCase):
    def test_endpoint_poses_miss_the_wall_but_the_continuous_sweep_hits(self):
        obstacle = mesh([[[0, -1, 0], [0, 1, 0], [0, 0, 2]]])
        path, lower, upper = (
            [[-1, 0, 0], [1, 0, 0]],
            [-0.125, -0.125, 0],
            [0.125, 0.125, 1],
        )
        for point in path:
            events, _work = sweep_surfaces(obstacle, [[point]], lower, upper)
            self.assertEqual(events, [])
        events, _work = sweep_surfaces(obstacle, [path], lower, upper)
        self.assertEqual(events, [[0, 0, 0, "interior"]])
        witness = penetration_witness(obstacle.vertices, *path, lower, upper)
        self.assertTrue(witness["exact_interior_witness"])
        self.assertEqual(witness["reconstructed_box_margin_m"], 0.125)

    def test_floor_contact_and_one_ulp_gap_or_intersection_are_distinct(self):
        lower, upper = [-0.125, -0.125, 0], [0.125, 0.125, 1]
        for height, expected in (
            (0, "contact"),
            (np.nextafter(0.0, 1.0), "interior"),
            (np.nextafter(0.0, -1.0), None),
        ):
            floor = mesh([[[-2, -2, height], [2, -2, height], [0, 2, height]]])
            for method in ("scan", "bounds"):
                events, _work = sweep_surfaces(
                    floor, [[[0, 0, 0]]], lower, upper, method=method
                )
                self.assertEqual(
                    [event[3] for event in events],
                    [] if expected is None else [expected],
                )

    def test_swept_aabb_can_overlap_a_triangle_outside_the_swept_box(self):
        obstacle = mesh([[[0, 1, 0.0625], [0, 1.25, 0.0625], [0.25, 1.25, 0.125]]])
        events, work = sweep_surfaces(
            obstacle, [[[0, 0, 0], [1, 1, 0]]], [-0.125] * 3, [0.125] * 3
        )
        self.assertEqual(work["candidate_pairs"], 1)
        self.assertEqual(events, [])
        self.assertGreater(work["tested_axes"], 3)

    def test_reverse_and_axis_permutation_preserve_contact_classes(self):
        triangles = [
            [[0, -1, 0], [0, 1, 0], [0, 0, 2]],
            [[-2, -2, 0], [2, -2, 0], [0, 2, 0]],
        ]
        path, lower, upper = (
            np.array([[-1, 0, 0], [1, 0, 0]]),
            np.array([-0.125, -0.125, 0]),
            np.array([0.125, 0.125, 1]),
        )
        expected, _work = sweep_surfaces(mesh(triangles), [path], lower, upper)
        for order in ([0, 1, 2], [2, 0, 1]):
            offset = np.array([37, -81, 13])
            actual, _work = sweep_surfaces(
                mesh(np.array(triangles)[:, :, order] + offset),
                [path[::-1, order] + offset],
                lower[order],
                upper[order],
            )
            self.assertEqual(actual, expected)

    def test_exact_pair_budget_and_degenerate_input_do_not_return_partial_results(self):
        obstacle = mesh([[[0, -1, 0], [0, 1, 0], [0, 0, 2]]])
        for method in ("scan", "bounds"):
            with self.assertRaisesRegex(ValueError, "exact-pair budget"):
                sweep_surfaces(
                    obstacle,
                    [[[0, 0, 0]]],
                    [-1] * 3,
                    [1] * 3,
                    method=method,
                    max_pairs=0,
                )
        with self.assertRaisesRegex(ValueError, "nondegenerate"):
            sweep_surfaces(
                mesh([[[0, 0, 0], [1, 0, 0], [2, 0, 0]]]),
                [[[0, 0, 0]]],
                [-1] * 3,
                [1] * 3,
            )

    def test_no_surface_hit_does_not_claim_solid_containment_or_headroom_coverage(self):
        vertices = np.array(
            [[x, y, z] for x in (-2, 2) for y in (-2, 2) for z in (-2, 2)]
        )
        quads = [
            [0, 1, 3, 2],
            [4, 6, 7, 5],
            [0, 4, 5, 1],
            [2, 3, 7, 6],
            [0, 2, 6, 4],
            [1, 5, 7, 3],
        ]
        obstacle = Mesh(
            vertices,
            np.array(
                [[q[0], q[1], q[2]] for q in quads]
                + [[q[0], q[2], q[3]] for q in quads]
            ),
        )
        events, _work = sweep_surfaces(
            obstacle, [[[0, 0, 0], [0.5, 0, 0]]], [-0.125] * 3, [0.125] * 3
        )
        # The box is wholly inside a closed cage without hitting its surfaces.
        self.assertEqual(events, [])

    def test_vertical_motion_and_rounded_zero_segments_need_no_xy_direction(self):
        ceiling = mesh([[[-2, -2, 1.5], [2, -2, 1.5], [0, 2, 1.5]]])
        path = [[0, 0, 0], [0, 0, 0], [0, 0, 2]]
        for method in ("scan", "bounds"):
            events, _work = sweep_surfaces(
                ceiling, [path], [-0.125, -0.125, 0], [0.125, 0.125, 1], method=method
            )
            self.assertEqual(events, [[0, 1, 0, "interior"]])

    def test_random_nondegenerate_cases_match_independent_lp_away_from_contact(self):
        rng = np.random.default_rng(1003)
        lower, upper = np.array([-0.25, -0.125, 0]), np.array([0.25, 0.125, 1])
        compared = 0
        for _ in range(64):
            triangle, path = rng.uniform(-2, 2, (3, 3)), rng.uniform(-1, 1, (2, 3))
            events, _work = sweep_surfaces(mesh([triangle]), [path], lower, upper)
            witness = penetration_witness(triangle, *path, lower, upper)
            margin = witness["lp_maximum_box_margin_m"]
            if abs(margin) > 1e-7:
                self.assertEqual(bool(events), margin > 0)
                if events:
                    self.assertTrue(witness["exact_interior_witness"])
                compared += 1
        self.assertEqual(compared, 64)


class NativeBoxSweep(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _plan, data, support, cls.routes, _source = construct_routes(
            "bounds", "float32", False
        )
        cls.mesh = Mesh(support.vertices, data["body_faces"])

    def test_outward_bounds_and_scan_return_the_same_full_body_surface_events(self):
        paths = [route["xyz_m"] for route in self.routes]
        lower, upper = [-0.14, -0.14, 0], [0.14, 0.14, 1.8]
        bounded, bounded_work = sweep_surfaces(self.mesh, paths, lower, upper)
        scanned, scanned_work = sweep_surfaces(
            self.mesh, paths, lower, upper, method="scan"
        )
        self.assertEqual(len(self.mesh.triangles), 784)
        self.assertEqual(bounded, scanned)
        self.assertLess(
            bounded_work["candidate_pairs"], scanned_work["candidate_pairs"]
        )
        self.assertLess(bounded_work["exact_vertices"], scanned_work["exact_vertices"])

    def test_unchanged_supported_stair_curve_is_not_a_rigid_box_body_trajectory(self):
        route = next(route for route in self.routes if any(route["riser_segments"]))
        lower, upper = [-0.14, -0.14, 0], [0.14, 0.14, 1.8]
        events, _work = sweep_surfaces(self.mesh, [route["xyz_m"]], lower, upper)
        candidates = [event for event in events if event[3] == "interior"]
        self.assertTrue(candidates)
        # One independent exact point suffices to refute the implied body trajectory.
        proofs = []
        points = np.array(route["xyz_m"])
        for _route, segment, face, _kind in candidates[:: max(1, len(candidates) // 8)][
            :8
        ]:
            proofs.append(
                penetration_witness(
                    self.mesh.vertices[self.mesh.triangles[face]],
                    *points[segment : segment + 2],
                    lower,
                    upper,
                )
            )
        self.assertTrue(
            any(
                proof["exact_interior_witness"]
                and proof["reconstructed_box_margin_m"] > 0.05
                for proof in proofs
            )
        )


if __name__ == "__main__":
    unittest.main()
