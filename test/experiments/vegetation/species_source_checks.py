"""Independent exact-name, synonym and independent distribution-flag controls."""

import csv
import hashlib
import io
import tempfile
import unittest
import zipfile
from pathlib import Path

from species_source import DIST_FIELDS, NAME_FIELDS, compile_catalog


def fixture(root, *, duplicate=False, flag="0"):
    names = []
    for key, name, status, accepted in (
        ("10", "Fictiona arbor", "Accepted", "10"),
        ("11", "Fictiona prior", "Synonym", "10"),
        ("20", "Fictiona other", "Accepted", "20"),
        ("30", "Fictiona unresolved", "Unplaced", ""),
    ):
        row = dict.fromkeys(NAME_FIELDS, "")
        row.update(
            plant_name_id=key,
            taxon_name=name,
            taxon_rank="Species",
            taxon_status=status,
            accepted_plant_name_id=accepted,
            family="Fictionaceae",
            climate_description="temperate",
            lifeform_description="tree",
            taxon_authors="First & Next",
        )
        names.append(row)
    if duplicate:
        names.append(dict(names[0], plant_name_id="40", accepted_plant_name_id="40"))
    distribution = []
    for key, code, introduced, extinct, doubtful in (
        ("101", "SWI", flag, "0", "0"),
        ("102", "TEX", "1", "0", "0"),
        ("103", "GER", "0", "1", "0"),
        ("104", "AUT", "0", "0", "1"),
    ):
        distribution.append(
            dict(zip(DIST_FIELDS, (key, "10", code, introduced, extinct, doubtful)))
        )
    archive = root / "source.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for member, fields, data in (
            ("wcvp_names.csv", NAME_FIELDS, names),
            ("wcvp_distribution.csv", DIST_FIELDS, distribution),
        ):
            text = io.StringIO()
            writer = csv.DictWriter(
                text, fieldnames=fields, delimiter="|", quoting=csv.QUOTE_NONE
            )
            writer.writeheader()
            writer.writerows(data)
            output.writestr(member, text.getvalue())
    specification = {
        "source": {"sha256": hashlib.sha256(archive.read_bytes()).hexdigest()},
        "requested_names": ["Fictiona prior", "Fictiona arbor"],
    }
    return archive, specification


class SpeciesSourceTests(unittest.TestCase):
    def test_synonym_resolves_once_and_all_flag_axes_remain_independent(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.species-source."
        ) as directory:
            source, specification = fixture(Path(directory))
            catalog = compile_catalog(source, specification)
            specification["requested_names"].reverse()
            self.assertEqual(catalog, compile_catalog(source, specification))
        self.assertEqual(len(catalog["species"]), 1)
        species = catalog["species"][0]
        self.assertEqual(species["id"], "outshine:taxon:wcvp:10")
        self.assertEqual(species["scientific_name"], "Fictiona arbor")
        self.assertEqual(species["authors"], "First & Next")
        self.assertFalse(species["visually_accepted"])
        self.assertIsNone(species["recipe_id"])
        self.assertEqual(
            [
                (r["requested_name"], r["source_status"], r["source_name_id"])
                for r in catalog["requests"]
            ],
            [("Fictiona arbor", "Accepted", "10"), ("Fictiona prior", "Synonym", "11")],
        )
        self.assertEqual(
            {
                r["region"]: (r["introduced"], r["extinct"], r["doubtful"])
                for r in species["distribution"]
            },
            {"SWI": (0, 0, 0), "TEX": (1, 0, 0), "GER": (0, 1, 0), "AUT": (0, 0, 1)},
        )

    def test_ambiguous_unplaced_missing_and_changed_sources_refuse(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.species-source."
        ) as directory:
            root = Path(directory)
            for duplicate, name, reason in (
                (True, "Fictiona arbor", "uniquely"),
                (False, "Fictiona unresolved", "unaccepted"),
                (False, "Unknown species", "uniquely"),
            ):
                source, specification = fixture(root, duplicate=duplicate)
                specification["requested_names"] = [name]
                with self.assertRaisesRegex(ValueError, reason):
                    compile_catalog(source, specification)
            source, specification = fixture(root)
            specification["source"]["sha256"] = "0" * 64
            with self.assertRaisesRegex(ValueError, "pinned snapshot"):
                compile_catalog(source, specification)

    def test_invalid_flags_and_duplicate_requests_refuse(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.species-source."
        ) as directory:
            source, specification = fixture(Path(directory), flag="2")
            with self.assertRaisesRegex(ValueError, "binary"):
                compile_catalog(source, specification)
            specification["requested_names"] *= 2
            with self.assertRaisesRegex(ValueError, "unique exact"):
                compile_catalog(source, specification)


if __name__ == "__main__":
    unittest.main()
