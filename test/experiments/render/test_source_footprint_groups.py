"""Analytic plan measures, holes, atomic unknowns and original source-only reads."""

import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import shapely
from parent_surface_inputs import ParentInputs, open_parent
from source_footprint_groups import coverage_groups, read_footprints, union_inventory


class FootprintMeasureTest(unittest.TestCase):
    def test_overlap_is_unioned_without_deduplicating_source_identity(self):
        geometry, report = union_inventory(
            [shapely.box(0, 0, 2, 1), shapely.box(1, 0, 3, 1)]
        )
        self.assertEqual(geometry.area, 3)
        self.assertEqual(report["source_area_sum_m2"], 4)
        self.assertEqual(report["largest_component_sources"], 2)
        self.assertEqual(report["overlap_fraction_of_sum"], 0.25)
        self.assertEqual(report["nearest_source_gap_quantiles_m"], [0] * 6)

    def test_identical_geometry_still_has_two_distinct_sources(self):
        _, report = union_inventory([shapely.box(0, 0, 1, 1), shapely.box(0, 0, 1, 1)])
        self.assertEqual(report["sources"], 2)
        self.assertEqual(report["largest_component_sources"], 2)
        self.assertEqual(report["nearest_source_gap_quantiles_m"], [0] * 6)

    def test_corner_contact_does_not_mix_polygon_area_membership(self):
        _, report = union_inventory([shapely.box(0, 0, 1, 1), shapely.box(1, 1, 2, 2)])
        self.assertEqual(report["union_components"], 2)
        self.assertEqual(report["largest_component_sources"], 1)
        self.assertEqual(report["nearest_source_gap_quantiles_m"], [0] * 6)

    def test_hole_and_another_source_inside_it_remain_separate(self):
        shell = shapely.Polygon(
            [(0, 0), (4, 0), (4, 4), (0, 4)], [[(1, 1), (3, 1), (3, 3), (1, 3)]]
        )
        geometry, report = union_inventory([shell, shapely.box(1.5, 1.5, 2.5, 2.5)])
        self.assertEqual(geometry.area, 13)
        self.assertEqual(report["union_components"], 2)
        self.assertEqual(report["union_holes"], 1)
        self.assertEqual(report["nearest_source_gap_quantiles_m"], [0.5] * 6)

    def test_triangle_has_independently_known_cell_areas_and_plan_l2(self):
        geometry = shapely.Polygon([(0, 0), (2, 0), (0, 2)])
        product, report = coverage_groups(geometry, 1)
        np.testing.assert_array_equal(product["fractions"], [[1, 0.5], [0.5, 0]])
        self.assertEqual(report["integrated_coverage_m2"], 2)
        self.assertEqual(report["opaque_any_touch_area_m2"], 3)
        self.assertEqual(report["cell_average_plan_l2_integral_m2"], 0.5)
        self.assertEqual(report["opaque_any_touch_plan_l2_integral_m2"], 1)

    def test_negative_global_grid_origin_preserves_covered_area(self):
        geometry = shapely.Polygon([(-0.5, -0.25), (1.5, -0.25), (-0.5, 1.75)])
        product, report = coverage_groups(geometry, 1)
        np.testing.assert_array_equal(product["low_xy_m"], [-1, -1])
        self.assertEqual(report["integrated_coverage_m2"], 2)
        self.assertTrue(
            np.all((product["fractions"] >= 0) & (product["fractions"] <= 1))
        )

    def test_unknown_source_or_part_never_becomes_a_partial_known_footprint(self):
        valid = {
            "ring": [[0, 0], [1, 0], [1, 1], [0, 1]],
            "holes": [],
            "foot_m": 0,
            "sole_m": 0,
            "roof_kind": 0,
        }
        invalid = valid | {"ring": [[0, 0], [1, 1], [1, 0], [0, 1]]}
        surface = {
            "origin": [0, 0, 0],
            "east": [1, 0, 0],
            "north": [0, 1, 0],
            "up": [0, 0, 1],
            "shapes": [valid],
        }
        parent = SimpleNamespace(
            plans={
                "summaries": [
                    {
                        "supported": True,
                        "frame": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]],
                    }
                ]
            },
            iter_forms=lambda _cells: iter(
                [
                    (
                        1,
                        [3, 4, 5],
                        [None, surface | {"shapes": [valid, invalid]}, surface],
                    )
                ]
            ),
        )
        geometries, registry, report = read_footprints(parent, {1})
        self.assertEqual(len(geometries), 1)
        np.testing.assert_array_equal(registry["source_ids"], [3, 4, 5])
        np.testing.assert_array_equal(
            registry["source_supported"], [False, False, True]
        )
        self.assertEqual([row["source"] for row in report["needs"]], [3, 4])

    def test_empty_and_unbounded_input_is_rejected_as_an_unknown_measure(self):
        with self.assertRaises(ValueError):
            union_inventory([])
        for pitch in [0, -1, np.nan]:
            with self.assertRaises(ValueError):
                coverage_groups(shapely.box(0, 0, 1, 1), pitch)
        with patch(
            "source_footprint_groups.np.arange",
            side_effect=AssertionError("oversized grid must reject before allocation"),
        ):
            for extent in [2**25, 2**32]:
                with self.assertRaises(ValueError):
                    coverage_groups(shapely.box(0, 0, extent, extent), 1)


class OriginalFootprintTest(unittest.TestCase):
    def test_original_cell_builds_before_native_view_or_triangle_interpretation(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        original_member = ParentInputs.member
        read = []

        def member(parent, name):
            allowed = {parent.metadata["index"], parent.forms[33]}
            self.assertIn(name, allowed)
            read.append(name)
            return original_member(parent, name)

        with patch.object(ParentInputs, "member", member), open_parent(path) as parent:
            geometries, registry, _ = read_footprints(parent, {33})
            np.testing.assert_array_equal(
                registry["source_ids"], np.flatnonzero(parent.cells == 33)
            )
            self.assertTrue(registry["source_supported"].all())
            self.assertEqual(len(geometries), 576)
            geometry, inventory = union_inventory(geometries)
            product, coverage = coverage_groups(geometry, 64)
            self.assertEqual(parent.form_reads, 1)
            self.assertEqual(len(read), 2)
            self.assertEqual(
                inventory["source_coordinates"], inventory["union_coordinates"]
            )
            self.assertEqual(inventory["largest_component_sources"], 1)
            self.assertAlmostEqual(
                coverage["integrated_coverage_m2"], geometry.area, places=6
            )
            self.assertGreater(coverage["opaque_any_touch_area_m2"], 2 * geometry.area)
            self.assertLess(product["fractions"].size, len(geometries))
