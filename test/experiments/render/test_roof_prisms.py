"""Analytic ray, complete fallback and independent triangulated-solid controls."""

import unittest

import numpy as np
from native_capture_meshes import independent_depth
from primitive_bvh import build, trace
from roof_prism_control import explicit_mesh
from roof_prisms import Prisms, compile_prisms, intersect_planes, shape_planes


def box():
    return np.array(
        [
            [1, 0, 0, 1],
            [-1, 0, 0, 1],
            [0, 1, 0, 1],
            [0, -1, 0, 1],
            [0, 0, 1, 1],
            [0, 0, -1, 1],
        ],
        float,
    )


class PrismsTest(unittest.TestCase):
    def test_parallel_boundary_inside_and_behind(self):
        origins = np.array(
            [[-3, 0, 0], [-3, 1, 0], [-3, 1.01, 0], [0, 0, 0], [3, 0, 0]], float
        )
        directions = np.tile([1, 0, 0], (5, 1))
        t, _ = intersect_planes(box(), origins, directions)
        np.testing.assert_array_equal(t, [2, 2, np.inf, np.inf, np.inf])

    def test_bvh_matches_explicit_polyhedra(self):
        rng = np.random.default_rng(888)
        centres = rng.uniform(-15, 15, (21, 3))
        planes = np.array(
            [
                box() + np.column_stack((np.zeros((6, 3)), box()[:, :3] @ c))
                for c in centres
            ]
        )
        prisms = Prisms(
            planes.reshape(-1, 4),
            np.arange(0, 6 * 22, 6, dtype=np.uint32),
            np.zeros((21, 2), np.int32),
            np.stack((centres - 1, centres + 1), axis=1),
            np.zeros(21 * 6, bool),
            np.empty(0, np.uint32),
        )
        hierarchy = build(prisms.bounds)
        origins = rng.normal(size=(512, 3)) * 30
        directions = centres[np.arange(512) % 21] - origins
        expected = independent_depth(
            np.concatenate(
                [explicit_mesh(p, c) for p, c in zip(planes, centres, strict=True)]
            ),
            origins,
            directions,
        )
        actual, _, _, work = trace(prisms, hierarchy, origins, directions)
        np.testing.assert_array_equal(np.isfinite(actual), np.isfinite(expected))
        finite = np.isfinite(expected)
        np.testing.assert_allclose(
            actual[finite], expected[finite], atol=1e-9, rtol=1e-9
        )
        self.assertLess(work["primitive_ray_tests"], 512 * 21)

    def test_native_gable_and_invalid_footprint(self):
        shape = {
            "ring": [[-1, -1], [1, -1], [1, 1], [-1, 1]],
            "holes": [],
            "roof_kind": 1,
            "roof_deck_m": 2,
            "roof_bands": [
                {
                    "height_at_v0_m": 3,
                    "slope_v": 1,
                    "v_interval": {"min": None, "max": 0},
                },
                {
                    "height_at_v0_m": 3,
                    "slope_v": -1,
                    "v_interval": {"min": 0, "max": None},
                },
            ],
            "axis_u": [1, 0],
            "centre": [0, 0],
            "height_controls": [],
            "bottom_m": 0,
        }
        p, _, _ = shape_planes(shape)
        t, _ = intersect_planes(
            p,
            np.array([[0, 0, 8], [0, 0.5, 8], [0, 1.1, 8]]),
            np.tile([0, 0, -1], (3, 1)),
        )
        np.testing.assert_allclose(t, [5, 5.5, np.inf])
        native = {
            "projectable": True,
            "shapes": [shape],
            "east": [1, 0, 0],
            "north": [0, 1, 0],
            "up": [0, 0, 1],
            "origin": [0, 0, 0],
        }
        bad = dict(shape, ring=[[-1, -1], [1, -1], [0, 0], [1, 1], [-1, 1]])
        source = {"binding": np.array([[0, 7], [0, 8], [0, 8]], np.int32)}
        doc = {
            "sources": [
                {"index": 7, "surface": native},
                {"index": 8, "surface": dict(native, shapes=[shape, bad])},
            ]
        }
        compiled, reasons = compile_prisms(doc, source)
        np.testing.assert_array_equal(compiled.binding, [[7, 0]])
        np.testing.assert_array_equal(compiled.fallback_faces, [1, 2])
        self.assertEqual(set(reasons), {8})

    def test_reject_nonconcave_or_discontinuous_bands(self):
        shape = {
            "ring": [[-1, -1], [1, -1], [1, 1], [-1, 1]],
            "holes": [],
            "roof_kind": 1,
            "roof_deck_m": 2,
            "roof_bands": [
                {
                    "height_at_v0_m": 3,
                    "slope_v": -1,
                    "v_interval": {"min": None, "max": 0},
                },
                {
                    "height_at_v0_m": 3,
                    "slope_v": 1,
                    "v_interval": {"min": 0, "max": None},
                },
            ],
            "axis_u": [1, 0],
            "centre": [0, 0],
            "height_controls": [],
            "bottom_m": 0,
        }
        with self.assertRaises(ValueError):
            shape_planes(shape)
        shape["roof_bands"][0]["slope_v"] = 1
        shape["roof_bands"][1].update(slope_v=-1, height_at_v0_m=4)
        with self.assertRaises(ValueError):
            shape_planes(shape)

    def test_bounds_and_ray_budget_fail_closed(self):
        with self.assertRaises(ValueError):
            build(np.array([[[1, 0, 0], [0, 1, 1]]]))
        h = build(np.array([[[-1, -1, -1], [1, 1, 1]]]))
        with self.assertRaises(ValueError):
            trace(None, h, np.zeros((1, 3)), np.zeros((1, 3)))


if __name__ == "__main__":
    unittest.main()
