from types import SimpleNamespace
from unittest import TestCase

import numpy as np
from shapely import box
from water_reflection import reflection_levels, reflection_transform


def flat_water(footprint, height):
    return SimpleNamespace(
        footprint=footprint, heights=lambda _, points: np.full(len(points), height)
    )


class WaterReflectionTests(TestCase):
    def test_captures_the_two_levels_in_view_and_skips_water_behind(self):
        camera = SimpleNamespace(
            eye=np.array([0.0, 0.0, 4.0]),
            look=np.array([0.0, 12 / 13, -5 / 13]),
            focal=707.0,
            source=None,
            water_plan=[
                flat_water(box(-30, 2, 0, 30), 1.0),
                flat_water(box(0, 2, 30, 30), -1.0),
                flat_water(box(-30, -30, 30, -2), 2.5),
            ],
        )
        self.assertEqual(set(reflection_levels(camera)), {-1.0, 1.0})
        camera.water_plan = camera.water_plan[2:]
        self.assertEqual(reflection_levels(camera), ())

    def test_mirror_fixes_the_water_plane_and_reverses_elevation(self):
        mirror = reflection_transform(3.5)
        np.testing.assert_allclose(mirror @ [2, 5, 3.5, 1], [2, 5, 3.5, 1])
        np.testing.assert_allclose(mirror @ [2, 5, 8.5, 1], [2, 5, -1.5, 1])
        np.testing.assert_allclose(mirror @ mirror, np.eye(4))
