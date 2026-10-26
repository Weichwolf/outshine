"""Conserved count measure, lazy work, true source support and stable leaf IDs."""

import json
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
from outshine_ref import LocalFrame
from outshine_ref.vegetation import HabitatStand, StandHabitat
from shapely.geometry.base import BaseGeometry


def plan(domain=None, *, counts=(1200, 800), levels=3, **budgets):
    domain = shapely.box(0, 0, 8, 8) if domain is None else domain
    habitat = StandHabitat.prepare(
        LocalFrame("test-enu"), "source-1", "root-8m", domain, (0, 0), 8
    )
    return HabitatStand(
        habitat, ("a", "b"), counts, (10.0, 20.0), (2.0, 4.0), levels=levels, **budgets
    )


class HabitatStandTests(unittest.TestCase):
    def test_fixed_actual_forest_habitats_keep_sources_areas_and_only_requested_roots(
        self,
    ):
        fixture = json.loads(
            (Path(__file__).parent / "data/habitat-stand-cases.geojson").read_text()
        )
        for feature in fixture["features"]:
            row = feature["properties"]
            habitat = StandHabitat.prepare(
                LocalFrame(row["frame"]),
                row["source_id"],
                row["region_id"],
                shapely.geometry.shape(feature["geometry"]),
                tuple(row["region_lower_xy_m"]),
                row["region_side_m"],
            )
            self.assertAlmostEqual(
                habitat.area_m2, row["expected_root_area_m2"], places=7
            )
            p = HabitatStand(
                habitat,
                tuple(row["kinds"]),
                tuple(row["counts"]),
                tuple(row["heights_m"]),
                tuple(row["spreads_m"]),
                levels=row["levels"],
                seed=row["seed"],
            )
            self.assertEqual(p.field()["source_id"], feature["id"])
            self.assertEqual(len(p._cells), 1)
            self.assertFalse(p._sites)
            key = (0, 0, 0)
            for _ in range(p.levels):
                parent = p.cell(key)
                children = p.children(key)
                np.testing.assert_array_equal(
                    np.array([c.counts for c in children]).sum(axis=0), parent.counts
                )
                self.assertAlmostEqual(
                    sum(c.area_m2 for c in children), parent.area_m2, places=7
                )
                key = max(children, key=lambda c: (sum(c.counts), c.key)).key
            result = p.sites(key)
            self.assertEqual(len(result["ids"]), sum(p.cell(key).counts))
            self.assertTrue(
                shapely.covers(
                    habitat.domain, shapely.points(result["positions_xy_m"])
                ).all()
            )
            self.assertEqual(len(p._sites), 1)
            self.assertLessEqual(len(p._cells), 1 + 4 * p.levels)

    def test_gross_field_has_no_children_random_sites_or_model_work(self):
        p = plan(counts=(10**9, 10**9))
        with patch.object(
            HabitatStand,
            "_random",
            side_effect=AssertionError("unrequested split/site RNG"),
        ):
            field = p.field()
        np.testing.assert_array_equal(field["counts"], [10**9, 10**9])
        self.assertEqual(len(p._cells), 1)
        limited = replace(p, max_cells=4096, max_geometry_bytes=p._geometry_bytes)
        with patch.object(
            HabitatStand,
            "_random",
            side_effect=AssertionError("geometry capacity before RNG"),
        ):
            self.assertIsNone(limited.children())
        self.assertEqual(len(limited._cells), 1)
        self.assertFalse(p._sites)
        self.assertIs(field, p.field())
        self.assertEqual(field["mean_height_m"], 15.0)
        self.assertEqual(field["frame"], LocalFrame("test-enu"))
        with self.assertRaises(TypeError):
            field["counts"] = None
        with self.assertRaises(ValueError):
            field["counts"].setflags(write=True)

    def test_analytic_half_support_and_every_parent_integer_area_conservation(self):
        p = plan(shapely.box(0, 0, 4, 8), counts=(100000, 0))
        children = p.children()
        self.assertEqual([c.area_m2 for c in children], [16, 0, 16, 0])
        self.assertEqual(children[1].counts, (0, 0))
        self.assertEqual(children[3].counts, (0, 0))
        self.assertLess(abs(children[0].counts[0] - 50000), 2000)
        for level in range(p.levels):
            for x in range(1 << level):
                for y in range(1 << level):
                    parent = p.cell((level, x, y))
                    child = p.children(parent.key)
                    np.testing.assert_array_equal(
                        np.array([c.counts for c in child]).sum(axis=0), parent.counts
                    )
                    self.assertAlmostEqual(
                        sum(c.area_m2 for c in child), parent.area_m2, places=12
                    )
        self.assertFalse(p._sites)

    def test_holes_and_thin_disconnected_support_have_no_rejection_or_outside_sites(
        self,
    ):
        domain = shapely.box(0, 0, 8, 8).difference(shapely.box(1, 1, 7, 7))
        p = plan(domain)
        records = []
        for x in range(8):
            for y in range(8):
                result = p.sites((3, x, y))
                self.assertTrue(
                    shapely.covers(
                        domain, shapely.points(result["positions_xy_m"])
                    ).all()
                )
                records.append(result)
        self.assertEqual(sum(len(r["ids"]) for r in records), 2000)
        ids = np.concatenate([r["ids"] for r in records])
        self.assertEqual(len(np.unique(ids, axis=0)), 2000)
        for record in records:
            with self.assertRaises(ValueError):
                record["positions_xy_m"].setflags(write=True)
        thin = plan(shapely.box(0, 0, 8, 0.00001), levels=0, counts=(20, 0))
        result = thin.sites((0, 0, 0))
        self.assertEqual(len(result["ids"]), 20)
        self.assertTrue(
            shapely.covers(
                thin.habitat.domain, shapely.points(result["positions_xy_m"])
            ).all()
        )

    def test_uniform_triangle_integral_and_disconnected_area_weights(self):
        triangle = shapely.Polygon(((0, 0), (8, 0), (0, 8)))
        p = plan(triangle, levels=0, counts=(20000, 0))
        sites = p.sites((0, 0, 0))["positions_xy_m"]
        np.testing.assert_allclose(
            sites.mean(axis=0), (8 / 3, 8 / 3), rtol=0, atol=0.08
        )
        domain = shapely.MultiPolygon(
            (shapely.box(0, 0, 1, 1), shapely.box(4, 0, 7, 1))
        )
        p = plan(domain, levels=0, counts=(10000, 0))
        sites = p.sites((0, 0, 0))["positions_xy_m"]
        self.assertLess(abs(float((sites[:, 0] < 2).mean()) - 0.25), 0.02)

    def test_true_saved_replay_reversed_requests_and_traits_do_not_move_layout(self):
        p = plan()
        recipe = json.loads(json.dumps(p.recipe()))
        with (
            patch.object(
                StandHabitat,
                "prepare",
                side_effect=AssertionError("source prepare on restore"),
            ),
            patch.object(
                BaseGeometry,
                "intersection",
                side_effect=AssertionError("source clip on restore"),
            ),
            patch.object(
                shapely,
                "normalize",
                side_effect=AssertionError("source normalize on restore"),
            ),
        ):
            q = HabitatStand.from_recipe(recipe)
            np.testing.assert_array_equal(q.field()["counts"], p.counts)
        keys = [(3, 2, 3), (3, 7, 1), (3, 0, 0)]
        a = {key: p.sites(key) for key in keys}
        b = {key: q.sites(key) for key in reversed(keys)}
        changed = replace(p, heights_m=(15.0, 25.0), spreads_m=(3.0, 5.0))
        self.assertEqual(p.layout_identity, changed.layout_identity)
        self.assertNotEqual(p.identity, changed.identity)
        for key in keys:
            for name in a[key]:
                np.testing.assert_array_equal(a[key][name], b[key][name])
                np.testing.assert_array_equal(a[key][name], changed.sites(key)[name])
        self.assertNotEqual(replace(p, seed=8).layout_identity, p.layout_identity)

    def test_prepared_parent_replay_does_not_repeat_splits_or_source_processing(self):
        p = plan()
        for key in ((2, 1, 2), (2, 3, 0), (2, 0, 0)):
            p.field(key)
        recipe = json.loads(json.dumps(p.recipe()))
        with (
            patch.object(
                HabitatStand,
                "_split",
                side_effect=AssertionError("split on prepared hit"),
            ),
            patch.object(
                HabitatStand,
                "_random",
                side_effect=AssertionError("RNG on prepared hit"),
            ),
            patch.object(
                BaseGeometry,
                "intersection",
                side_effect=AssertionError("clip on prepared hit"),
            ),
            patch.object(
                shapely,
                "normalize",
                side_effect=AssertionError("normalize on prepared hit"),
            ),
        ):
            restored = HabitatStand.from_recipe(recipe)
            self.assertEqual(len(restored._cells), len(p._cells))
            for key in p._fields:
                np.testing.assert_array_equal(
                    restored.field(key)["counts"], p.field(key)["counts"]
                )
        wrong = json.loads(json.dumps(recipe))
        wrong["prepared_cells"][0]["counts"][0] += 1
        with self.assertRaisesRegex(ValueError, "conserve parent counts"):
            HabitatStand.from_recipe(wrong)
        exposed = p.habitat.layout
        exposed["lower"][0] = 100
        self.assertEqual(p.habitat.lower, (0.0, 0.0))

    def test_capacity_refuses_before_polygon_splits_or_site_triangulation(self):
        p = plan(max_cells=1)
        with patch.object(
            BaseGeometry,
            "intersection",
            side_effect=AssertionError("capacity before split"),
        ):
            self.assertIsNone(p.cell((3, 2, 1)))
        self.assertEqual(len(p._cells), 1)
        p = plan(levels=0, counts=(10, 0))
        with patch.object(
            shapely,
            "constrained_delaunay_triangles",
            side_effect=AssertionError("capacity before triangles"),
        ):
            self.assertIsNone(p.sites((0, 0, 0), max_sites=9))
            self.assertIsNone(p.sites((0, 0, 0), max_triangles=1))
            self.assertIsNone(p.sites((0, 0, 0), max_sites=0))
            self.assertIsNone(replace(p, max_cached_sites=0).sites((0, 0, 0)))

    def test_provenance_outside_source_and_empty_habitat_remain_explicit(self):
        p = plan(shapely.box(-2, -2, 9, 9))
        self.assertEqual(p.habitat.source_area_m2, 121)
        self.assertEqual(p.habitat.area_m2, 64)
        self.assertEqual(p.field()["source_id"], "source-1")
        self.assertEqual(p.field()["region_id"], "root-8m")
        source = shapely.box(0, 0, 8, 8)
        adjusted = StandHabitat.prepare(
            LocalFrame("test-enu"),
            "source-1",
            "root-8m",
            source,
            (0, 0),
            8,
            support=source.difference(shapely.box(0, 0, 2, 8)),
        )
        self.assertEqual(adjusted.source_area_m2, 64)
        self.assertEqual(adjusted.area_m2, 48)
        self.assertTrue(adjusted.source_geometry.equals(source))
        empty = plan(shapely.box(20, 20, 21, 21), counts=(0, 0))
        self.assertEqual(empty.field()["cover_estimate"], 0)
        with self.assertRaises(ValueError):
            replace(empty, counts=(1, 0))
        with self.assertRaises(ValueError):
            replace(p, kinds=("b", "a"))
        with self.assertRaises(ValueError):
            p.cell((3, 8, 0))
        with self.assertRaises(ValueError):
            p.sites((2, 0, 0))
        for kwargs in (
            {"counts": (True, 0)},
            {"levels": 17},
            {"seed": -1},
            {"max_cells": 0},
        ):
            with self.assertRaises(ValueError):
                replace(p, **kwargs)


if __name__ == "__main__":
    unittest.main()
