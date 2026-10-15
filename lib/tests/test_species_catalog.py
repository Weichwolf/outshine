"""Candidate eligibility, immutable source evidence and installed pilot records."""

import copy
import unittest

from outshine_ref.vegetation import SpeciesCatalog, load_species_catalog


def document():
    return {
        "format": "outshine-species-pilot-1",
        "distribution_fields": [
            "region",
            "introduced",
            "extinct",
            "doubtful",
            "source_ids",
        ],
        "source": {"title": "own fictional controls"},
        "requests": [
            {"requested_name": "Fictiona arbor", "accepted_id": "outshine:taxon:wcvp:1"}
        ],
        "species": [
            {
                "id": "outshine:taxon:wcvp:1",
                "wcvp_id": "1",
                "scientific_name": "Fictiona arbor",
                "distribution": [
                    ["SWI", 0, 0, 0, ["10"]],
                    ["TEX", 1, 0, 0, ["11"]],
                    ["GER", 0, 1, 0, ["12"]],
                    ["AUT", 0, 0, 1, ["13"]],
                ],
            },
            {
                "id": "outshine:taxon:wcvp:2",
                "wcvp_id": "2",
                "scientific_name": "Fictiona other",
                "distribution": [["SWI", 1, 0, 0, ["20"]], ["AUT", 0, 0, 0, ["21"]]],
            },
        ],
    }


class SpeciesCatalogTests(unittest.TestCase):
    def test_extant_certain_native_and_introduced_pools_remain_separate(self):
        catalog = SpeciesCatalog(document())
        one, two = "outshine:taxon:wcvp:1", "outshine:taxon:wcvp:2"
        self.assertEqual(catalog.candidate_ids(("SWI",)), (one,))
        self.assertEqual(catalog.candidate_ids(("SWI",), introduced=True), (two,))
        self.assertEqual(catalog.candidate_ids(("TEX",)), ())
        self.assertEqual(catalog.candidate_ids(("TEX",), introduced=True), (one,))
        self.assertEqual(catalog.candidate_ids(("GER",)), ())
        self.assertEqual(catalog.candidate_ids(("AUT",)), (two,))
        self.assertEqual(catalog.candidate_ids(("SWI", "AUT", "SWI")), (one, two))
        self.assertEqual(catalog.candidate_ids(("ZZZ",)), ())

    def test_inputs_are_copied_and_metadata_keeps_excluded_occurrence_evidence(self):
        data = document()
        before = copy.deepcopy(data)
        catalog = SpeciesCatalog(data)
        data["species"][0]["distribution"][0][1] = 1
        self.assertEqual(catalog.candidate_ids(("SWI",)), ("outshine:taxon:wcvp:1",))
        self.assertEqual(
            tuple(catalog.document["species"][0]["distribution"][2][:4]),
            ("GER", 0, 1, 0),
        )
        self.assertEqual(before["species"][0]["distribution"][0][1], 0)
        with self.assertRaises(TypeError):
            catalog.records["outshine:taxon:wcvp:1"]["scientific_name"] = "changed"
        with self.assertRaises(AttributeError):
            catalog.aliases = {}

    def test_invalid_identity_flags_and_region_inputs_refuse(self):
        for change in (
            lambda d: d["species"][0].update(id="wrong"),
            lambda d: d["species"][0]["distribution"][0].__setitem__(1, 2),
            lambda d: d["requests"][0].update(accepted_id="missing"),
        ):
            data = document()
            change(data)
            with self.assertRaises(ValueError):
                SpeciesCatalog(data)
        catalog = SpeciesCatalog(document())
        for regions in ("SWI", ("swiss",), ("swi",)):
            with self.assertRaises(ValueError):
                catalog.candidate_ids(regions)

    def test_packaged_real_pilot_and_synonym_keep_names_and_coverage_distinct(self):
        catalog = load_species_catalog()
        self.assertEqual(len(catalog.records), 30)
        self.assertEqual(len(catalog.aliases), 31)
        self.assertEqual(
            catalog.aliases["Acacia tortilis"], catalog.aliases["Vachellia tortilis"]
        )
        for name, region in (
            ("Picea abies", "SWI"),
            ("Eucalyptus globulus", "TAS"),
            ("Sequoiadendron giganteum", "CAL"),
            ("Vachellia tortilis", "KEN"),
        ):
            self.assertIn(catalog.aliases[name], catalog.candidate_ids((region,)))
        self.assertNotIn(
            catalog.aliases["Picea abies"], catalog.candidate_ids(("CAL",))
        )
        self.assertTrue(
            all(
                r["recipe_id"] is None and not r["visually_accepted"]
                for r in catalog.records.values()
            )
        )
        self.assertEqual(catalog.document["source"]["license"], "CC-BY-3.0")
        self.assertEqual(catalog.document["source"]["version"], "16")


if __name__ == "__main__":
    unittest.main()
