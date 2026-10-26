"""Closed stored-geometry predicates and independent physical-plane controls."""

import unittest
from fractions import Fraction

import numpy as np
import shapely
from outshine_ref.places.camera import axes, ecef
from outshine_ref.places.sources import tile_geography
from pyproj import Transformer
from shapely.affinity import translate
from water_cell_field import prepare_field, sample_parameters
from water_geographic_predicates import (
    Interval,
    coefficients,
    exact_hit,
    filtered_hit,
    source_members,
)
from water_geographic_query import query_geographic
from water_nodal_height import parameter_triangle


def plane_field(height=500, *, hole=False, cell=(14, 8192, 8192)):
    mask = shapely.box(0, 0, 1, 1)
    if hole:
        mask = mask.difference(shapely.box(0.25, 0.25, 0.5, 0.5))

    def facet(keys):
        xy = parameter_triangle(keys)
        lat, lon = tile_geography(xy, 1, (cell[0], 0, 0))
        sea = ecef(lat, lon, 0)
        up = np.array([axes(a, b)[2] for a, b in zip(lat, lon, strict=True)])
        h = (6378137 + height - sea[:, 0]) / up[:, 0]
        world = sea + h[:, None] * up
        world[:, 0] = 6378137 + height
        return "admitted", world

    polygon = translate(mask, xoff=cell[1], yoff=cell[2])
    return prepare_field(
        cell, [polygon], ("analytic-plane",), facet, context=f"plane:{height}"
    )[0]


def constant_height_field(cell):
    def facet(keys):
        lat, lon = tile_geography(parameter_triangle(keys), 1, (cell[0], 0, 0))
        return "admitted", ecef(lat, lon, 500)

    source = shapely.box(cell[1], cell[2], cell[1] + 1, cell[2] + 1)
    return prepare_field(
        cell, [source], (cell,), facet, context="analytic-constant-geodetic-nodes-v1"
    )[0]


class PredicateControls(unittest.TestCase):
    def test_hole_boundaries_and_one_ulp_offsets_use_exact_source_coverage(self):
        polygon = shapely.box(0, 0, 1, 1).difference(
            shapely.box(0.25, 0.25, 0.75, 0.75)
        )
        for xy, wet in (
            ((0, 0), True),
            ((1, 0.5), True),
            ((0.25, 0.5), True),
            ((np.nextafter(0.25, 1), 0.5), False),
            ((np.nextafter(0.25, 0), 0.5), True),
            ((0.5, 0.5), False),
        ):
            point = tuple(Interval.value(v) for v in xy)

            def exact(xy=xy):
                return tuple(Fraction(float(v)) for v in xy)

            members = source_members([polygon], point, exact, lambda _: None)
            self.assertEqual(members, [0] if wet else [])

    def test_filtered_closed_triangle_matches_dyadic_barycentric_ground_truth(self):
        triangle = np.array([[0, 0, 0], [2, 0, 0], [0, 2, 0]])
        coordinates = [-np.spacing(0.0), 0, 0.25, 0.5, 1, 1.25]
        for u in coordinates:
            for v in coordinates:
                origin = [2 * u, 2 * v, 3.5]
                exact = coefficients(triangle, origin, [0, 0, 1], Fraction)
                expected = u >= 0 and v >= 0 and Fraction(u) + Fraction(v) <= 1
                self.assertEqual(exact_hit(exact), expected)
                selected = filtered_hit(
                    coefficients(triangle, origin, [0, 0, 1], Interval.value)
                )
                if selected is not None:
                    self.assertEqual(selected, expected)


class GeographicWater(unittest.TestCase):
    def test_high_latitude_dateline_and_southern_fields_match_the_exact_model(self):
        inverse = Transformer.from_crs(4978, 4979, always_xy=True)
        for cell in ((14, 0, 100), (14, 16383, 3000), (14, 14000, 12000)):
            field = constant_height_field(cell)
            world = sample_parameters(
                field, [[0.125, 0.125], [0.5, 0.5], [0.875, 0.875]]
            ).world
            lon, lat, _ = inverse.transform(*world.T)
            a = query_geographic(field, np.c_[lon, lat])
            b = query_geographic(field, np.c_[lon, lat], exact=True)
            self.assertEqual(a.states, ("water",) * 3)
            self.assertEqual(a.states, b.states)
            np.testing.assert_array_equal(a.parents, b.parents)
            np.testing.assert_allclose(
                a.world, world, rtol=0, atol=64 * np.spacing(6378137.0)
            )

    def test_four_cell_seams_have_a_hit_without_enlarging_any_cell(self):
        fields = [
            constant_height_field((14, x, y))
            for x in (8192, 8193)
            for y in (8192, 8193)
        ]
        world = sample_parameters(
            fields[0], [[1, 0.25], [1, 0.75], [1, 1], [0.25, 1], [0.75, 1]]
        ).world
        inverse = Transformer.from_crs(4978, 4979, always_xy=True)
        lon, lat, _ = inverse.transform(*world.T)
        results = [query_geographic(f, np.c_[lon, lat]) for f in fields]
        for i in range(len(world)):
            hits = [r for r in results if r.states[i] == "water"]
            self.assertTrue(hits)
            self.assertLessEqual(
                max(r.height_bounds_m[i, 0] for r in hits),
                min(r.height_bounds_m[i, 1] for r in hits),
            )

    def test_vertical_plane_height_matches_closed_form_and_exact_bounds(self):
        field = plane_field()
        points = np.array([[0, 0], [0.001, -0.001], [0.01, -0.01]])
        result = query_geographic(field, points)
        self.assertEqual(result.states, ("water",) * len(points))
        origin = ecef(points[:, 1], points[:, 0], 0)
        for i, (lon, lat) in enumerate(points):
            up = axes(lat, lon)[2]
            expected = (Fraction(6378637) - Fraction(float(origin[i, 0]))) / Fraction(
                float(up[0])
            )
            self.assertLessEqual(
                Fraction(float(result.height_bounds_m[i, 0])), expected
            )
            self.assertGreaterEqual(
                Fraction(float(result.height_bounds_m[i, 1])), expected
            )
            self.assertLess(abs(result.height_m[i] - float(expected)), 1e-8)
        np.testing.assert_allclose(result.world[:, 0], 6378637, rtol=0, atol=1e-8)

    def test_hole_and_outside_field_are_misses_and_boundaries_are_closed(self):
        field = plane_field(hole=True)
        water = sample_parameters(field, [[0.125, 0.125]]).world
        hole = sample_parameters(plane_field(), [[0.375, 0.375]]).world
        transform = Transformer.from_crs(4978, 4979, always_xy=True)
        lon, lat, _ = transform.transform(*np.concatenate([water, hole]).T)
        result = query_geographic(field, np.c_[lon, lat])
        self.assertEqual(result.states, ("water", "miss"))
        border = query_geographic(field, [[0, 0], [-0.001, 0], [180, 0]])
        self.assertEqual(border.states, ("water", "miss", "miss"))
        self.assertEqual(int(border.parents[0]), 0)

    def test_negative_model_height_is_not_rejected_as_a_negative_ray_distance(self):
        result = query_geographic(plane_field(-50), [[0, 0]])
        self.assertEqual(result.states, ("water",))
        self.assertEqual(result.height_m[0], -50)

    def test_budget_refusal_keeps_no_best_so_far_or_partial_source_members(self):
        field = plane_field()
        for options, state in (
            ({"predicate_limit": 0}, "predicate_budget_exhausted"),
            ({"member_limit": 0}, "membership_budget_exhausted"),
        ):
            result = query_geographic(field, [[0, 0]], **options)
            self.assertEqual(result.states, (state,))
            self.assertTrue(np.isnan(result.height_m).all())
            self.assertEqual(len(result.source_members), 0)
        for point in ([np.nan, 0], [181, 0], [0, 91]):
            with self.assertRaisesRegex(ValueError, "finite WGS84"):
                query_geographic(field, [point])

    def test_filtered_and_full_exact_modes_agree_and_outputs_are_owned(self):
        field = plane_field(hole=True)
        points = [[0, 0], [0.001, -0.001], [0.01, -0.01], [-0.001, 0]]
        a, b = (
            query_geographic(field, points),
            query_geographic(field, points, exact=True),
        )
        self.assertEqual(a.states, b.states)
        np.testing.assert_array_equal(a.parents, b.parents)
        np.testing.assert_array_equal(a.source_members, b.source_members)
        wet = np.array(a.states) == "water"
        self.assertTrue((a.height_bounds_m[wet, 0] <= b.height_m[wet]).all())
        self.assertTrue((a.height_bounds_m[wet, 1] >= b.height_m[wet]).all())
        self.assertFalse(a.world.flags.writeable)
        self.assertFalse(a.source_members.flags.writeable)


if __name__ == "__main__":
    unittest.main()
