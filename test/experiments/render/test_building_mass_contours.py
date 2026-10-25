"""Adjacent-word collapse is recoverable; topology and real small features stay binding."""

import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from build_building_mass import cook
from building_mass_contours import packed_domain
from building_mass_mesh import PackedNodeCollapse, extrude, strict_prism


class BuildingMassContours(unittest.TestCase):
    def test_actual_previous_groups_stay_word_equal_and_new_groups_own_whole_sources(
        self,
    ):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        with patch("building_mass_product.extrude", strict_prism):
            before, _ = cook(path, {33})
        after, report = cook(path, {33})

        def meshes(product):
            result = {}
            for i, (low, high) in enumerate(
                zip(
                    product["group_offsets"][:-1],
                    product["group_offsets"][1:],
                    strict=True,
                )
            ):
                faces = product["indices"][product["triangle_groups"] == i]
                nodes, indices = np.unique(faces, return_inverse=True)
                result[tuple(product["group_sources"][low:high])] = (
                    product["positions"][nodes].tobytes(),
                    indices.astype(np.uint16).tobytes(),
                )
            return result

        old, new = meshes(before), meshes(after)
        self.assertTrue(old.keys() <= new.keys())
        for key, mesh in old.items():
            self.assertEqual(mesh, new[key])
        extra = {s for key in new.keys() - old.keys() for s in key}
        self.assertEqual(
            extra, set(before["needs_sources"]) - set(after["needs_sources"])
        )
        np.testing.assert_array_equal(
            before["selected_sources"], after["selected_sources"]
        )
        rounded = {
            tuple(g["sources"]): g
            for g in report["groups"]
            if "pretriangulation_rounding" in g
        }
        self.assertEqual(rounded.keys(), new.keys() - old.keys())
        self.assertTrue(
            all(
                g["pretriangulation_rounding"]["adjacent_duplicate_words_removed"] > 0
                for g in rounded.values()
            )
        )

    def test_adjacent_short_edge_is_removed_before_both_caps_and_walls(self):
        domain = shapely.Polygon(
            [(100, 0), (100 + 1e-7, 0), (101, 0), (101, 1), (100, 1)]
        )
        with self.assertRaises(PackedNodeCollapse):
            strict_prism(domain, (0, 2))
        mesh, report = extrude(domain, (0, 2))
        self.assertEqual(
            report["pretriangulation_rounding"]["adjacent_duplicate_words_removed"], 1
        )
        self.assertEqual(report["euler"], 2)
        self.assertAlmostEqual(report["packed_volume_m3"], 2)
        self.assertEqual(len(mesh["positions"]), 8)
        self.assertEqual(len(mesh["indices"]), 12)

    def test_cyclic_first_last_duplicate_and_orientation_are_preserved(self):
        ring = [(100 + 1e-7, 0), (101, 0), (101, 1), (100, 1), (100, 0)]
        for points in (ring, ring[::-1]):
            rounded, report = packed_domain(shapely.Polygon(points))
            self.assertEqual(report["adjacent_duplicate_words_removed"], 1)
            self.assertEqual(rounded.exterior.is_ccw, shapely.LinearRing(points).is_ccw)
            self.assertEqual(rounded.area, 1)

    def test_unmodified_prism_path_is_word_equal_including_indices(self):
        domain = shapely.box(100.1234567, 2.2345678, 101.3456789, 3.456789)
        a, ar = strict_prism(domain, (1.234567, 5.345678))
        b, br = extrude(domain, (1.234567, 5.345678))
        for key in a:
            np.testing.assert_array_equal(a[key].view(np.uint8), b[key].view(np.uint8))
        self.assertEqual(ar, br)

    def test_representable_short_edge_is_not_simplified_or_welded(self):
        x = float(np.nextafter(np.float32(100), np.float32(101)))
        domain = shapely.Polygon([(100, 0), (x, 0), (101, 0), (101, 1), (100, 1)])
        rounded, report = packed_domain(domain)
        self.assertEqual(report["adjacent_duplicate_words_removed"], 0)
        np.testing.assert_array_equal(
            shapely.get_coordinates(rounded), shapely.get_coordinates(domain)
        )

    def test_real_hole_and_distinct_components_survive_adjacent_word_removal(self):
        domain = shapely.Polygon(
            [(100, 0), (100 + 1e-7, 0), (104, 0), (104, 4), (100, 4)],
            [[(101, 1), (101, 3), (103, 3), (103, 1)]],
        )
        combined = shapely.MultiPolygon([domain, shapely.box(106, 0, 107, 1)])
        rounded, _ = packed_domain(combined)
        self.assertEqual(len(rounded.geoms), 2)
        self.assertEqual(len(rounded.geoms[0].interiors), 1)
        self.assertFalse(rounded.covers(shapely.Point(102, 2)))
        _, report = extrude(combined, (0, 3))
        self.assertEqual(report["euler"], 2)
        self.assertAlmostEqual(report["packed_volume_m3"], 39)

    def test_narrow_hole_and_component_gap_cannot_disappear(self):
        hole = [(100.5, 1), (100.5, 3), (100.5 + 1e-7, 3), (100.5 + 1e-7, 1)]
        domain = shapely.Polygon([(100, 0), (104, 0), (104, 4), (100, 4)], [hole])
        with self.assertRaises(ValueError):
            packed_domain(domain)
        domains = shapely.MultiPolygon(
            [shapely.box(100, 0, 101, 1), shapely.box(101 + 1e-7, 0, 102, 1)]
        )
        self.assertTrue(domains.is_valid)
        with self.assertRaisesRegex(ValueError, "topology"):
            packed_domain(domains)

    def test_distant_word_collision_does_not_close_a_narrow_slit(self):
        domain = shapely.Polygon(
            [
                (100, 0),
                (104, 0),
                (104, 4),
                (102 + 1e-7, 4),
                (102 + 1e-7, 1),
                (102, 1),
                (102, 4),
                (100, 4),
            ]
        )
        self.assertTrue(domain.is_valid)
        with self.assertRaisesRegex(ValueError, "nonadjacent"):
            packed_domain(domain)

    def test_collapsed_height_and_tiny_footprint_remain_original_needs(self):
        for domain, heights in (
            (shapely.box(0, 0, 1, 1), (100, 100 + 1e-7)),
            (shapely.box(100, 0, 100 + 1e-7, 1), (0, 1)),
        ):
            with self.assertRaisesRegex(ValueError, "collapse in Float32"):
                extrude(domain, heights)

    def test_invalid_and_nonpolygon_inputs_are_not_repaired(self):
        for domain in (
            shapely.Polygon([(0, 0), (1, 1), (0, 1), (1, 0)]),
            shapely.LineString([(0, 0), (1, 1)]),
            shapely.Polygon(),
        ):
            with self.assertRaises(ValueError):
                packed_domain(domain)


if __name__ == "__main__":
    unittest.main()
