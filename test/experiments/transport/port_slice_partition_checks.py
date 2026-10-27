"""Independent coverage, source-edge and rounding checks for progressive cuts."""

import itertools
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref.transport.port_heights._slices import SliceBuilder, SliceLimit
from port_plane_checks import field


class SlicePartitionChecks(unittest.TestCase):
    def test_rounding_snaps_do_not_duplicate_polygon_corners(self):
        xy = np.array([[50.0, 0], [100, 1], [0, 1]])
        builder = SliceBuilder(field(), xy, np.zeros(3))
        polygon = [(i, builder.term(0, i)[2], frozenset([i])) for i in range(3)]
        lower, upper = builder.split(polygon, 0, 0.5 + np.finfo(float).eps)
        for half in (lower, upper):
            ids = [p[0] for p in half]
            self.assertEqual(len(ids), len(set(ids)))
        self.assertIn(0, [p[0] for p in lower])
        self.assertIn(0, [p[0] for p in upper])

    def test_exact_corner_level_reuses_both_original_half_boundaries(self):
        xy = np.array([[0.0, -2], [0, 2], [100, 0]])
        builder = SliceBuilder(field(), xy, np.zeros(3))
        polygon = [(i, builder.term(0, i)[2], frozenset([i])) for i in range(3)]
        with patch.object(builder, "crossing", side_effect=AssertionError):
            lower, upper = builder.split(polygon, 0, 0.0)
        self.assertEqual([p[0] for p in lower], [0, 1])
        self.assertEqual([p[0] for p in upper], [0, 1, 2])
        self.assertEqual(len(builder.xy), 3)

    def test_skew_partitions_keep_coverage_winding_and_original_edge_priors(self):
        xy = np.array([[5.0, -3], [92, 2], [38, 5]])
        targets = 3 + xy @ [0.07, -0.013]
        for skew, order in itertools.product(
            (-0.3, 0.0, 0.2), itertools.permutations(range(3))
        ):
            with self.subTest(skew=skew, order=order):
                model = field(skew)
                builder = SliceBuilder(model, xy, targets)
                values = [builder.term(0, i)[2] for i in range(3)]
                knots = np.unique(np.r_[0.0, 1.0, np.linspace(0, 1, 17), values])
                faces, intervals = builder.triangle(np.array(order), 0, knots)
                generated = np.asarray(builder.xy)[faces]
                shapes = shapely.polygons(generated)
                self.assertTrue(shapely.coverage_is_valid(shapes))
                self.assertLess(
                    shapely.symmetric_difference(
                        shapely.coverage_union_all(shapes), shapely.Polygon(xy)
                    ).area,
                    1e-10,
                )
                a, b = (
                    generated[:, 1] - generated[:, 0],
                    generated[:, 2] - generated[:, 0],
                )
                signed = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
                original = xy[list(order)]
                d, e = original[1] - original[0], original[2] - original[0]
                self.assertTrue(np.all(signed * (d[0] * e[1] - d[1] * e[0]) > 0))
                for point in np.asarray(builder.xy)[3:]:
                    self.assertLess(
                        shapely.Polygon(xy).boundary.distance(shapely.Point(point)),
                        1e-11,
                    )
                np.testing.assert_allclose(
                    builder.targets,
                    3 + np.asarray(builder.xy) @ [0.07, -0.013],
                    rtol=0,
                    atol=1e-12,
                )
                for face, (lo, hi) in zip(faces, intervals):
                    at = np.asarray(builder.xy)[list(face)].mean(axis=0)
                    signed = (at - model.cut_centres[0]) * model.inward_normals[0]
                    distances = signed.sum(axis=1)
                    parameter = distances[0] / distances.sum()
                    self.assertGreaterEqual(parameter, lo - 1e-12)
                    self.assertLessEqual(parameter, hi + 1e-12)

    def test_crossings_retain_the_existing_vertex_budget(self):
        xy = np.array([[0.0, -2], [0, 2], [100, 0]])
        builder = SliceBuilder(field(), xy, np.zeros(3), max_vertices=3)
        with self.assertRaisesRegex(SliceLimit, "slice_vertex_budget"):
            builder.triangle(np.array([0, 2, 1]), 0, np.array([0, 0.5, 1]))


if __name__ == "__main__":
    unittest.main()
