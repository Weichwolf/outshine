from types import SimpleNamespace
from unittest import TestCase

import numpy as np
from shapely import box
from water_reflection import ReflectionPlane, reflection_planes


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
        self.assertEqual({p.height for p in reflection_planes(camera)}, {-1.0, 1.0})
        camera.water_plan = camera.water_plan[2:]
        self.assertEqual(reflection_planes(camera), ())

    def test_mirror_fixes_the_water_plane_and_reverses_elevation(self):
        mirror = ReflectionPlane.through((0, 0), 3.5, (0, 0)).mirror()
        np.testing.assert_allclose(mirror @ [2, 5, 3.5, 1], [2, 5, 3.5, 1])
        np.testing.assert_allclose(mirror @ [2, 5, 8.5, 1], [2, 5, -1.5, 1])
        np.testing.assert_allclose(mirror @ mirror, np.eye(4))

    def test_downhill_water_selects_a_tilted_mirror(self):
        body = flat_water(box(-20, 2, 20, 150), 1.0)
        body.profile = True
        body.heights = lambda _, points: 1.1 - points[:, 1] * 0.0005
        camera = SimpleNamespace(
            eye=np.array([0.0, 0.0, 4.0]),
            look=np.array([0.0, 12 / 13, -5 / 13]),
            focal=707.0,
            source=None,
            water_plan=[body],
        )
        (plane,) = reflection_planes(camera)
        np.testing.assert_allclose(
            plane.normal[:2] / plane.normal[2], [0.0, 0.0005], atol=1e-8
        )
        xyz = [2, 100, 1.05, 1]
        np.testing.assert_allclose(plane.mirror() @ xyz, xyz)
