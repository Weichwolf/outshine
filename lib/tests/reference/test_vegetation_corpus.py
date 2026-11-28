import unittest
from pathlib import Path
from unittest.mock import patch

from lib.tests.fixtures.vegetation.catalog import load_sites, site_views
from lib.tests.fixtures.vienna.catalog import load_catalog
from lib.tests.render_places import main


class VegetationCorpusTest(unittest.TestCase):
    def test_catalog_views_and_walks_resolve_to_reference_inputs(self):
        config = load_catalog()
        for site_id, site in load_sites().items():
            with self.subTest(site=site_id):
                self.assertGreaterEqual(len(set(site["views"])), 2)
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
