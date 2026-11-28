import json
import unittest
from collections import Counter
from pathlib import Path

from lib.tests.fixtures.infrastructure.catalog import (
    CONTINENTS,
    case_tiles,
    load_survey,
)

HERE = Path(__file__).resolve().parent.parent / "fixtures" / "infrastructure"


class InfrastructureCorpusTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = json.loads((HERE / "cases.json").read_text())

    def test_every_continent_has_40_development_cases_and_10_reserved_cases(self):
        rows = self.catalog["cases"]
        self.assertEqual(len({row["id"] for row in rows}), 300)
        self.assertEqual(len({tuple(row["at"]) for row in rows}), 300)
        self.assertEqual(
            Counter((row["continent"], row["split"]) for row in rows),
            {
                (c, s): n
                for c in CONTINENTS
                for s, n in (("development", 40), ("holdout", 10))
            },
        )
        expected = {
            f"{window.continent.lower()}-{window.area}-{serial}": window.split
            for window in load_survey()
            for serial in (1, 2)
        }
        self.assertEqual({row["id"]: row["split"] for row in rows}, expected)
        holdout = {row["family"] for row in rows if row["split"] == "holdout"}
        self.assertEqual(holdout, set(self.catalog["checks"]))

    def test_every_case_has_observed_source_evidence_and_two_distinct_views(self):
        receipts = {
            "/".join(map(str, item["tile"]))
            for layer in self.catalog["sources"].values()
            for item in layer
            if len(item["sha256"]) == 64 and item["bytes"] > 0
        }
        for row in self.catalog["cases"]:
            with self.subTest(case=row["id"]):
                actual = set(row["tiles"])
                required = {
                    "/".join(map(str, key))
                    for zoom in (12, 14)
                    for key in case_tiles(row, zoom)
                }
                self.assertTrue(row["evidence"])
                self.assertTrue(
                    required | {item[0] for item in row["evidence"]} <= actual
                )
                self.assertTrue(actual <= receipts)
                self.assertEqual(row["checks"], row["family"])
                self.assertEqual(len({tuple(view["at"]) for view in row["views"]}), 2)
                if row["family"] == "crossing":
                    self.assertEqual(
                        Counter(link[0] for link in row["links"]),
                        {"road": 2, "rail": 2},
                    )
                if row["family"] == "turnout":
                    self.assertGreaterEqual(len(row["links"]), 3)
