"""Height-band queries and positive interior overlap without an underflowing area."""

import unittest
from fractions import Fraction

import shapely
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Reservation


class ReservationQueries(unittest.TestCase):
    def test_height_query_preserves_original_geometry_and_excludes_pure_height_touch(
        self,
    ):
        row = Reservation(
            "road", shapely.box(0, 0, 10, 10), HeightInterval(5, 6), "traffic"
        )
        self.assertIs(row.conflict_geometry(HeightInterval(4, 7)), row.geometry)
        for band in (HeightInterval(0, 5), HeightInterval(6, 7)):
            mask = row.conflict_geometry(band)
            self.assertEqual(mask.geom_type, "Polygon")
            self.assertTrue(mask.is_empty)

    def test_holes_xy_touch_and_containment_keep_their_positive_volume_meaning(self):
        source = shapely.Polygon(
            shapely.box(0, 0, 10, 10).exterior.coords,
            [shapely.box(4, 4, 6, 6).exterior.coords],
        )
        h = HeightInterval(5, 6)
        row = Reservation("road", source, h, "traffic")
        for shape, wanted in (
            (shapely.box(4, 4, 6, 6), False),
            (shapely.box(10, 0, 11, 1), False),
            (shapely.box(1, 1, 2, 2), True),
            (source, True),
        ):
            self.assertIs(row.overlaps(BuildingFootprint("building", shape, h)), wanted)

    def test_valid_positive_domains_and_crossing_slivers_survive_area_underflow(self):
        length = 1e-170
        self.assertGreater(Fraction(length) ** 2, 0)
        tiny = shapely.box(0, 0, length, length)
        pairs = (
            (tiny, tiny),
            (shapely.box(0, 0, 1, 1), tiny),
            (shapely.box(0, 0, 1, length), shapely.box(0, 0, length, 1)),
        )
        for a, b in pairs:
            with self.subTest(a=a.bounds, b=b.bounds):
                self.assertTrue(a.is_valid and b.is_valid)
                self.assertEqual(a.intersection(b).area, 0)
                row = Reservation("road", a, HeightInterval(5, 6), "traffic")
                self.assertTrue(
                    row.overlaps(BuildingFootprint("building", b, HeightInterval(5, 6)))
                )

    def test_query_requires_a_positive_explicit_height_interval(self):
        row = Reservation(
            "road", shapely.box(0, 0, 1, 1), HeightInterval(5, 6), "traffic"
        )
        with self.assertRaises(TypeError):
            row.conflict_geometry((5, 6))


if __name__ == "__main__":
    unittest.main()
