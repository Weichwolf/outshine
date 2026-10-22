import copy
import unittest

import numpy as np
import shapely
from stair_band_remesh import remesh
from stair_band_rings import clean_ring
from stair_indexed_levels import LevelCuts


def two_sources():
    xy = np.array([[0, 0], [1, 0], [1, 1], [0, 1], [0, 0.5], [-1, 0], [-1, 1]], float)
    points = np.c_[xy, xy[:, 0] * 0.1]
    faces = np.array([[4, 0, 1], [4, 1, 2], [4, 2, 3], [5, 0, 4], [5, 4, 6], [6, 4, 3]])
    owners, steps = np.array([1, 1, 1, 2, 2, 2]), np.array([1, 1, 1, -1, -1, -1])
    flight = {
        "owner": 0,
        "origin_xy_m": np.zeros(2),
        "tangent": np.array([1.0, 0.0]),
        "start_cut_m": 0.0,
        "end_cut_m": 1.0,
    }
    recipe = {
        "owner": 0,
        "low": 0.0,
        "high": 0.1,
        "risers": 1,
        "rise_m": 0.1,
        "level_offset": 0,
    }
    note = {"stairs": [recipe]}
    plan = {"stair_lofts": [flight], "stair_directions": {0: 1}}
    return points, faces, owners, steps, np.array([0, 0.1]), note, plan


class IndexedStairBandTests(unittest.TestCase):
    def test_adjacent_bands_and_faces_share_one_original_edge_intersection(self):
        cuts = LevelCuts(
            np.array([[0, 0, 0], [2, 0, 2], [0, 1, 0]], float),
            np.array([0, 0.5, 1, 1.5, 2]),
        )
        low = cuts.crossing(0, 1, 1)
        high_after_low = cuts.crossing(low, 1, 2)
        high_from_original = cuts.crossing(1, 0, 2)
        self.assertEqual(high_after_low, high_from_original)
        np.testing.assert_array_equal(cuts.points[high_after_low], [1, 0, 1])
        self.assertEqual(len(cuts.points), 5)

    def test_numeric_ring_cleanup_keeps_shared_vertices_and_real_corners(self):
        ring = [[0, 0], [0.25, 0], [0.5, 0], [1, 0], [1, 1], [0, 1], [0, 0]]
        result, note = clean_ring(ring, {(0.5, 0)})
        self.assertEqual(len(result), 5)
        self.assertIn((0.5, 0), set(map(tuple, result)))
        self.assertEqual(note["maximum_chain_vertex_deviation_m"], 0)
        self.assertLessEqual(note["ring_visits"], 8 * 6)

    def test_accumulated_small_deletions_cannot_remove_a_real_boundary_bend(self):
        x = np.linspace(0, 1, 17)
        curve = np.c_[x, 1e-9 * np.sin(x * np.pi)]
        ring = [*curve, [1, 1], [0, 1], curve[0]]
        with self.assertRaisesRegex(ValueError, "accumulated band boundary"):
            clean_ring(ring, set())

    def test_remesh_keeps_shared_source_vertex_and_other_source_faces(self):
        args = two_sources()
        before = copy.deepcopy(args)
        points, faces, owners, _steps, _levels, note = remesh(*args)
        shared = np.flatnonzero(np.all(points == before[0][4], axis=1))
        self.assertEqual(len(shared), 1)
        for mask in (1, 2):
            self.assertIn(int(shared[0]), faces[owners == mask])
        np.testing.assert_array_equal(
            points[faces[owners == 2]], before[0][before[1][before[2] == 2]]
        )
        for a, b in zip(args[:5], before[:5], strict=True):
            np.testing.assert_array_equal(a, b)
        self.assertEqual(note["band_remesh"]["maximum_loft_height_residual_m"], 0)
        polygons = shapely.polygons(points[faces[owners == 1], :2])
        self.assertTrue(shapely.coverage_is_valid(polygons))
        self.assertAlmostEqual(shapely.union_all(polygons).area, 1)

    def test_nonplanar_reference_is_not_flattened_by_retriangulation(self):
        args = list(two_sources())
        args[0][4, 2] = 0.01
        with self.assertRaisesRegex(ValueError, "declared flat/linear/flat field"):
            remesh(*args)

    def test_stair_level_cannot_be_relabelled_from_another_source(self):
        args = list(two_sources())
        args[3][0] = 2
        with self.assertRaisesRegex(ValueError, "level belongs to another stair"):
            remesh(*args)

    def test_noninteger_ownership_is_refused_before_remeshing(self):
        args = list(two_sources())
        args[2] = args[2].astype(float)
        with self.assertRaisesRegex(ValueError, "indexed source and step"):
            remesh(*args)

    def test_other_height_layer_keeps_separate_vertices_at_the_same_xy(self):
        args = list(two_sources())
        extra = args[0][:3].copy()
        extra[:, 2] += 10
        args[0] = np.vstack([args[0], extra])
        args[1] = np.vstack([args[1], [7, 8, 9]])
        args[2] = np.r_[args[2], 4]
        args[3] = np.r_[args[3], -1]
        points, faces, owners, *_rest = remesh(*args)
        np.testing.assert_array_equal(points[faces[owners == 4]][0], extra)

    def test_retriangulation_preserves_a_hole_in_the_prepared_band(self):
        shape = shapely.Polygon(
            [[0, 0], [2, 0], [2, 1], [0, 1]],
            [[[0.5, 0.25], [1.5, 0.25], [1.5, 0.75], [0.5, 0.75]]],
        )
        triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(shape))
        xy = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
        points, inverse = np.unique(xy.reshape(-1, 2), axis=0, return_inverse=True)
        faces = inverse.reshape(-1, 3)
        coords = points[faces]
        a, b = coords[:, 1] - coords[:, 0], coords[:, 2] - coords[:, 0]
        reverse = a[:, 0] * b[:, 1] < a[:, 1] * b[:, 0]
        faces[reverse] = faces[reverse, ::-1]
        points = np.c_[points, points[:, 0] * 0.05]
        _p, _f, _o, _s, levels, note, plan = two_sources()
        plan["stair_lofts"][0]["end_cut_m"] = 2
        result = remesh(
            points,
            faces,
            np.ones(len(faces), int),
            np.ones(len(faces), int),
            levels,
            note,
            plan,
        )
        actual = shapely.union_all(shapely.polygons(result[0][result[1], :2]))
        self.assertEqual(len(actual.interiors), 1)
        self.assertLess(actual.symmetric_difference(shape).area, 1e-7)
