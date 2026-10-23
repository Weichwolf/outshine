import unittest

import numpy as np
import shapely
from mvt_t_profile_products import whole_section
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


def rectangles(bounds):
    vertices, faces = [], []
    for low, high in bounds:
        start = len(vertices)
        vertices.extend([[low, -1, 0], [high, -1, 0], [high, 1, 0], [low, 1, 0]])
        faces.extend(np.array([[0, 1, 2], [0, 2, 3]]) + start)
    return np.array(vertices, float), np.array(faces)


def section(vertices, faces, line, *, tolerance=0, extra=None):
    sampler = SurfaceSampler(Mesh(vertices, faces), xy_tolerance_m=tolerance)
    return whole_section(
        shapely.LineString(line),
        shapely.polygons(vertices[faces, :2]),
        sampler,
        np.array(line if extra is None else extra),
    )


class WholeProfileCoverageTests(unittest.TestCase):
    def test_valid_boundary_samples_do_not_bridge_a_ten_centimetre_hole(self):
        vertices, faces = rectangles([(-2, -0.05), (0.05, 2)])
        line = np.array([[-1.0, 0.0], [1.0, 0.0]])
        sampler = SurfaceSampler(Mesh(vertices, faces), xy_tolerance_m=0)
        valid_points = np.array([[-1.0, 0.0], [-0.05, 0.0], [0.05, 0.0], [1.0, 0.0]])
        self.assertFalse(any(sampler.sample(valid_points).reasons))
        self.assertAlmostEqual(
            shapely.LineString(line)
            .difference(shapely.union_all(shapely.polygons(vertices[faces, :2])))
            .length,
            0.1,
        )
        with self.assertRaisesRegex(ValueError, "unsupported intervals"):
            section(vertices, faces, line)

    def test_unqueried_profile_ends_cannot_be_silently_truncated(self):
        vertices, faces = rectangles([(-2, 2)])
        with self.assertRaisesRegex(ValueError, "unsupported intervals"):
            section(vertices, faces, [[-3.0, 0.0], [3.0, 0.0]], extra=[[0.0, 0.0]])

    def test_complete_bent_profile_keeps_all_requested_knots_and_affine_heights(self):
        vertices, faces = rectangles([(-2, 2)])
        vertices[:, 2] = 3 + 0.2 * vertices[:, 0] - 0.3 * vertices[:, 1]
        line = np.array([[-1.0, -0.25], [0.0, 0.5], [1.0, -0.25]])
        result = np.array(section(vertices, faces, line))
        for point in line:
            self.assertTrue(np.any(np.all(result[:, :2] == point, axis=1)))
        np.testing.assert_allclose(
            result[:, 2],
            3 + 0.2 * result[:, 0] - 0.3 * result[:, 1],
            rtol=0,
            atol=1e-14,
        )
        reversed_profile = np.array(section(vertices, faces, line[::-1]))
        np.testing.assert_array_equal(reversed_profile, result[::-1])

    def test_existing_explicit_metric_tolerance_is_respected_without_expansion(self):
        vertices, faces = rectangles([(-2, 2)])
        for offset, tolerance, accepted in (
            (0.0, 0.0, True),
            (0.5e-12, 1e-12, True),
            (2e-12, 1e-12, False),
            (0.5e-12, 0.0, False),
        ):
            line = [[-1.0, 1.0 + offset], [1.0, 1.0 + offset]]
            with self.subTest(offset=offset, tolerance=tolerance):
                if accepted:
                    result = np.array(
                        section(vertices, faces, line, tolerance=tolerance)
                    )
                    np.testing.assert_array_equal(result[:, :2], line)
                else:
                    with self.assertRaisesRegex(ValueError, "unsupported intervals"):
                        section(vertices, faces, line, tolerance=tolerance)

    def test_far_faces_do_not_change_profile_points_or_heights(self):
        vertices, faces = rectangles([(-2, 2)])
        line = [[-1.0, 0.0], [1.0, 0.0]]
        expected = section(vertices, faces, line)
        distant_vertices, distant_faces = rectangles(
            [(100.0 + 4 * i, 102.0 + 4 * i) for i in range(64)]
        )
        actual = section(
            np.vstack([vertices, distant_vertices]),
            np.vstack([faces, distant_faces + len(vertices)]),
            line,
        )
        np.testing.assert_array_equal(actual, expected)

    def test_disconnected_bent_profile_rejects_missing_middle_segment(self):
        vertices, faces = rectangles([(-2, -0.05), (0.05, 2)])
        line = [[-1.0, -0.5], [-0.5, 0.5], [0.5, 0.5], [1.0, -0.5]]
        with self.assertRaisesRegex(ValueError, "unsupported intervals"):
            section(vertices, faces, line)


if __name__ == "__main__":
    unittest.main()
