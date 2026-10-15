"""Independent full-port, derivative and rational-bound counterexamples for #169."""

import unittest

import numpy as np
from port_plane_contacts import contact_audit
from port_plane_field import PlaneField, control_bounds
from port_plane_layout import prepare_plane_field
from port_plane_system import assemble, fit_generated, fit_planes


def field(skew=0.0):
    right = np.array([-1.0, -skew])
    right /= np.linalg.norm(right)
    return PlaneField(
        [[0, 0], [100, 0]], [[0, 1]], [[[0, 0], [100, 0]]], [[[1, 0], right]], [100]
    )


def subdivide(triangles):
    a, b, c = triangles.transpose(1, 0, 2)
    ab, bc, ca = (a + b) / 2, (b + c) / 2, (c + a) / 2
    return np.concatenate(
        [
            np.stack(p, axis=1)
            for p in ((a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca))
        ]
    )


def shared_strip():
    xy = np.array([[x, y] for x in (-10, 0, 100, 110) for y in (-4, 4)], float)
    faces = np.array(
        [[a, a + 2, a + 1] for a in (0, 2, 4)]
        + [[a + 2, a + 3, a + 1] for a in (0, 2, 4)]
    )
    return {
        "vertices": xy,
        "indices": faces,
        "face_owners": np.tile([0, 1, 2], 2),
        "part_roles": np.array([1, 0, 1]),
        "port_edges": np.array([[2, 3], [4, 5]]),
        "port_parts": np.array([[0, 1], [1, 2]]),
    }


class PortPlaneChecks(unittest.TestCase):
    def test_global_cone_and_lp_preserve_feasible_sloped_shared_contacts(self):
        product = shared_strip()
        model, _ = prepare_plane_field(product)
        xy, faces, owners = (
            product[key] for key in ("vertices", "indices", "face_owners")
        )
        frames = np.tile(np.diag([1 / 0.08, 1 / 0.025]), (len(faces), 1, 1))
        frames[product["part_roles"][owners] == 1] = np.eye(2) / 0.025
        h, g, equal, _parts, system = assemble(model, xy, faces, owners, frames)
        target = 7 + xy @ [0.02, 0.01]
        for method in ("cone", "lp", "generated-cone"):
            if method == "generated-cone":
                coefficients, report = fit_generated(h, g, equal, target, system)
            else:
                coefficients, report = fit_planes(h, g, equal, target, method=method)
            self.assertTrue(report["valid"], report)
            result = h @ coefficients + report["datum_m"]
            np.testing.assert_allclose(result, target, rtol=0, atol=1e-7)
            note = contact_audit(model, coefficients, product, result)
            self.assertEqual(note["complete_original_ports"], 2)
            self.assertLessEqual(note["maximum_contact_grade_error"], 1e-10)

    def test_steep_targets_are_adjusted_without_widening_the_original_mesh(self):
        product = shared_strip()
        xy, faces, owners = (
            product[key] for key in ("vertices", "indices", "face_owners")
        )
        original = xy.copy()
        model, _ = prepare_plane_field(product)
        frames = np.tile(np.diag([1 / 0.08, 1 / 0.025]), (len(faces), 1, 1))
        frames[product["part_roles"][owners] == 1] = np.eye(2) / 0.025
        h, g, equal, _parts, _ = assemble(model, xy, faces, owners, frames)
        target = np.where(xy[:, 0] >= 100, 30.0, 0.0)
        coefficients, report = fit_planes(h, g, equal, target)
        self.assertTrue(report["valid"], report)
        self.assertGreater(report["maximum_target_deviation_m"], 10)
        points = np.c_[np.linspace(0, 100, 1001), np.full(1001, 3.0)]
        _, gradients = model.evaluate(points, np.ones(1001, int), coefficients)
        self.assertLessEqual(
            np.linalg.norm(gradients @ np.diag([1 / 0.08, 1 / 0.025]), axis=1).max(),
            1.0000001,
        )
        np.testing.assert_array_equal(xy, original)

    def test_zero_solver_budget_precedes_matrix_and_target_conversion(self):
        coefficients, report = fit_planes(None, None, None, None, seconds=0)
        self.assertIsNone(coefficients)
        self.assertEqual(report["reason"], "time_budget_exhausted")

    def test_entire_skew_contacts_match_both_parent_gradients(self):
        for skew in (0.0, 0.2, -0.3):
            model = field(skew)
            values = np.array([2, 0.02, 0.015, 5, -0.01, -0.005])
            for side in (0, 1):
                normal = model.inward_normals[0, side]
                tangent = np.array([-normal[1], normal[0]])
                points = (
                    model.cut_centres[0, side]
                    + np.linspace(-6, 6, 97)[:, None] * tangent
                )
                height, grade = model.evaluate(
                    points, np.zeros(len(points), int), values
                )
                coefficients = values[side * 3 : side * 3 + 3]
                expected = (
                    coefficients[0] + (points - model.origins[side]) @ coefficients[1:]
                )
                np.testing.assert_allclose(height, expected, atol=1e-12, rtol=0)
                np.testing.assert_allclose(
                    grade,
                    np.broadcast_to(coefficients[1:], grade.shape),
                    atol=1e-12,
                    rtol=0,
                )

    def test_interior_gradient_matches_independent_central_differences(self):
        model = field(0.2)
        coefficients = [2, 0.02, 0.015, 5, -0.01, -0.005]
        rng = np.random.default_rng(169)
        points = rng.uniform([5, -5], [90, 5], size=(97, 2))
        owners = np.zeros(len(points), int)
        _, gradients = model.evaluate(points, owners, coefficients)
        for coordinate in (0, 1):
            shift = np.eye(2)[coordinate] * 1e-5
            before, _ = model.evaluate(points - shift, owners, coefficients)
            after, _ = model.evaluate(points + shift, owners, coefficients)
            np.testing.assert_allclose(
                (after - before) / 2e-5, gradients[:, coordinate], atol=1e-9, rtol=0
            )

    def test_one_world_plane_is_retained_across_different_local_origins(self):
        model = field(0.2)
        coefficients = [3, 0.02, -0.01, 5, 0.02, -0.01]
        points = np.array([[0, -4], [70, -4], [90, 4]])
        height, gradients = model.evaluate(points, [0, 0, 0], coefficients)
        np.testing.assert_allclose(height, 3 + points @ [0.02, -0.01], atol=1e-12)
        np.testing.assert_allclose(
            gradients, np.tile([0.02, -0.01], (3, 1)), atol=1e-12
        )
        _, error = control_bounds(
            model, points[None], [0], coefficients, np.eye(2)[None]
        )
        self.assertLess(error[0], 1e-10)

    def test_interior_grade_extremum_is_bounded_when_corners_miss_it(self):
        model, coefficients = field(), [0, 0, 0, 3, 0, 0]
        triangle = np.array([[[0, -4], [100, -4], [100, 4]]])
        maximum, _ = control_bounds(model, triangle, [0], coefficients, np.eye(2)[None])
        x = np.linspace(0, 100, 1001)
        height, gradients = model.evaluate(
            np.c_[x, np.zeros(len(x))], np.zeros(len(x), int), coefficients
        )
        np.testing.assert_allclose(
            height, 3 * (3 * (x / 100) ** 2 - 2 * (x / 100) ** 3), atol=1e-12
        )
        self.assertAlmostEqual(gradients[:, 0].max(), 0.045)
        self.assertGreaterEqual(maximum[0] + 1e-12, 0.045)
        np.testing.assert_allclose(gradients[[0, -1]], 0, atol=1e-12)

    def test_rational_certificate_bounds_true_interpolant_error_and_refines(self):
        model = field(0.2)
        coefficients = [2, 0.02, 0.015, 5, -0.01, -0.005]
        triangles = np.array([[[0, -4], [80, -4], [80, 4]]])
        rng = np.random.default_rng(170)
        weights = rng.dirichlet(np.ones(3), size=1001)
        old = np.inf
        for _ in range(5):
            parts = np.zeros(len(triangles), int)
            _, bound = control_bounds(
                model,
                triangles,
                parts,
                coefficients,
                np.tile(np.eye(2), (len(parts), 1, 1)),
            )
            corners, _ = model.evaluate(
                triangles.reshape(-1, 2), np.repeat(parts, 3), coefficients
            )
            points = np.einsum("ki,nij->nkj", weights, triangles)
            actual, _ = model.evaluate(
                points.reshape(-1, 2), np.repeat(parts, len(weights)), coefficients
            )
            interpolated = np.einsum("ki,ni->nk", weights, corners.reshape(-1, 3))
            errors = abs(actual.reshape(-1, len(weights)) - interpolated).max(axis=1)
            self.assertTrue(np.all(errors <= bound + 1e-12))
            self.assertLessEqual(bound.max(), old + 1e-12)
            old = bound.max()
            triangles = subdivide(triangles)
        self.assertLess(old, 0.01)

    def test_nonpositive_denominator_is_rejected(self):
        model = field()
        model.inward_normals[0, 1] = [1, 0]
        with self.assertRaises(ValueError):
            model.evaluate([[50, 0]], [0], [0, 0, 0, 3, 0, 0])

    def test_rigid_frame_change_preserves_heights_gradients_and_bounds(self):
        model = field(0.2)
        coefficients = np.array([2, 0.02, 0.015, 5, -0.01, -0.005])
        triangle = np.array([[[0, -4], [80, -4], [80, 4]]])
        for angle in (0.7, 2.5):
            rotation = np.array(
                [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
            )
            offset = [700, -1500]
            changed = PlaneField(
                model.origins @ rotation.T + offset,
                model.plane_ids,
                model.cut_centres @ rotation.T + offset,
                model.inward_normals @ rotation.T,
                model.scales,
            )
            values = coefficients.reshape(2, 3).copy()
            values[:, 1:] = values[:, 1:] @ rotation.T
            points = triangle[0]
            a, g = model.evaluate(points, [0, 0, 0], coefficients)
            b, h = changed.evaluate(
                points @ rotation.T + offset, [0, 0, 0], values.ravel()
            )
            np.testing.assert_allclose(a, b, atol=1e-12)
            np.testing.assert_allclose(g @ rotation.T, h, atol=1e-12)
            left = control_bounds(
                model, triangle, [0], coefficients, np.eye(2)[None] / 0.08
            )
            right = control_bounds(
                changed,
                triangle @ rotation.T + offset,
                [0],
                values.ravel(),
                rotation.T[None] / 0.08,
            )
            for a, b in zip(left, right):
                np.testing.assert_allclose(a, b, atol=1e-10, rtol=0)


if __name__ == "__main__":
    unittest.main()
