"""Coordinate candidates are compared with the unchanged independent field."""

import unittest

import numpy as np
from cloud_field import Layer
from cloud_shape_field import extinction
from cloud_wind_coordinates import density
from cloud_wind_precision import MONTH_S, VELOCITY, points


class WindCoordinates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.points = points(512)[:, :3].astype(float)

    def test_split_preserves_both_original_profiles_at_large_signed_phases(self):
        for profile, kind in ((0, "stratus"), (1, "cell-smooth")):
            for cover in (0.0, 0.45, 0.65, 1.0):
                layer = Layer(cover=cover)
                for phase in (37.0, MONTH_S, -MONTH_S, 1792022400.0):
                    expected = extinction(
                        self.points, layer, phase, kind, drift_m=VELOCITY * phase
                    )
                    actual = density(
                        self.points, layer, VELOCITY, phase, "split", profile
                    )
                    with self.subTest(profile=profile, cover=cover, phase=phase):
                        self.assertLessEqual(
                            float(np.max(abs(actual - expected)))
                            / layer.extinction_per_m,
                            1e-4,
                        )

    def test_continuous_small_steps_survive_long_phases_in_both_directions(self):
        layer = Layer(cover=0.65)
        for velocity in (VELOCITY, -VELOCITY, np.array([6.92820323027551, 0.0, -4.0])):
            for rate in (30, 60):
                actual = [
                    density(self.points, layer, velocity, phase, "split", 1)
                    for phase in (MONTH_S, MONTH_S + 1 / rate)
                ]
                expected = [
                    extinction(
                        self.points,
                        layer,
                        phase,
                        "cell-smooth",
                        drift_m=velocity * phase,
                    )
                    for phase in (MONTH_S, MONTH_S + 1 / rate)
                ]
                self.assertGreater(np.count_nonzero(actual[0] != actual[1]), 0)
                self.assertLessEqual(
                    float(
                        np.max(
                            abs((actual[1] - actual[0]) - (expected[1] - expected[0]))
                        )
                    )
                    / layer.extinction_per_m,
                    1e-4,
                )

    def test_hourly_wrapping_does_not_preserve_the_same_field(self):
        layer = Layer(cover=0.65)
        expected = extinction(
            self.points, layer, MONTH_S, "cell-smooth", drift_m=VELOCITY * MONTH_S
        )
        wrapped = density(self.points, layer, VELOCITY, MONTH_S, "modulo-hour", 1)
        self.assertGreater(
            float(np.max(abs(wrapped - expected))) / layer.extinction_per_m, 0.1
        )

    def test_camera_anchor_changes_do_not_move_the_field(self):
        layer = Layer(cover=0.65)
        expected = extinction(
            self.points, layer, MONTH_S, "cell-smooth", drift_m=VELOCITY * MONTH_S
        )
        for origin in (
            np.zeros(3),
            np.array([768, -512, 1024]),
            np.array([-1400, 2800, -4200]),
        ):
            actual = density(self.points, layer, VELOCITY, MONTH_S, "split", 1, origin)
            self.assertLessEqual(
                float(np.max(abs(actual - expected))) / layer.extinction_per_m, 1e-4
            )


if __name__ == "__main__":
    unittest.main()
