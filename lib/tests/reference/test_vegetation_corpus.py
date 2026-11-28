import unittest
from collections import Counter
from datetime import date
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from lib.tests.fixtures.vegetation.catalog import (
    CONTINENTS,
    load_holdout_anchors,
    load_sites,
    site_views,
)
from lib.tests.fixtures.vegetation.pin_sources import collect_pins
from lib.tests.fixtures.vienna.catalog import load_catalog
from lib.tests.render_places import main


class VegetationCorpusTest(unittest.TestCase):
    def test_pilot_allocates_two_development_sites_per_continent(self):
        sites = load_sites(pilot_only=True).values()
        self.assertEqual(
            Counter(site["continent"] for site in sites),
            dict.fromkeys(CONTINENTS, 2),
        )
        for site in sites:
            self.assertEqual(site["partition"], "development")

    def test_holdout_anchors_are_unique_and_reserved_outside_development(self):
        records = load_holdout_anchors()
        self.assertEqual(
            Counter(site["continent"] for site in records),
            dict.fromkeys(CONTINENTS, 10),
        )
        self.assertEqual(len({site["id"] for site in records}), len(records))
        self.assertEqual(
            len({(site["latitude"], site["longitude"]) for site in records}),
            len(records),
        )
        development = load_sites()
        development_anchors = {
            (site["latitude"], site["longitude"]) for site in development.values()
        }
        for site in records:
            with self.subTest(site=site["id"]):
                self.assertEqual(site["partition"], "holdout")
                self.assertNotIn(site["id"], development)
                self.assertNotIn(
                    (site["latitude"], site["longitude"]), development_anchors
                )
                self.assertTrue(site["stratum"])
                self.assertEqual(site["id"], f"whc-{site['source_id']}")
                self.assertLess(abs(site["latitude"]), 85.05112878)
                self.assertLess(abs(site["longitude"]), 180)

    def test_pin_inventory_preserves_missing_tiles_and_rejects_changed_receipts(self):
        near = SimpleNamespace(
            source_receipts=(
                {"url": "local", "sha256": "a"},
                {"url": "missing-dem", "missing": True},
            )
        )
        regional = SimpleNamespace(
            source_receipts=(
                {"url": "regional", "sha256": "b"},
                {"url": "local", "sha256": "a"},
            )
        )
        with patch(
            "lib.tests.fixtures.vegetation.pin_sources.ReferenceScene",
            side_effect=[near, regional],
        ):
            self.assertEqual(
                collect_pins({}, ["near", "regional"]),
                {"local": "a", "regional": "b", "missing-dem": None},
            )
        changed = SimpleNamespace(source_receipts=({"url": "local", "sha256": "c"},))
        with (
            patch(
                "lib.tests.fixtures.vegetation.pin_sources.ReferenceScene",
                side_effect=[near, changed],
            ),
            self.assertRaisesRegex(ValueError, "Source changed during capture: local"),
        ):
            collect_pins({}, ["near", "changed"])

    def test_catalog_views_and_walks_resolve_to_reference_inputs(self):
        config = load_catalog()
        for site_id, site in load_sites().items():
            with self.subTest(site=site_id):
                self.assertGreaterEqual(len(set(site["views"])), 2)
                self.assertTrue(site["reference_photos"])
                for photo in site["reference_photos"]:
                    self.assertEqual(
                        date.fromisoformat(photo["date"]).isoformat(), photo["date"]
                    )
                captures = site_views(site, config)
                for view, view_config in captures:
                    self.assertNotIn("absolute_eye_m", view)
                    self.assertTrue(view.get("utc", view_config["utc"]))
                    self.assertTrue(view_config["source_pins"])
                if "walk" in site:
                    walk_config = captures[0][1]
                    self.assertIn(
                        walk_config["walks"][site["walk"]]["origin"], site["views"]
                    )

    def test_cli_renders_site_views_once_in_manifest_order(self):
        site_id = "at-koerbersee-treeline"
        args = ["render_places", "--vegetation-site", site_id]
        with (
            patch("sys.argv", [*args, "--vegetation-site", site_id]),
            patch("lib.tests.render_places.render_scene") as render,
        ):
            main()
        expected = load_sites()[site_id]["views"]
        self.assertEqual(
            [call.args[0]["id"] for call in render.call_args_list], expected
        )
        for call, view_id in zip(render.call_args_list, expected, strict=True):
            self.assertEqual(Path(call.args[2]).name, f"{view_id}.png")
            self.assertTrue(call.args[1]["source_pins"])
