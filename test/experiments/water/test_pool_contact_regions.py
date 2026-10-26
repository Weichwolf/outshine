"""Planar interior/exterior/corner controls; no manufactured native terrain heights."""

import unittest
from fractions import Fraction as F

import numpy as np
from pool_body import build
from pool_body_experiment import FIXTURE, construct, original
from pool_cell_partition import _area, partition
from pool_terrain_contact import contact_patch


def exact(points):
    return [tuple(F(float(v)) for v in p[:2]) for p in points]


def inside_area(points, faces, ring):
    total = F(0)
    for face in faces:
        centre = tuple(sum(points[i][axis] for i in face) / 3 for axis in (0, 1))
        signs = [
            (b[0] - a[0]) * (centre[1] - a[1]) - (b[1] - a[1]) * (centre[0] - a[0])
            for a, b in zip(ring, ring[1:] + ring[:1])
        ]
        if all(s >= 0 for s in signs):
            total += _area([points[i] for i in face])
    return total


def body_from_polygon(xy, faces):
    vertices = np.column_stack((xy, np.zeros(len(xy))))
    return build(
        vertices[np.array(faces)],
        lambda points: np.zeros(len(points)),
        kind="swimming_pool",
        ground_role="declared_ground",
        footprint_role="declared_complete_local_component",
        wall_width_m=0.25,
    )


class PoolContactRegions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data, source, _, water, _ = original(FIXTURE)
        cls.body = construct(data, source, water)
        cls.water = water
        cls.square = body_from_polygon(
            [[0, 0], [2, 0], [2, 2], [0, 2]], [[0, 1, 2], [0, 2, 3]]
        )

    def verify_disk(self, source, source_faces, result):
        _, faces, points, _ = result
        expected = sum(_area(exact(source[f])) for f in source_faces)
        self.assertEqual(sum(_area([points[i] for i in f]) for f in faces), expected)
        self.assertTrue(all(_area([points[i] for i in f]) > 0 for f in faces))
        self.assertTrue(set(exact(source)) <= set(points))
        edges = np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1)
        unique, counts = np.unique(edges, axis=0, return_counts=True)
        self.assertTrue(np.all((counts == 1) | (counts == 2)))
        self.assertEqual(len(points) - len(unique) + len(faces), 1)

    def test_all_original_water_facets_as_interior_plan_control(self):
        vertices, indices = np.unique(
            self.water.reshape(-1, 3), axis=0, return_inverse=True
        )
        source_faces = indices.reshape(-1, 3)
        result = partition(self.body, vertices, source_faces)
        self.verify_disk(vertices, source_faces, result)
        self.assertEqual(result[3]["pool_boundary_edges_considered"], 0)
        _, _, _, report = contact_patch(self.body, vertices, source_faces)
        self.assertLess(report["contact"]["maximum_above_contact_surface_m"], 1e-7)
        self.assertLess(report["contact"]["maximum_below_contact_surface_m"], 1e-7)
        self.assertEqual(report["admission"], "candidate_pending_neighbor_updates")
        self.assertEqual(self.body["water_triangles"].tobytes(), self.water.tobytes())

    def test_whole_original_outline_is_covered_inside_bounded_plan_control(self):
        low = self.body["vertices"][:, :2].min(axis=0) - 1
        high = self.body["vertices"][:, :2].max(axis=0) + 1
        source = np.array(
            [
                [low[0], high[1], 1],
                [high[0], high[1], 1],
                [low[0], low[1], 1],
                [high[0], low[1], 1],
            ]
        )
        source_faces = np.array([[0, 2, 3], [0, 3, 1]])
        result = partition(self.body, source, source_faces)
        self.verify_disk(source, source_faces, result)
        ring = exact(self.body["plan_xy"][self.body["outer_ring"]])
        self.assertEqual(result[3]["pool_boundary_edges_considered"], len(ring))
        self.assertEqual(inside_area(result[2], result[1], ring), _area(ring))

    def test_analytic_convex_corner_has_exact_one_sixth_inside(self):
        source = np.array([[-1.25, -0.25, 0], [-0.25, -1.25, 0], [0.25, 0.25, 0]])
        source_faces = np.array([[0, 1, 2]])
        result = partition(self.square, source, source_faces)
        self.verify_disk(source, source_faces, result)
        ring = exact(self.square["plan_xy"][self.square["outer_ring"]])
        self.assertEqual(result[3]["pool_boundary_edges_considered"], 2)
        self.assertEqual(inside_area(result[2], result[1], ring), F(1, 3))
        self.assertEqual(_area(exact(source)), 2)
        self.assertIn((F(-1, 4) + F(1, 3), F(-1, 4)), result[2])

    def test_exterior_triangle_retains_its_area_and_original_nodes(self):
        source = np.array([[-4, -4, 0], [-3, -4, 0], [-3, -3, 0]])
        source_faces = np.array([[0, 1, 2]])
        result = partition(self.square, source, source_faces)
        self.verify_disk(source, source_faces, result)
        ring = exact(self.square["plan_xy"][self.square["outer_ring"]])
        self.assertEqual(result[3]["pool_boundary_edges_considered"], 0)
        self.assertEqual(inside_area(result[2], result[1], ring), 0)

    def test_concave_partial_contact_is_not_assumed_to_be_half_plane_intersection(self):
        body = body_from_polygon(
            [[0, 0], [2, 0], [2, 1], [1, 1], [1, 2], [0, 2]],
            [[0, 1, 3], [1, 2, 3], [0, 3, 5], [3, 4, 5]],
        )
        source = np.array(
            [[0.75, 0.75, 0], [1.75, 0.75, 0], [1.75, 1.75, 0], [0.75, 1.75, 0]]
        )
        with self.assertRaisesRegex(ValueError, "convex edge half-planes"):
            partition(body, source, np.array([[0, 1, 2], [0, 2, 3]]))

    def check_flat_floor_boundary(self, method):
        level = self.square["bottom_height_m"][0]
        profiles = []
        for xy, edge in (
            ([[0.25, 0.25], [1.75, 0.25], [0.25, 1.75]], [1, 2]),
            ([[1.75, 0.25], [1.75, 1.75], [0.25, 1.75]], [0, 2]),
        ):
            source = np.column_stack((xy, np.full(3, level)))
            vertices, _, _, report = contact_patch(
                self.square, source, np.array([[0, 1, 2]]), method=method
            )
            self.assertLessEqual(report["maximum_outer_boundary_height_change_m"], 1e-7)
            self.assertEqual(report["new_outer_boundary_vertices"], 1)
            self.assertTrue(report["neighbor_update_required"])
            self.assertEqual(report["admission"], "candidate_pending_neighbor_updates")
            profile = {}
            a, b = exact(source[edge])
            for point, position in zip(exact(vertices), vertices):
                det = (b[0] - a[0]) * (point[1] - a[1]) - (b[1] - a[1]) * (
                    point[0] - a[0]
                )
                if det == 0:
                    t = (point[0] - a[0]) / (b[0] - a[0])
                    if 0 <= t <= 1:
                        profile[t] = position
            profiles.append(profile)
        self.assertEqual(set(profiles[0]), {F(0), F(1, 2), F(1)})
        self.assertEqual(profiles[0].keys(), profiles[1].keys())
        for t in profiles[0]:
            self.assertEqual(profiles[0][t].tobytes(), profiles[1][t].tobytes())

    def test_exact_unchanged_height_still_demands_shared_new_boundary_nodes(self):
        self.check_flat_floor_boundary("exact")

    def test_geos_unchanged_height_still_demands_shared_new_boundary_nodes(self):
        self.check_flat_floor_boundary("geos")


if __name__ == "__main__":
    unittest.main()
