import unittest

import numpy as np
import shapely
from compare_stair_route_sections import construct_routes
from shapely import affinity
from stair_route_width import measure_routes, projected_support, side_clearance


def neck():
    return shapely.union_all(
        [
            shapely.box(-2, -1, -0.1, 1),
            shapely.box(-0.1, -0.1, 0.1, 0.1),
            shapely.box(0.1, -1, 2, 1),
        ]
    )


class ContinuousSideRoom(unittest.TestCase):
    def test_positive_support_disks_do_not_prove_passage_through_a_narrow_neck(self):
        domain = neck()
        endpoints = [[-1.5, 0], [1.5, 0]]
        self.assertFalse(domain.buffer(-0.14).is_empty)
        for point in endpoints:
            self.assertGreater(shapely.Point(point).distance(domain.boundary), 0.14)
        result = side_clearance(domain, endpoints)
        self.assertTrue(result["covered"])
        self.assertAlmostEqual(result["available_radius_m"], 0.1)
        self.assertLess(result["available_radius_m"], 0.14)

    def test_holes_count_as_boundaries_and_missing_support_has_no_radius(self):
        domain = shapely.box(-2, -2, 2, 2).difference(
            shapely.box(-0.25, -0.25, 0.25, 0.25)
        )
        across = side_clearance(domain, [[-1, 0], [1, 0]])
        self.assertFalse(across["covered"])
        self.assertIsNone(across["available_radius_m"])
        self.assertEqual(across["status"], "unsupported_route")
        above = side_clearance(domain, [[-1, 0.6], [1, 0.6]])
        self.assertAlmostEqual(above["available_radius_m"], 0.35)

    def test_boundary_contact_is_preserved_without_trimming_the_route(self):
        result = side_clearance(shapely.box(-2, -1, 2, 1), [[-2, 0], [1, 0]])
        self.assertTrue(result["covered"])
        self.assertEqual(result["available_radius_m"], 0)
        self.assertEqual(result["status"], "boundary_contact")
        self.assertEqual(result["witness_xy_m"], [[-2.0, 0.0], [-2.0, 0.0]])

    def test_standstill_and_outside_point_are_distinguished(self):
        domain = shapely.box(-2, -1, 2, 1)
        self.assertEqual(side_clearance(domain, [[0, 0]])["available_radius_m"], 1)
        outside = side_clearance(domain, [[10, 0]])
        self.assertIsNone(outside["available_radius_m"])
        self.assertEqual(outside["nearest_boundary_distance_m"], 8)

    def test_rotation_and_translation_preserve_the_analytic_capacity(self):
        domain = affinity.translate(
            affinity.rotate(neck(), 0.437, origin=(0, 0), use_radians=True), 37, -81
        )
        rotation = np.array(
            [[np.cos(0.437), -np.sin(0.437)], [np.sin(0.437), np.cos(0.437)]]
        )
        points = np.array([[-1.5, 0], [1.5, 0]]) @ rotation.T + [37, -81]
        self.assertAlmostEqual(
            side_clearance(domain, points)["available_radius_m"], 0.1, delta=1e-13
        )


class NativeProjectedRoom(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _plan, _data, cls.mesh, cls.routes, _report = construct_routes(
            "bounds", "float32", False
        )

    def test_common_projection_and_rebuild_have_identical_continuous_results(self):
        shared, shared_work = measure_routes(self.mesh, self.routes)
        rebuilt, rebuilt_work = measure_routes(self.mesh, self.routes, method="rebuild")
        self.assertEqual(shared, rebuilt)
        self.assertEqual(shared_work["projection_builds"], 1)
        self.assertEqual(rebuilt_work["projection_builds"], 6)
        self.assertEqual(
            shared_work["union_operands"] * 6, rebuilt_work["union_operands"]
        )

    def test_internal_curves_have_side_room_and_all_four_cut_contacts_remain_zero(self):
        rows, _work = measure_routes(self.mesh, self.routes)
        degree = {}
        for route in self.routes:
            for node in route["nodes"]:
                degree[node] = degree.get(node, 0) + 1
        contacts = 0
        for route, result in zip(self.routes, rows, strict=True):
            self.assertTrue(result["covered"])
            if all(degree[node] > 1 for node in route["nodes"]):
                self.assertGreater(result["available_radius_m"], 0.14)
            else:
                self.assertEqual(result["available_radius_m"], 0)
                contacts += 1
        self.assertEqual(contacts, 4)

    def test_projection_retains_positive_areas_and_reports_vertical_faces(self):
        domain, work = projected_support(self.mesh)
        self.assertTrue(domain.is_valid)
        self.assertEqual(work["union_operands"], 196)
        self.assertEqual(work["zero_projected_area_faces"], 36)


if __name__ == "__main__":
    unittest.main()
