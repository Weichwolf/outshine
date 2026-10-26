"""Independent analytic controls and critical contact regressions for #955."""

import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from check_roof_invariance import check, transform_record
from outshine_ref.buildings.modelling import (
    HipPlanner,
    SkeletonBudgetExceeded,
    SkeletonLimits,
    hip_footprints,
    prepare_hip,
)
from outshine_ref.buildings.modelling._roof_contours import stitch_contacts
from outshine_ref.buildings.modelling.roof_partition import collision, velocities
from outshine_ref.buildings.modelling.skeleton_plan import refine
from verify_hip_fixture import verify
from verify_modelling_roof_shed import source
from verify_roof_partition_consumer import coplanar_control, verify_prepared


def height(plan, point):
    local = np.asarray(point) - plan.origin
    values = [
        local @ f.gradient + f.intercept_m
        for f in plan.facets
        if f.polygon.covers(shapely.Point(local))
    ]
    if not values:
        return None
    np.testing.assert_allclose(values, values[0], atol=1e-7, rtol=0)
    return values[0]


class RoofPartitionTests(unittest.TestCase):
    def test_coplanar_cut_moves_without_changing_source_uv_or_volume(self):
        original, first, second = coplanar_control()
        for value, mode, expected in (
            (3.0, "byHeight", 72.0),
            (30.0, "byAngle", 72.0 * np.tan(np.deg2rad(30))),
        ):
            for plan in (first, second):
                roof, result = verify_prepared(original, plan, value, mode)
                self.assertAlmostEqual(result["volume_m3"], expected, places=9)
                self.assertGreater(result["uv_values_checked"], 0)
                self.assertTrue(
                    {"uv", *(f"uv{i}" for i in range(1, 10))} <= roof.emit().keys()
                )

    def test_nearly_parallel_fronts_have_stable_unit_advance(self):
        for angle in (1e-12, -1e-12, 0.0, 0.1):
            normals = np.array(
                [
                    [0.6, 0.8],
                    [
                        0.6 * np.cos(angle) - 0.8 * np.sin(angle),
                        0.6 * np.sin(angle) + 0.8 * np.cos(angle),
                    ],
                ]
            )
            speed = velocities(np.array([0, 1]), normals)
            np.testing.assert_allclose(speed @ normals.T, 1.0, rtol=0, atol=2e-15)

    def test_valid_nearly_dependent_planes_do_not_displace_their_node(self):
        node = np.array([[12.0, 23.0]])
        distances = np.array([3.0])
        normals = np.array([[0.0, 1.0], [1e-12, 1.0], [1.0, 0.0]])
        intercepts = distances[0] - normals @ node[0]
        owners = [
            (i, n, c) for i, (n, c) in enumerate(zip(normals, intercepts, strict=True))
        ]
        points, heights, correction = refine(node, distances, [(0,)] * 3, owners)
        np.testing.assert_array_equal(points, node)
        np.testing.assert_array_equal(heights, distances)
        self.assertEqual(correction, 0.0)
        inconsistent = [
            (0, np.array([1.0, 0.0]), -1.0),
            (1, np.array([0.0, 1.0]), -2.0),
            (2, np.array([-1.0, 0.0]), 3.0),
        ]
        with self.assertRaisesRegex(ValueError, "metric correction"):
            refine(np.zeros((1, 2)), np.ones(1), [(0,)] * 3, inconsistent)

    def test_rigidly_transformed_original_countercases(self):
        fixture = json.loads(
            (Path(__file__).with_name("reference") / "hip-plans.json").read_text()
        )
        ids = {
            "Vienna:327170:159",
            "Tokyo:17970970:4224",
            "Tokyo:11479717002:0",
            "Tokyo:155129020:23",
            "Tokyo:1367300462:0",
        }
        for original in fixture["records"]:
            if original["id"] not in ids:
                continue
            with self.subTest(source=original["id"]):
                row = transform_record(original, 37, np.array([100000.0, -200000.0]))
                result = check(row, fixture["comparison"])
                self.assertLessEqual(result["maximum_surface_height_error_m"], 1e-7)
                if original["id"] != "Tokyo:1367300462:0":
                    self.assertTrue(result["strict_facet_ownership_match"])

    def test_collapsed_remote_edge_is_a_point_not_its_supporting_line(self):
        small = np.array([[0.0, 0.0], [2.0, 0.0], [2.0, 2.0], [0.0, 2.0]])
        large = np.array([[100.0, 0.0], [120.0, 0.0], [120.0, 20.0], [100.0, 20.0]])
        loops = [(np.arange(4), small), (np.arange(4, 8), large)]
        normals, intercepts = [], []
        for _, points in loops:
            edges = np.roll(points, -1, axis=0) - points
            n = (
                np.c_[-edges[:, 1], edges[:, 0]]
                / np.linalg.norm(edges, axis=1)[:, None]
            )
            normals.extend(n)
            intercepts.extend(-np.sum(n * points, axis=1))
        normals = np.array(normals)
        dt, _, events = collision(
            loops,
            [velocities(ids, normals) for ids, _ in loops],
            normals,
            np.array(intercepts),
            0.0,
            1e-11,
        )
        self.assertAlmostEqual(dt, 1.0)
        self.assertTrue(all(i < 4 for participants, _ in events for i in participants))

    def test_existing_desktop_worker_transfers_and_reuses_contour_plans(self):
        triangle = shapely.Polygon([(0, 0), (12, 0), (0, 9)])
        with HipPlanner() as planner:
            pid = planner.pid
            first = planner.prepare(triangle, "contour")
            second = planner.prepare(triangle, "contour")
            self.assertEqual(planner.pid, pid)
            np.testing.assert_array_equal(first.positions_m, second.positions_m)
            self.assertFalse(second.positions_m.flags.writeable)
            self.assertAlmostEqual(second.distances_m.max(), 3.0, places=9)

    def test_rotated_rectangles_and_windings(self):
        rotation = np.array([[0.6, -0.8], [0.8, 0.6]])
        origin = np.array([100000.0, -200000.0])
        for width, depth in ((20.0, 10.0), (10.0, 20.0), (10.0, 10.0)):
            ring = (
                np.array([[0, 0], [width, 0], [width, depth], [0, depth]]) @ rotation.T
                + origin
            )
            for points in (ring, ring[::-1]):
                plan = prepare_hip(shapely.Polygon(points), "contour")
                self.assertEqual(len(plan.facets), 4)
                self.assertAlmostEqual(
                    plan.distances_m.max(), min(width, depth) / 2, places=8
                )
                for u, v in (
                    (0.37, 0.29),
                    (width / 2, depth / 2),
                    (width - 0.4, depth - 0.6),
                ):
                    self.assertAlmostEqual(
                        height(plan, np.array([u, v]) @ rotation.T + origin),
                        min(u, width - u, v, depth - v),
                        places=7,
                    )

    def test_triangle_inradius(self):
        plan = prepare_hip(shapely.Polygon([(0, 0), (12, 0), (0, 9)]), "contour")
        self.assertAlmostEqual(plan.distances_m.max(), 3.0, places=9)
        self.assertAlmostEqual(height(plan, (3, 3)), 3.0, places=9)

    def test_concave_missing_corner_stays_empty(self):
        polygon = shapely.Polygon([(0, 0), (10, 0), (10, 4), (4, 4), (4, 10), (0, 10)])
        plan = prepare_hip(polygon, "contour")
        self.assertEqual(len(plan.facets), 6)
        self.assertIsNone(height(plan, (7, 7)))
        self.assertAlmostEqual(plan.distances_m.max(), 2.0, places=9)

    def test_courtyard_remains_open(self):
        polygon = shapely.Polygon(
            [(0, 0), (20, 0), (20, 20), (0, 20)], [[(8, 8), (8, 12), (12, 12), (12, 8)]]
        )
        plan = prepare_hip(polygon, "contour")
        self.assertEqual(len(plan.facets), 8)
        self.assertIsNone(height(plan, (10, 10)))
        self.assertAlmostEqual(height(plan, (5, 10)), 3.0, places=9)

    def test_collinear_original_eave_is_not_dissolved(self):
        plan = prepare_hip(
            shapely.Polygon([(0, 0), (5, 0), (10, 0), (10, 6), (0, 6)]), "contour"
        )
        self.assertEqual(len(plan.facets), 5)
        self.assertEqual(len({f.boundary for f in plan.facets}), 5)
        self.assertAlmostEqual(height(plan, (4, 2)), 2.0, places=9)

    def test_touching_hole_stitches_before_further_motion(self):
        outer = np.array([[0, 0], [10, 0], [20, 0], [20, 20], [0, 20]], dtype=float)
        hole = np.array([[10, 0], [7, 3], [10, 6], [13, 3]], dtype=float)
        loops = [(np.array([0, 0, 1, 2, 3]), outer), (np.arange(4, 8), hole)]
        normals = np.empty((8, 2))
        for ids, points in loops:
            edges = np.roll(points, -1, axis=0) - points
            normals[ids] = (
                np.c_[-edges[:, 1], edges[:, 0]]
                / np.linalg.norm(edges, axis=1)[:, None]
            )
        result = stitch_contacts(loops, 1e-11)
        self.assertEqual(len(result), 1)
        ids, points = result[0]
        self.assertEqual(np.count_nonzero(np.all(points == [10, 0], axis=1)), 2)
        advanced = shapely.Polygon(points + velocities(ids, normals) * 0.001)
        expected = (
            shapely.box(0, 0, 20, 20)
            .buffer(-0.001, join_style="mitre")
            .difference(shapely.Polygon(hole).buffer(0.001, join_style="mitre"))
        )
        self.assertTrue(advanced.is_valid)
        self.assertEqual(len(advanced.interiors), 0)
        self.assertLess(advanced.symmetric_difference(expected).area, 1e-8)

    def test_existing_limits_still_refuse_complete_plan(self):
        polygon = shapely.box(0, 0, 20, 10)
        for limits in (
            SkeletonLimits(vertices=3),
            SkeletonLimits(nodes=3),
            SkeletonLimits(python_events=0),
        ):
            with self.subTest(limits=limits), self.assertRaises(SkeletonBudgetExceeded):
                source = (
                    shapely.Polygon([(0, 0), (12, 0), (0, 9)])
                    if limits.python_events == 0
                    else polygon
                )
                prepare_hip(source, "contour", limits)

    def test_near_touching_native_counterexample(self):
        polygon = shapely.Polygon(
            [
                (0, 0),
                (-0.45231913453437711, 0.45155481886627058),
                (-0.90463826906875378, -0.45155484650368294),
                (-2.2615956726718842, 0.45155481886627058),
                (3.1662339417406367, 9.9341996371328207),
                (-7.6894252870844051, 15.804402235010485),
                (-12.255186343214582, 7.1026908842675827),
                (-0.41524366068275098, -0.7772666189143429),
            ]
        )
        with self.assertRaisesRegex(ValueError, "clearance"):
            prepare_hip(polygon, "contour")

    def test_vienna_rest_case_without_either_skeleton_dependency(self):
        fixture = json.loads(
            (Path(__file__).with_name("reference") / "hip-plans.json").read_text()
        )
        row = next(r for r in fixture["records"] if r["id"] == "Vienna:1210000:12")
        blocked = {
            name: None
            for name in (
                "pysfcgal",
                "pysfcgal.geometry",
                "py_straight_skeleton",
                "py_straight_skeleton.algorithm",
            )
        }
        with patch.dict(sys.modules, blocked):
            plan = verify(
                row,
                fixture["comparison"],
                lambda polygon: prepare_hip(polygon, "contour"),
            )
        self.assertEqual(len(plan.facets), 45)
        self.assertFalse(plan.positions_m.flags.writeable)

    def test_existing_native_57_eave_courtyard_is_complete(self):
        fixture = json.loads(
            (Path(__file__).with_name("reference") / "hip-plans.json").read_text()
        )
        row = next(r for r in fixture["records"] if r["id"] == "Vienna:1137690:3")
        plan = verify(
            row, fixture["comparison"], lambda polygon: prepare_hip(polygon, "contour")
        )
        self.assertEqual(len(plan.facets), 57)
        self.assertAlmostEqual(
            sum(f.polygon.area for f in plan.facets), 2649.9305988923356, places=7
        )

    def test_prepared_plan_consumer_preserves_uv0_to9_without_reconstruction(self):
        polygons = [
            shapely.Polygon([(0, 0), (10, 0), (10, 4), (4, 4), (4, 10), (0, 10)]),
            shapely.Polygon(
                [(0, 0), (20, 0), (20, 20), (0, 20)],
                [[(8, 8), (8, 12), (12, 12), (12, 8)]],
            ),
        ]
        for polygon in polygons:
            original = source(polygon)
            plan = prepare_hip(hip_footprints(original)[0], "contour")
            for value, mode in ((3.0, "byHeight"), (30.0, "byAngle")):
                verify_prepared(original, plan, value, mode)


if __name__ == "__main__":
    unittest.main()
