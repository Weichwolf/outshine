"""Independent exact elimination checks the whole metadata-derived capture hull."""

import unittest
from fractions import Fraction as F
from itertools import product

import numpy as np
from field_capture_hull import capture_hull
from field_pose_geometry import geometric_candidate, projection_bound
from field_view_projection import CapturePlane
from field_view_sampling import FieldGrid
from test_field_pose_geometry import exact_error, perspective


def exact_position(grid, coordinates):
    rows = [
        [*[F(float(v)) for v in axis], F(float(value))]
        for axis, value in zip(
            (*grid.axes(), grid.toward_eye), coordinates, strict=True
        )
    ]
    for column in range(3):
        pivot = next(r for r in range(column, 3) if rows[r][column])
        rows[column], rows[pivot] = rows[pivot], rows[column]
        factor = rows[column][column]
        rows[column] = [x / factor for x in rows[column]]
        for r in range(3):
            if r != column:
                factor = rows[r][column]
                rows[r] = [
                    x - factor * y for x, y in zip(rows[r], rows[column], strict=True)
                ]
    return tuple(F(float(c)) + rows[i][3] for i, c in enumerate(grid.centre))


class CaptureHullTests(unittest.TestCase):
    def setUp(self):
        self.grid = FieldGrid((123, -57, 46), (0.5, 2**-0.5, 0.5), 473)
        self.hull = capture_hull(self.grid, complete_original_capture_domain=True)

    def test_interval_vertices_enclose_exact_capture_box_not_just_rounded_positions(
        self,
    ):
        for grid in (
            self.grid,
            FieldGrid((6e6, -2e6, 3e6), (0, 1, 0), 0.125),
            FieldGrid((0, 0, 0), (0.6, 0, 0.8), 20),
        ):
            hull = capture_hull(grid, complete_original_capture_domain=True)
            for signs, vertex in zip(
                product((-1, 1), repeat=3), hull["vertices"], strict=True
            ):
                coordinates = [
                    s * h * grid.half_extent_m
                    for s, h in zip(signs, (1, 1, 2), strict=True)
                ]
                exact = exact_position(grid, coordinates)
                for i, (value, bound) in enumerate(zip(exact, vertex, strict=True)):
                    self.assertLessEqual(F(bound[0]), value)
                    self.assertLessEqual(value, F(bound[1]))
                    self.assertLessEqual(F(hull["lower"][i]), value)
                    self.assertLessEqual(value, F(hull["upper"][i]))

    def test_preserving_hull_correlation_improves_admission_without_source_data(self):
        plane = CapturePlane(
            self.hull["lower"],
            self.hull["upper"],
            self.grid.centre,
            self.grid.toward_eye,
        )
        eye = np.asarray(self.grid.centre) + np.asarray(self.grid.toward_eye) * 64000
        basis = (*self.grid.axes(), -np.asarray(self.grid.toward_eye))
        args = (plane, eye, basis, perspective(), (1280, 720))
        box = projection_bound(*args)
        correlated = projection_bound(*args, source_hull=self.hull["vertices"])
        self.assertFalse(geometric_candidate(box, 0.25))
        self.assertTrue(geometric_candidate(correlated, 0.25))
        self.assertLess(correlated["screen_l1_upper_px"], box["screen_l1_upper_px"] / 5)
        rng = np.random.default_rng(18211)
        for coordinates in rng.uniform(-1, 1, (30, 3)) * [473, 473, 946]:
            exact = exact_position(self.grid, coordinates)
            actual = exact_error(
                plane, exact, eye, basis, perspective(), (1280, 720), plane.toward_eye
            )
            self.assertLessEqual(sum(actual), F(correlated["screen_l1_upper_px"]))

    def test_capture_domain_unknown_is_not_an_empty_or_valid_source_hull(self):
        for known in (False, None, 1):
            with self.assertRaises(ValueError):
                capture_hull(self.grid, complete_original_capture_domain=known)
        with self.assertRaises(TypeError):
            capture_hull({}, complete_original_capture_domain=True)

    def test_invalid_or_unbound_interval_vertices_are_rejected(self):
        plane = CapturePlane(
            self.hull["lower"],
            self.hull["upper"],
            self.grid.centre,
            self.grid.toward_eye,
        )
        args = (plane, (0, 0, 64000), np.eye(3), perspective(), (1280, 720))
        for vertices in (
            (),
            self.hull["vertices"][:7],
            [((0, 0), (0, 0), (1e10, 1e10))] * 8,
            [((0, 0), (0, 0), (1, -1))] * 8,
        ):
            with self.assertRaises(ValueError):
                projection_bound(*args, source_hull=vertices)


if __name__ == "__main__":
    unittest.main()
