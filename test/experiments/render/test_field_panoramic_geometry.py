import math
import unittest
from itertools import product

import numpy as np

from field_panoramic_geometry import panoramic_bound
from field_view_projection import CapturePlane
from test_field_pose_geometry import exact_error, perspective


class PanoramicFieldTests(unittest.TestCase):
    def test_rotations_at_edges_and_interior_of_the_complete_capture(self):
        rng = np.random.default_rng(1955)
        plane = CapturePlane((-60, -30, -120), (60, 30, 120), (0, 0, 0), (0, 0, 1))
        count = 0
        for distance, side, fit, jitter in product((2000., 64000.), (0., 800.),
                                                  ((0, 0, 1), (.2, -.3, 1)),
                                                  ((0, 0), (.001, -.002))):
            eye = np.array((side, -side / 4, distance))
            matrix = perspective(jitter=jitter)
            bound = panoramic_bound(plane, eye, matrix, (1280, 720), normal=fit)
            self.assertTrue(bound['depth_inside'])
            points = np.array([*product(*zip(plane.lower, plane.upper)),
                               *rng.uniform(plane.lower, plane.upper, (4, 3))])
            for yaw, pitch, roll in product((-.65, -.4, 0, .4, .65, math.pi),
                                           (-.3, 0, .3), (0, .8)):
                forward = np.array((math.sin(yaw) * math.cos(pitch), math.sin(pitch),
                                    -math.cos(yaw) * math.cos(pitch)))
                right = np.cross(forward, (0, 1, 0))
                right /= np.linalg.norm(right)
                up = np.cross(right, forward)
                basis = (right * math.cos(roll) + up * math.sin(roll),
                         up * math.cos(roll) - right * math.sin(roll), forward)
                for p in points:
                    h = np.dot(p, fit) / np.dot(plane.toward_eye, fit)
                    q = p - h * np.array(plane.toward_eye)
                    clips = [matrix @ np.array((*[np.dot(a, v - eye) for a in basis], 1))
                             for v in (p, q)]
                    visible = any(c[3] > 0 and abs(c[0]) <= c[3] and abs(c[1]) <= c[3]
                                  and 0 <= c[2] <= c[3] for c in clips)
                    if not visible:
                        continue
                    error = exact_error(plane, p, eye, basis, matrix, (1280, 720), fit)
                    self.assertLessEqual(float(sum(error)), bound['screen_l1_upper_px'])
                    count += 1
        self.assertGreater(count, 1000)

    def test_near_eye_clipping_backside_and_invalid_projection(self):
        plane = CapturePlane((-1, -1, -2), (1, 1, 2), (0, 0, 0), (0, 0, 1))
        matrix = perspective()
        self.assertFalse(panoramic_bound(plane, (0, 0, 3), matrix, (1280, 720))['depth_inside'])
        with self.assertRaises(ValueError):
            panoramic_bound(plane, (0, 0, -10), matrix, (1280, 720))
        matrix[3][3] = 1
        with self.assertRaises(ValueError):
            panoramic_bound(plane, (0, 0, 10), matrix, (1280, 720))


if __name__ == '__main__':
    unittest.main()
