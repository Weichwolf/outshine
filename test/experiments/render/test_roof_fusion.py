"""Joined-roof proofs against the existing independent exact segment oracle."""

import itertools
import unittest

import numpy as np
from building_roof_part_selection import contour_selection
from outshine_ref.geometry.occlusion import classify_boxes, convex_patch_umbra
from roof_fusion import fused_roof_umbra
from test_occlusion import scalar_triangle_hit


def mesh(warp=2**-30):
    vertices = np.array(
        [[-2, -2, 0], [2, -2, 0], [2, 2, warp], [-2, 2, 0]], dtype=float
    )
    return vertices[[[0, 1, 2], [0, 2, 3]]]


OBSERVER = np.array([[-0.1, -0.1, 4], [0.1, 0.1, 5]])
TARGET = np.array([[[-0.2, -0.2, -2], [0.2, 0.2, -1]]])


class RoofFusion(unittest.TestCase):
    def test_concave_boundary_uses_inner_kernel_without_filling_notch(self):
        vertices = np.array(
            [[-2, -2, 0], [2, -2, 0], [2, 0, 0], [0, 0, 0], [0, 2, 0], [-2, 2, 0]],
            float,
        )
        triangles = vertices[[[0, 1, 3], [1, 2, 3], [0, 3, 5], [3, 4, 5]]]
        eyes = np.array([[-1.1, -1.1, 4], [-0.9, -0.9, 5]])
        inside = np.array([[[-1.2, -1.2, -2], [-0.8, -0.8, -1]]])
        certificate = fused_roof_umbra(triangles, eyes, certified_opaque=True)
        self.assertTrue(classify_boxes(inside, certificate)[0][0])
        void = np.array([[[1, 1, -1], [1, 1, -1]]])
        self.assertFalse(classify_boxes(void, certificate)[0][0])
        self.assertFalse(
            any(
                scalar_triangle_hit([-1, -1, 4], void[0, 0], triangle)
                for triangle in triangles
            )
        )
        for eye in itertools.product(*zip(*eyes, strict=True)):
            for target in itertools.product(*zip(*inside[0], strict=True)):
                self.assertTrue(
                    any(
                        scalar_triangle_hit(eye, target, triangle)
                        for triangle in triangles
                    )
                )

    def test_self_crossing_positive_triangle_chain_is_rejected(self):
        vertices = np.array(
            [[0, 3, 0], [3, 1, 0], [2, -3, 0], [-2, -3, 0], [-3, 1, 0], [0, 0, 0]],
            float,
        )
        ring = [0, 3, 1, 4, 2]
        triangles = vertices[[[5, ring[i], ring[(i + 1) % 5]] for i in range(5)]]
        with self.assertRaisesRegex(ValueError, "self-intersecting"):
            fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)

    def test_contour_budget_retains_every_unproved_target(self):
        certificate = fused_roof_umbra(mesh(), OBSERVER, certified_opaque=True)
        rows = [
            {
                "bounds": TARGET[0],
                "support": np.array(
                    list(itertools.product(*zip(*TARGET[0], strict=True)))
                ),
            }
        ]
        packet = [(certificate, None)]
        no_work, stopped = contour_selection(
            rows, packet, np.array([False]), max_tests=0
        )
        self.assertFalse(no_work.any())
        self.assertTrue(stopped["budget_exhausted"])
        result, work = contour_selection(rows, packet, np.array([False]), max_tests=512)
        self.assertTrue(result[0])
        self.assertFalse(work["budget_exhausted"])
        self.assertLessEqual(work["box_tests"] + work["contour_point_tests"], 512)

    def test_negative_dominant_axes_preserve_front_orientation(self):
        for matrix in (
            np.array([[0, 0, -1], [0, 1, 0], [1, 0, 0]]),
            np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]]),
            np.diag([1, -1, -1]),
        ):
            triangles = mesh() @ matrix.T

            def box_bounds(box, transform):
                points = (
                    np.array(list(itertools.product(*zip(*box, strict=True))))
                    @ transform.T
                )
                return np.array([points.min(axis=0), points.max(axis=0)])

            eye_box, target_box = (
                box_bounds(OBSERVER, matrix),
                box_bounds(TARGET[0], matrix),
            )
            certificate = fused_roof_umbra(triangles, eye_box, certified_opaque=True)
            self.assertTrue(classify_boxes(target_box[None], certificate)[0][0])
            for eye in itertools.product(*zip(*eye_box, strict=True)):
                for target in itertools.product(*zip(*target_box, strict=True)):
                    self.assertTrue(
                        any(
                            scalar_triangle_hit(eye, target, triangle)
                            for triangle in triangles
                        )
                    )

    def test_piecewise_graph_with_an_interior_vertex(self):
        vertices = np.array(
            [[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0], [0, 0, 0.5]]
        )
        triangles = vertices[[[0, 1, 4], [1, 2, 4], [2, 3, 4], [3, 0, 4]]]
        certificate = fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
        self.assertTrue(classify_boxes(TARGET, certificate)[0][0])
        for eye in itertools.product(*zip(*OBSERVER, strict=True)):
            for target in itertools.product(*zip(*TARGET[0], strict=True)):
                self.assertTrue(
                    any(
                        scalar_triangle_hit(eye, target, triangle)
                        for triangle in triangles
                    )
                )

    def test_fusion_closes_artificial_triangle_diagonal_gap(self):
        triangles = mesh()
        self.assertTrue(
            classify_boxes(
                TARGET, fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
            )[0][0]
        )
        for triangle in triangles:
            self.assertFalse(
                classify_boxes(
                    TARGET,
                    convex_patch_umbra(triangle, OBSERVER, certified_opaque=True),
                )[0][0]
            )

    def test_exact_segments_from_corners_and_interior(self):
        triangles = mesh()
        certificate = fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
        self.assertTrue(classify_boxes(TARGET, certificate)[0][0])
        eyes = itertools.product(
            *[(a, (a + b) / 2, b) for a, b in zip(*OBSERVER, strict=True)]
        )
        targets = list(itertools.product(*zip(*TARGET[0], strict=True))) + [
            TARGET[0].mean(axis=0)
        ]
        for eye in eyes:
            for target in targets:
                self.assertTrue(
                    any(
                        scalar_triangle_hit(eye, target, triangle)
                        for triangle in triangles
                    )
                )

    def test_missing_triangle_does_not_fill_diagonal(self):
        certificate = fused_roof_umbra(mesh()[:1], OBSERVER, certified_opaque=True)
        self.assertFalse(classify_boxes(TARGET, certificate)[0][0])

    def test_courtyard_remains_a_real_hole(self):
        outer = np.array([[-2, -2, 0], [2, -2, 0], [2, 2, 0], [-2, 2, 0]], float)
        inner = outer / 4
        triangles = []
        for i in range(4):
            j = (i + 1) % 4
            triangles.extend(
                ([outer[i], outer[j], inner[j]], [outer[i], inner[j], inner[i]])
            )
        with self.assertRaisesRegex(ValueError, "holes|single bounded"):
            fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
        self.assertFalse(
            any(
                scalar_triangle_hit([0, 0, 4], [0, 0, -1], triangle)
                for triangle in triangles
            )
        )

    def test_subnanometre_crack_is_not_filled(self):
        gap = 2**-42
        triangles = []
        for low, high in ((-2, -gap), (gap, 2)):
            vertices = np.array(
                [[low, -2, 0], [high, -2, 0], [high, 2, 0], [low, 2, 0]], float
            )
            triangles.extend(vertices[[[0, 1, 2], [0, 2, 3]]])
        with self.assertRaisesRegex(ValueError, "holes|single bounded"):
            fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)
        self.assertFalse(
            any(
                scalar_triangle_hit([0, 0, 4], [0, 0, -1], triangle)
                for triangle in triangles
            )
        )

    def test_duplicate_and_reversed_faces_are_rejected(self):
        triangles = mesh()
        with self.assertRaisesRegex(ValueError, "nonmanifold|duplicate"):
            fused_roof_umbra(
                np.concatenate((triangles, triangles[:1])),
                OBSERVER,
                certified_opaque=True,
            )
        triangles[1] = triangles[1, ::-1]
        with self.assertRaisesRegex(ValueError, "consistently oriented"):
            fused_roof_umbra(triangles, OBSERVER, certified_opaque=True)

    def test_backside_and_crossing_observer_boxes_hide_nothing(self):
        for box in (
            [[-0.1, -0.1, -5], [0.1, 0.1, -4]],
            [[-0.1, -0.1, -1], [0.1, 0.1, 1]],
        ):
            certificate = fused_roof_umbra(mesh(), box, certified_opaque=True)
            self.assertFalse(classify_boxes(TARGET, certificate)[0].any())

    def test_unknown_opacity_and_above_roof_targets_stay_visible(self):
        self.assertFalse(classify_boxes(TARGET, fused_roof_umbra(None, None))[0].any())
        above = np.array([[[-0.1, -0.1, 1], [0.1, 0.1, 2]]])
        certificate = fused_roof_umbra(mesh(), OBSERVER, certified_opaque=True)
        self.assertFalse(classify_boxes(above, certificate)[0].any())

    def test_rotated_translated_certificate_and_exact_segments(self):
        angle = 0.47
        matrix = np.array(
            [
                [np.cos(angle), 0, np.sin(angle)],
                [0, 1, 0],
                [-np.sin(angle), 0, np.cos(angle)],
            ]
        )
        shift = np.array([4e6, -3e6, 2e6])
        triangles = mesh() @ matrix.T + shift

        def transformed_box(box):
            points = (
                np.array(list(itertools.product(*zip(*box, strict=True)))) @ matrix.T
                + shift
            )
            return np.array([points.min(axis=0), points.max(axis=0)])

        eye_box = transformed_box(OBSERVER)
        target_box = transformed_box(np.array([[-0.01, -0.01, -2], [0.01, 0.01, -1.9]]))
        certificate = fused_roof_umbra(triangles, eye_box, certified_opaque=True)
        self.assertTrue(classify_boxes(target_box[None], certificate)[0][0])
        for eye in itertools.product(*zip(*eye_box, strict=True)):
            for target in itertools.product(*zip(*target_box, strict=True)):
                self.assertTrue(
                    any(
                        scalar_triangle_hit(eye, target, triangle)
                        for triangle in triangles
                    )
                )


if __name__ == "__main__":
    unittest.main()
