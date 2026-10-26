"""Affine underpass, boundary and topology controls independent of Vienna recipes."""

import dataclasses
import random
import unittest
from fractions import Fraction as F

import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Reservation
from stair_occupancy_geometry import AffineOccupancy
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_inputs import volume
from stair_occupancy_pipeline import handoff


def slab(domain=None, lower=(5, 1, 0), upper=(6, 1, 0), origin=(0, 0)):
    return AffineOccupancy(
        "road",
        domain if domain is not None else shapely.box(0, 0, 10, 10),
        "traffic",
        origin,
        lower,
        upper,
    )


def oracle(vertices, plane):
    """Independent exact triangle clipping, with a different edge formulation."""
    result = []
    for i, a in enumerate(vertices):
        b = vertices[(i + 1) % len(vertices)]
        fa, fb = (plane[0] + plane[1] * p[0] + plane[2] * p[1] for p in (a, b))
        if fa >= 0:
            result.append(a)
        if fa * fb < 0:
            result.append(
                tuple((fb * x - fa * y) / (fb - fa) for x, y in zip(a, b, strict=True))
            )
    if not result or all(
        plane[0] + plane[1] * p[0] + plane[2] * p[1] == 0 for p in result
    ):
        return []
    return result


def exact_area(vertices):
    if len(vertices) < 3:
        return F(0)
    return (
        abs(
            sum(
                a[0] * b[1] - b[0] * a[1]
                for a, b in zip(vertices, vertices[1:] + vertices[:1], strict=True)
            )
        )
        / 2
    )


class AffineClearance(unittest.TestCase):
    def test_cap_fit_covers_original_binary64_body_outward_with_bounded_padding(self):
        import numpy as np
        from stair_occupancy_verify import cap_error

        upper = np.array([[0.1, 0.2, 12.3], [0.3, 0.4, 12.4], [0.8, 0.3, 12.7]])
        lower = upper - [0, 0, 0.2]
        row, bound = volume("actual", lower, upper, 2.5)
        independent = cap_error(row, lower, upper, 2.5)
        self.assertGreater(bound, 0)
        self.assertEqual(bound, independent)
        self.assertLess(bound, 1e-8)

    def test_index_and_scan_preserve_adjusted_source_and_pass_height_aware_pipeline(
        self,
    ):
        from outshine_ref.contracts import LocalFrame

        original = BuildingFootprint(
            "source",
            shapely.box(0, 0, 10, 10),
            HeightInterval(0, 8),
            part=(2,),
            attributes={"class": "real", "nested": [1, 2]},
        )
        query = OccupancyQuery((slab(),))
        changes, notes = adjusted(query, (original,))
        scan, _ = adjusted(query, (original,), method="scan")
        self.assertTrue(
            changes[0].adjusted[0].geometry.equals(scan[0].adjusted[0].geometry)
        )
        self.assertEqual(notes[0]["retained_area_m2"], 70)
        kept = changes[0].adjusted[0]
        self.assertEqual(
            (kept.key, kept.attributes, kept.heights),
            (original.key, original.attributes, original.heights),
        )
        underpass = BuildingFootprint(
            "probe", shapely.box(4, 0, 5, 1), HeightInterval(0, 2)
        )
        world, seen = handoff(
            LocalFrame("local"), (original,), (slab(),), changes, (underpass,)
        )
        self.assertEqual(len(world.footprints), 1)
        # The retained building blocks the probe, although the bridge itself does not.
        self.assertEqual(seen["probe_blocked"], (True,))

    def test_budget_failure_is_local_and_never_exposes_unverified_space(self):
        original = BuildingFootprint(
            "blocked", shapely.box(0, 0, 1, 1), HeightInterval(5, 6)
        )
        free = BuildingFootprint(
            "free", shapely.box(20, 20, 21, 21), HeightInterval(5, 6)
        )
        changes, notes = adjusted(
            OccupancyQuery((slab(), slab())), (original, free), maximum=1
        )
        self.assertEqual(changes[0].adjusted, ())
        self.assertIn("candidate budget", notes[0]["deferred"])
        self.assertNotIn("deferred", notes[1])
        self.assertEqual(len(changes), 1)

    def test_underpass_only_clips_the_actual_sloped_slice(self):
        actual = slab().conflict_geometry(HeightInterval(3, 8))
        self.assertTrue(actual.equals(shapely.box(0, 0, 3, 10)))
        below = BuildingFootprint(
            "building", shapely.box(4, 1, 9, 9), HeightInterval(0, 8)
        )
        self.assertFalse(slab().overlaps(below))
        self.assertTrue(
            Reservation(
                "old-envelope", slab().geometry, slab().heights, "traffic"
            ).overlaps(below)
        )

    def test_two_height_planes_both_cut_the_query(self):
        actual = slab().conflict_geometry(HeightInterval(8, 10))
        self.assertTrue(actual.equals(shapely.box(2, 0, 5, 10)))

    def test_height_and_xy_boundary_touches_are_free(self):
        flat = slab(lower=(5, 0, 0), upper=(6, 0, 0))
        for h in (HeightInterval(0, 5), HeightInterval(6, 10)):
            empty = flat.conflict_geometry(h)
            self.assertTrue(empty.is_empty)
            self.assertEqual(empty.geom_type, "Polygon")
        touch = BuildingFootprint(
            "touch", shapely.box(10, 0, 11, 1), HeightInterval(5, 6)
        )
        self.assertFalse(flat.overlaps(touch))
        self.assertTrue(slab().conflict_geometry(HeightInterval(0, 5)).is_empty)

    def test_polygonal_closure_discards_disconnected_line_touch(self):
        domain = shapely.MultiPolygon(
            [shapely.box(0, 0, 1, 1), shapely.box(2, 0, 3, 1)]
        )
        actual = slab(domain).conflict_geometry(HeightInterval(0, 7))
        self.assertTrue(actual.equals(shapely.box(0, 0, 1, 1)))
        self.assertEqual(actual.geom_type, "Polygon")

    def test_holes_and_disconnected_parts_remain(self):
        holed = shapely.Polygon(
            shapely.box(0, 0, 10, 10).exterior.coords,
            [shapely.box(1, 1, 2, 2).exterior.coords],
        )
        domain = shapely.MultiPolygon([holed, shapely.box(20, 0, 21, 1)])
        actual = slab(domain).conflict_geometry(HeightInterval(0, 8))
        self.assertEqual(actual.geom_type, "Polygon")
        self.assertEqual(len(actual.interiors), 1)
        self.assertEqual(actual.area, 29)

    def test_local_origin_translates_without_changing_area(self):
        shifted = slab(
            shapely.box(100000, -100000, 100010, -99990), origin=(100000, -100000)
        )
        self.assertEqual(shifted.conflict_geometry(HeightInterval(8, 10)).area, 30)

    def test_one_ulp_positive_height_is_not_an_arbitrary_epsilon(self):
        import math

        flat = slab(lower=(5, 0, 0), upper=(6, 0, 0))
        self.assertEqual(
            flat.conflict_geometry(HeightInterval(0, math.nextafter(5.0, 6))).area, 100
        )

    def test_invalid_and_mutable_inputs_and_inversion_are_rejected(self):
        for kw in (
            {"lower": [5, 1, 0]},
            {"upper": (4, 1, 0)},
            {"upper": (6, -1, 0)},
            {"upper": (6, 0.875, 0), "domain": shapely.box(0, 0, 8, 8)},
            {"origin": (float("nan"), 0)},
            {"domain": shapely.box(1e7, 0, 1e7 + 1, 1)},
            {"lower": (0, 10001, 0)},
            {"lower": (5, 0, 0), "upper": (5, 0, 0)},
        ):
            with self.subTest(kw=kw), self.assertRaises(ValueError):
                slab(**kw)
        with self.assertRaises(dataclasses.FrozenInstanceError):
            slab().purpose = "changed"

    def test_old_constant_reservation_stays_constant(self):
        old = Reservation("old", shapely.box(0, 0, 1, 1), HeightInterval(5, 6), "old")
        self.assertIs(old.conflict_geometry(HeightInterval(5, 7)), old.geometry)
        self.assertTrue(old.conflict_geometry(HeightInterval(0, 5)).is_empty)

    def test_exact_rational_triangle_oracle(self):
        rng = random.Random(568)
        for case in range(200):
            vertices = [
                (F(0), F(0)),
                (F(rng.randint(1, 20)), F(0)),
                (F(0), F(rng.randint(1, 20))),
            ]
            z, gx, gy = (F(rng.randint(-8, 8), 4) for _ in range(3))
            lower, upper = F(rng.randint(-10, 10), 2), F(rng.randint(11, 20), 2)
            wanted = oracle(vertices, (upper - z, -gx, -gy))
            wanted = oracle(wanted, (z + 2 - lower, gx, gy)) if wanted else []
            p = slab(
                shapely.Polygon(vertices),
                (float(z), float(gx), float(gy)),
                (float(z + 2), float(gx), float(gy)),
            )
            actual = p.conflict_geometry(HeightInterval(float(lower), float(upper)))
            with self.subTest(case=case):
                self.assertAlmostEqual(
                    actual.area, float(exact_area(wanted)), delta=1e-10
                )
                self.assertTrue(actual.is_valid)


if __name__ == "__main__":
    unittest.main()
