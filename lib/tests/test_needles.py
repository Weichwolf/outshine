"""Independent organ surface contracts, analytic geometry and counterexamples."""

import unittest

import numpy as np
from outshine_ref.vegetation import Needle
from outshine_ref.vegetation.curve_frame import arc_frame


class NeedleTests(unittest.TestCase):
    def test_circle_frame_is_arc_length_and_right_handed(self):
        t = np.linspace(0, 1, 67)
        for bend in (0, 1e-12, -1.3, np.pi):
            centre, tangent, lateral, normal = arc_frame(0.17, bend, 0.8, t)
            frames = np.stack((lateral, tangent, normal), axis=-1)
            np.testing.assert_allclose(np.linalg.det(frames), 1, atol=1e-14)
            np.testing.assert_allclose(
                frames.transpose(0, 2, 1) @ frames,
                np.broadcast_to(np.eye(3), frames.shape),
                atol=1e-14,
            )
            step = 1e-6
            derivative = (
                arc_frame(0.17, bend, 0.8, t + step)[0]
                - arc_frame(0.17, bend, 0.8, t - step)[0]
            ) / (2 * step)
            np.testing.assert_allclose(derivative, 0.17 * tangent, atol=5e-11)
            if bend == 0:
                np.testing.assert_array_equal(
                    centre, np.column_stack((t * 0, 0.17 * t, t * 0))
                )

    def test_buffers_are_closed_oriented_and_nested(self):
        for shape in (
            Needle(),
            Needle(section="rhombus"),
            Needle(section="sector", sector_count=2),
            Needle(section="sector", sector_count=5),
        ):
            near, coarse = shape.mesh(), shape.mesh(16, 8)
            profile = shape.mesh(4, 8, axial_sampling="profile")
            for mesh in (near, coarse, profile):
                self.assertEqual(mesh["vertices"].dtype, np.dtype(np.float64))
                self.assertEqual(mesh["faces"].dtype, np.dtype(np.int32))
                self.assertEqual(mesh["normals"].shape, (3 * len(mesh["faces"]), 3))
                edges = np.concatenate(
                    [
                        mesh["faces"][:, [0, 1]],
                        mesh["faces"][:, [1, 2]],
                        mesh["faces"][:, [2, 0]],
                    ]
                )
                unique, counts = np.unique(
                    np.sort(edges, axis=1), axis=0, return_counts=True
                )
                np.testing.assert_array_equal(counts, 2)
                self.assertEqual(
                    len(mesh["vertices"]) - len(unique) + len(mesh["faces"]), 2
                )
                p = mesh["vertices"][mesh["faces"]]
                cross = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
                self.assertTrue(
                    np.all(
                        np.sum(
                            cross[:, None] * mesh["normals"].reshape(-1, 3, 3), axis=2
                        )
                        > 0
                    )
                )
            self.assertTrue(
                all(
                    any(np.array_equal(p, q) for q in near["vertices"])
                    for p in coarse["vertices"]
                )
            )

    def test_profile_rings_preserve_peak_and_straight_projected_area(self):
        shape = Needle(section="rhombus", bend=0, twist=0)
        expected = (
            2
            * shape.length
            * shape.radius
            * (0.775 * shape.taper_start + (1 - shape.taper_start) * 2 / 3)
        )
        errors = []
        for sampling in ("uniform", "profile"):
            mesh = shape.mesh(4, 4, axial_sampling=sampling)
            # Along +X/-X the straight rhombus silhouette is a polygonal strip.
            boundary = mesh["vertices"][:-2].reshape(4, 4, 3)[:, 0]
            height = np.r_[boundary[:, 1], shape.length]
            width = np.r_[boundary[:, 0], 0]
            area = np.sum(np.diff(height) * (width[:-1] + width[1:]))
            errors.append(abs(area / expected - 1))
            if sampling == "profile":
                self.assertEqual(float(width.max()), shape.radius)
                self.assertEqual(float(height[width.argmax()]), 0.75 * shape.length)
                scalar = shape.mesh(4, 4, "scalar", axial_sampling=sampling)
                for name in mesh:
                    np.testing.assert_array_equal(mesh[name], scalar[name])
        self.assertLess(errors[1], 0.015)
        self.assertLess(errors[1], errors[0] / 3)

    def test_straight_sector_approaches_analytic_volume(self):
        shape = Needle(section="sector", sector_count=5, bend=0, twist=0)
        polynomial = np.polynomial.Polynomial([0.55, 0, 1.35, -0.9])
        integral = (polynomial * polynomial).integ()
        profile = (
            shape.taper_start * (integral(1) - integral(0))
            + (1 - shape.taper_start) * 8 / 15
        )
        expected = shape.length * shape.radius**2 * np.pi * shape.aspect / 5 * profile
        errors = []
        for segments, sides in ((16, 8), (32, 16), (128, 32)):
            mesh = shape.mesh(segments, sides)
            p = mesh["vertices"][mesh["faces"]]
            actual = (
                np.sum(np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2]))) / 6
            )
            errors.append(abs(actual / expected - 1))
        self.assertGreater(errors[0], errors[1])
        self.assertGreater(errors[1], errors[2])
        self.assertLess(errors[2], 0.002)

    def test_scaling_preserves_normals_and_changes_surface_units(self):
        source = Needle(section="sector", sector_count=3, bend=-0.7, twist=1.2)
        target = Needle(
            **(
                source.recipe()
                | {"length": 7 * source.length, "radius": 7 * source.radius}
            )
        )
        t, angle = np.linspace(0.03, 0.97, 53), np.full(53, 2.1)
        p, n, a = source.evaluate(t, angle)
        q, m, b = target.evaluate(t, angle)
        np.testing.assert_allclose(q, 7 * p, atol=1e-16)
        np.testing.assert_allclose(m, n, atol=1e-14)
        np.testing.assert_allclose(b, 49 * a, rtol=1e-14)

    def test_invalid_and_underresolved_inputs_are_rejected(self):
        for changes in (
            {"radius": 0},
            {"radius": 1},
            {"bend": np.nan},
            {"section": "unknown"},
            {"sector_count": 1},
            {"sector_count": True},
        ):
            with self.assertRaises(ValueError):
                Needle(**changes)
        for options in (
            {"segments": 1},
            {"sides": 7},
            {"method": "other"},
            {"axial_sampling": "other"},
        ):
            with self.assertRaises(ValueError):
                Needle().mesh(**options)
        shape = Needle(
            length=0.08,
            radius=0.01,
            section="sector",
            sector_count=2,
            bend=np.pi,
            twist=4 * np.pi,
        )
        with self.assertRaisesRegex(ValueError, "subdivision"):
            shape.mesh(16, 8)
        self.assertEqual(len(shape.mesh(32, 16)["faces"]), 640)


if __name__ == "__main__":
    unittest.main()
