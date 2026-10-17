"""Analytic occlusion, arbitrary orientation and covariance counterexamples."""

import unittest

import numpy as np
from needle_joint_contacts import camera_contacts, direct_response, filtered_fields
from scatter_reference import Float64Nearest
from scatter_scene import ScatterScene


def plane(z, xmax=1):
    vertices = np.array([[0.0, 0.0, z], [xmax, 0, z], [xmax, 1, z], [0, 1, z]])
    faces = np.array([[0, 1, 2], [0, 2, 3]])
    return vertices, faces


class NeedleJointChecks(unittest.TestCase):
    def test_common_visibility_before_filter_not_product_of_means(self):
        contacts = {"size": 2, "pixels": np.array([0, 1])}
        result = filtered_fields(
            contacts, np.array([1.0, 0.5]), np.array([False, True]), 2
        )
        self.assertEqual(result[0, 0, 0], 0.375)
        self.assertEqual(result[1, 0, 0], 0.125)
        self.assertEqual(result[2, 0, 0], 0.1875)

    def test_half_blocker_and_existing_independent_float64(self):
        vertices, faces = plane(0.5, 0.5)
        scene = ScatterScene(vertices, faces)
        contacts = {
            "points": np.array([[0.2, 0.3, 0], [0.8, 0.4, 0]]),
            "normals": np.tile([0.0, 0.0, 1.0], (2, 1)),
        }
        cosine, visible, work = direct_response(
            scene, contacts, np.array([0, 0, 1.0]), packet=1
        )
        np.testing.assert_array_equal(visible, [False, True])
        np.testing.assert_array_equal(cosine, [1, 1])
        ids, _, _ = Float64Nearest(vertices, faces).query(
            contacts["points"] + [0, 0, 2e-6], np.tile([0.0, 0.0, 1.0], (2, 1)), 2
        )
        np.testing.assert_array_equal(visible, ids < 0)
        self.assertEqual(work["rays"], 2)
        _, packet, _ = direct_response(scene, contacts, np.array([0, 0, 1.0]), packet=2)
        np.testing.assert_array_equal(packet, visible)

    def test_downward_light_and_no_nonpositive_work(self):
        vertices, faces = plane(-0.5, 0.5)
        scene = ScatterScene(vertices, faces)
        contacts = {
            "points": np.array([[0.2, 0.3, 0], [0.8, 0.4, 0]]),
            "normals": np.tile([0.0, 0.0, -1.0], (2, 1)),
        }
        _, visible, _ = direct_response(scene, contacts, np.array([0, 0, -1.0]))
        np.testing.assert_array_equal(visible, [False, True])
        cosine, visible, work = direct_response(scene, contacts, np.array([0, 0, 1.0]))
        self.assertEqual(work["rays"], 0)
        np.testing.assert_array_equal(cosine, [0, 0])
        self.assertFalse(visible.any())

    def test_frontmost_camera_and_original_corner_normals(self):
        vertices, faces = plane(0)
        upper, upperfaces = plane(0.5)
        source = {
            "vertices": np.r_[vertices, upper],
            "faces": np.r_[faces, upperfaces + 4],
            "normals": np.tile([0.0, 0.0, 1.0], (4, 3, 1)),
        }
        scene = ScatterScene(source["vertices"], source["faces"])
        contacts, work = camera_contacts(
            source, scene, np.array([0, 0, 1.0]), 16, packet=7
        )
        self.assertTrue(np.all(contacts["faces"] >= 2))
        np.testing.assert_allclose(contacts["points"][:, 2], 0.5, atol=1e-14)
        np.testing.assert_allclose(
            contacts["normals"], np.tile([0, 0, 1], (len(contacts["points"]), 1))
        )
        self.assertEqual(work["rays"], 256)

    def test_rotation_preserves_actual_visibility(self):
        vertices, faces = plane(0.5, 0.5)
        rotation = np.array([[0.0, 1, 0], [0, 0, 1], [1, 0, 0]])
        contacts = {
            "points": np.array([[0.2, 0.3, 0], [0.8, 0.4, 0]]) @ rotation,
            "normals": np.tile([0.0, 0.0, 1.0], (2, 1)) @ rotation,
        }
        scene = ScatterScene(vertices @ rotation, faces)
        _, visible, _ = direct_response(
            scene, contacts, np.array([0.0, 0.0, 1.0]) @ rotation
        )
        np.testing.assert_array_equal(visible, [False, True])


if __name__ == "__main__":
    unittest.main()
