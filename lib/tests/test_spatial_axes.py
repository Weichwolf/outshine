"""Independent spatial sweep math, frame invariants and prepared-plan ownership."""

import json
import subprocess
import sys
import unittest

import numpy as np
from outshine_ref.vegetation import (
    ArcMeasure,
    CubicSpine,
    Profile,
    SpatialAxis,
    SpineField,
)
from outshine_ref.vegetation.spine_frames import transport_frames


def straight_spine(length=1):
    return CubicSpine.from_hermite(
        [[0, 0, 0], [0, length, 0]], [[0, length, 0], [0, length, 0]]
    )


def example_axis():
    t = np.linspace(0, 1, 9)
    p = np.column_stack((0.12 * np.sin(np.pi * t) ** 2, 1.5 * t, 0.1 * t**2))
    d = np.column_stack((0.12 * np.pi * np.sin(2 * np.pi * t), 1.5 + 0 * t, 0.2 * t))
    return SpatialAxis(
        CubicSpine.from_hermite(p, d),
        Profile(np.linspace(0.012, 0.005, 4)),
        Profile(np.linspace(0, 0.3, 4)),
        (1.1, 0.8),
    )


class SpatialAxisTests(unittest.TestCase):
    def test_linear_axis_has_exact_metric_frame_and_polygonal_volume(self):
        axis = SpatialAxis(
            straight_spine(2), Profile(np.full(4, 0.04)), ellipse=(1, 0.7)
        )
        u = np.linspace(0, 1, 37)
        self.assertAlmostEqual(axis.length, 2, places=14)
        np.testing.assert_allclose(axis.measure.parameter(u), u, atol=4e-16)
        sample = axis.field.sample(u)
        np.testing.assert_allclose(sample["centre"][:, 1], 2 * u, atol=1e-15)
        np.testing.assert_array_equal(
            sample["tangent"], np.tile([0, 1, 0], (len(u), 1))
        )
        mesh = axis.mesh(16, 16)
        p = mesh["vertices"][mesh["faces"]]
        volume = np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6
        area = 16 / 2 * np.sin(2 * np.pi / 16) * 0.04**2 * 0.7
        self.assertAlmostEqual(volume, area * 2, places=14)

    def test_prepared_nodes_and_public_polynomials_cannot_poison_future_samples(self):
        axis = example_axis()
        expected = axis.field.sample(np.linspace(0, 1, 27))
        for nodes in (axis.measure.parameter_nodes, axis.measure.length_nodes):
            with self.assertRaises(ValueError):
                nodes.setflags(write=True)
        for polynomial in (
            axis.spine.polynomial,
            axis.measure.polynomial,
            axis.measure.inverse,
            axis.field.lateral_curve,
        ):
            polynomial.c[:] = 0
            polynomial.x[:] = 0
        actual = axis.field.sample(np.linspace(0, 1, 27))
        for key in expected:
            np.testing.assert_array_equal(actual[key], expected[key])

    def test_curved_surface_has_closed_oriented_edges_and_stable_nested_points(self):
        axis = example_axis()
        near, coarse = axis.mesh(48, 12), axis.mesh(12, 6)
        indices = np.arange(49 * 12).reshape(49, 12)[::4, ::2].ravel()
        indices = np.r_[indices, len(near["vertices"]) - 2, len(near["vertices"]) - 1]
        for name in ("vertices", "bark_coordinates", "bark_radius"):
            np.testing.assert_array_equal(coarse[name], near[name][indices])
        for mesh, sides in ((near, 12), (coarse, 6)):
            faces = mesh["faces"]
            edges = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
            signs = np.where(edges[:, 0] < edges[:, 1], 1, -1)
            _, inverse, counts = np.unique(
                np.sort(edges, axis=1), axis=0, return_inverse=True, return_counts=True
            )
            np.testing.assert_array_equal(counts, 2)
            np.testing.assert_array_equal(np.bincount(inverse, weights=signs), 0)
            p = mesh["vertices"][faces]
            cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
            corners = mesh["normals"].reshape(-1, 3, 3)
            self.assertTrue(np.all(np.sum(cross[:, None] * corners, axis=2) > 0))
            np.testing.assert_allclose(np.linalg.norm(corners, axis=2), 1, atol=2e-14)
            np.testing.assert_array_equal(
                corners[-2 * sides : -sides], np.tile([0, -1, 0], (sides, 3, 1))
            )

    def test_half_turn_aliases_and_incorrect_corner_orientation_are_rejected(self):
        axis = SpatialAxis(
            straight_spine(),
            Profile(np.full(4, 0.02)),
            Profile(np.linspace(0, 100, 4)),
            (1, 0.3),
        )
        for segments in (4, 8, 16):
            with self.assertRaisesRegex(ValueError, "cross-section roll"):
                axis.mesh(segments, 8)
        with self.assertRaisesRegex(ValueError, "oriented surface"):
            axis.mesh(64, 8)
        self.assertGreater(len(axis.mesh(256, 8)["faces"]), 0)
        oscillating = SpatialAxis(
            straight_spine(),
            Profile(np.full(4, 0.02)),
            Profile(np.array([0, 100, 0, 100, 0])),
        )
        with self.assertRaisesRegex(ValueError, "cross-section roll"):
            oscillating.mesh(2, 8)
        excessive = SpatialAxis(
            straight_spine(),
            Profile(np.full(4, 0.02)),
            Profile(np.array([0, 1e308, -1e308, 0])),
        )
        with (
            np.errstate(over="raise"),
            self.assertRaisesRegex(ValueError, "representable"),
        ):
            excessive.mesh()

    def test_transport_is_invariant_under_rigid_transform_and_reversal(self):
        t = np.linspace(0, 1, 65)
        spine = example_axis().spine
        p, tangent, _, _ = spine.sample(t)
        frames = transport_frames(p, tangent)
        matrix = np.array([[0, -1, 0], [1, 0, 0], [0, 0, 1]])
        moved = transport_frames(
            p @ matrix + [13, -27, 6], tangent @ matrix, [1, 0, 0] @ matrix
        )
        np.testing.assert_allclose(moved, frames @ matrix, atol=2e-13)
        backwards = transport_frames(p[::-1], -tangent[::-1], frames[-1, 0])
        np.testing.assert_allclose(backwards[:, 0], frames[::-1, 0], atol=2e-13)

    def test_reload_preserves_mesh_and_invalid_domains_are_refused(self):
        axis = example_axis()
        loaded = SpatialAxis.from_recipe(json.loads(json.dumps(axis.recipe())))
        reloaded = loaded.mesh()
        for name, values in axis.mesh().items():
            np.testing.assert_array_equal(reloaded[name], values)
        for f in (
            lambda: axis.spine.evaluate(0.5, True),
            lambda: axis.measure.evaluate(0.5, True),
            lambda: axis.mesh(True, 8),
            lambda: ArcMeasure(axis.spine, True),
            lambda: axis.field.sample(np.nan),
            lambda: axis.measure.parameter(1.01),
        ):
            with self.assertRaises(ValueError):
                f()
        with self.assertRaises(TypeError):
            ArcMeasure(None)
        with self.assertRaisesRegex(ValueError, "same curve"):
            SpineField(axis.spine, ArcMeasure(straight_spine()), axis.roll)

    def test_lightweight_profile_import_does_not_activate_spatial_scipy_dependencies(
        self,
    ):
        result = subprocess.run(
            [
                sys.executable,
                "-c",
                "import sys; from outshine_ref.vegetation import Profile; assert 'scipy' not in sys.modules",
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
