"""Independent exhaustive opaque boxes verify clipped premultiplied transport."""

import itertools
import unittest

import numpy as np
from test_urban_parent_conditional import brute_hit
from urban_parent_conditional import grid, render_conditional
from urban_parent_layer import compose, render_layer
from urban_parent_light import light_at, shade
from urban_parent_stats import UrbanParent


def fixture():
    parent = UrbanParent(
        np.ones((2, 2, 3)),
        np.array([0, 0, 1, 32, 16, 0.75, 0.15]),
        np.array([6.0, 12.0]),
        np.array([0.7, 0.3]),
    )
    origins = np.array(
        [[x, y, 50] for x in [5.0, 11.0, 19.0, 27.0] for y in [-8.0, 5.0, 19.0]]
    )
    directions = np.tile([0.05, 0.45, -1.0], (len(origins), 1))
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    profile = {
        "kind": "sun",
        "direction": [0.45, -0.5, 0.7],
        "intensity": 3,
        "ambient": 0.1,
    }
    return parent, origins, directions, profile


def brute_layer(heights, origins, directions, stop, profile, camera_mask, caster_mask):
    cells = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
    bounds = np.zeros((4, 2, 3))
    bounds[:, 0, :2], bounds[:, 1, :2] = (cells + 0.125) * 16, (cells + 0.875) * 16
    bounds[:, 1, 2] = heights
    camera = bounds[(np.asarray(heights) > 0) & ~camera_mask]
    casters = bounds[(np.asarray(heights) > 0) & ~caster_mask]
    depth, normal = np.full(len(origins), np.inf), np.zeros_like(origins)
    if len(camera):
        depth, normal = brute_hit(camera, origins, directions, 0.1)
    hit = depth < stop
    normal[~hit] = [0, 0, 1]
    points = origins + np.where(hit, depth, stop)[:, None] * directions
    light, _, limit = light_at(points, profile)
    visibility = np.ones(len(origins))
    if len(casters):
        shadow, _ = brute_hit(casters, points + normal * 0.001, light, 0)
        visibility = (~np.isfinite(shadow) | (shadow >= limit)).astype(float)
    colour = np.zeros_like(origins)
    for mask, albedo in [(hit, 0.6), (~hit, 0.2)]:
        colour[mask] = shade(
            points[mask],
            normal[mask],
            -directions[mask],
            albedo,
            visibility[mask],
            profile,
        )
    return colour, hit.astype(float)


class ParentLayerTest(unittest.TestCase):
    def test_ground_reproduces_existing_lighting(self):
        parent, o, d, p = fixture()
        stop = -o[:, 2] / d[:, 2]
        c, t, s, _ = render_layer(parent, o, d, p, stop, batch=5)
        points, n = o + stop[:, None] * d, np.tile([0.0, 0.0, 1.0], (len(o), 1))
        direct = shade(points, n, -d, 0.2, np.ones(len(o)), p | {"ambient": 0})
        other = np.full_like(o, p["ambient"] * 0.2)
        expected, alpha, _ = render_conditional(parent, o, d, p, batch=5)
        np.testing.assert_allclose(
            compose(c, t, s, direct, other), expected, atol=2e-14
        )
        np.testing.assert_array_equal(1 - t, alpha)

    def test_81_scenes_with_stops_and_independent_replacement(self):
        parent, o, d, p = fixture()
        cells = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
        regions = np.array([[[0, 0], [1, 2]]])
        mask = cells[:, 0] == 0
        for z, view, caster in [
            (8, False, False),
            (0, True, False),
            (0, False, True),
            (8, True, True),
        ]:
            stop = (z - o[:, 2]) / d[:, 2]
            rgb, alpha = np.zeros_like(o), np.zeros(len(o))
            for state in itertools.product(range(3), repeat=4):
                weight = np.prod(np.array([0.15, 0.85 * 0.7, 0.85 * 0.3])[list(state)])
                colour, coverage = brute_layer(
                    np.array([0, 6, 12])[list(state)],
                    o,
                    d,
                    stop,
                    p,
                    mask & view,
                    mask & caster,
                )
                rgb += weight * colour
                alpha += weight * coverage
            c, t, s, _ = render_layer(
                parent,
                o,
                d,
                p,
                stop,
                primary_covered=regions if view else None,
                caster_covered=regions if caster else None,
                batch=5,
            )
            points, n = o + stop[:, None] * d, np.tile([0.0, 0.0, 1.0], (len(o), 1))
            direct = shade(points, n, -d, 0.2, np.ones(len(o)), p | {"ambient": 0})
            actual = compose(c, t, s, direct, np.full_like(o, p["ambient"] * 0.2))
            with self.subTest(z=z, view=view, caster=caster):
                np.testing.assert_allclose(1 - t, alpha, atol=2e-14)
                np.testing.assert_allclose(actual, rgb, atol=2e-13, rtol=0)

    def test_full_replacement_skips_both_traversals(self):
        parent, o, d, p = fixture()
        regions = np.array([[np.zeros(2, int), grid(parent)["shape"]]])
        c, t, s, work = render_layer(
            parent,
            o,
            d,
            p,
            -o[:, 2] / d[:, 2],
            primary_covered=regions,
            caster_covered=regions,
        )
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)
        np.testing.assert_array_equal(s, 1)
        self.assertEqual(work["cell_ray_visits"], 0)
        self.assertEqual(work["shadow_cell_ray_visits"], 0)

    def test_foreground_and_infinite_sky(self):
        parent, o, d, p = fixture()
        p = {
            "kind": "point",
            "position": [16, 16, 40],
            "intensity": 3000,
            "ambient": 0.002,
        }
        stop = np.full(len(o), 1.0)
        c, t, _, work = render_layer(parent, o, d, p, stop)
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)
        self.assertEqual(work["cell_ray_visits"], 0)
        _, _, wall_visibility, _ = render_layer(
            parent,
            o[:1],
            d[:1],
            p,
            stop[:1],
            receiver_normals=np.array([[1.0, 0.0, 0.0]]),
        )
        np.testing.assert_array_equal(wall_visibility, 1)
        c, t, s, _ = render_layer(parent, o, d, p, np.full(len(o), np.inf))
        self.assertTrue(np.isfinite(c).all())
        self.assertTrue(np.isfinite(t).all())
        np.testing.assert_array_equal(s, 1)

    def test_invalid_contracts(self):
        parent, o, d, p = fixture()
        for kwargs in [
            {"stop": [1]},
            {"stop": np.full(len(o), np.nan)},
            {"stop": np.full(len(o), -0.1)},
            {"primary_covered": np.array([[[0.0, 0.0], [1.0, 1.0]]])},
            {"caster_covered": np.array([[[1, 0], [0, 1]]])},
            {"receiver_normals": np.zeros_like(o)},
        ]:
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                render_layer(parent, o, d, p, **({"stop": np.ones(len(o))} | kwargs))

    def test_coplanar_roof_and_external_caster(self):
        parent, o, d, p = fixture()
        o, d = np.array([[8.0, 8.0, 50.0]]), np.array([[0.0, 0.0, -1.0]])
        parent.heights, parent.probabilities = np.array([12.0]), np.array([1.0])
        c, t, _, _ = render_layer(parent, o, d, p, np.array([38.0]))
        np.testing.assert_array_equal(c, 0)
        np.testing.assert_array_equal(t, 1)
        dark, _, _, _ = render_layer(
            parent,
            o,
            d,
            p,
            np.array([50.0]),
            external_visibility=lambda points, normals: np.zeros(len(points)),
        )
        ambient, _, _, _ = render_layer(
            parent, o, d, p | {"intensity": 0}, np.array([50.0])
        )
        np.testing.assert_array_equal(dark, ambient)
        with self.assertRaises(ValueError):
            render_layer(
                parent,
                o,
                d,
                p,
                np.array([50.0]),
                external_visibility=lambda points, normals: np.full(
                    len(points), np.nan
                ),
            )


if __name__ == "__main__":
    unittest.main()
