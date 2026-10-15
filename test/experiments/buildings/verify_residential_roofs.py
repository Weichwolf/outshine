"""Real-source roof reservation and independently composed envelope controls."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from provider_contract import attributes
from tile_roof_envelopes import choose, legacy_choice
from verify_roof_envelopes import case, measures, sections


def sources():
    path = Path(__file__).with_name("data") / "residential-roofs.json"
    records = json.loads(path.read_text())
    return [
        {
            "id": row["id"],
            "polygon": shapely.from_wkb(row["footprint_wkb"]),
            "properties": row["source_properties"],
            "context": row["context"],
            "clipped": False,
        }
        for row in records["cases"]
    ]


class ResidentialRoofs(unittest.TestCase):
    def test_real_source_height_footprint_volume_and_boundary_topology(self):
        for row in sources():
            original = copy.deepcopy(row)
            parameters = attributes(row["properties"])
            choice = choose(row, parameters)
            self.assertEqual(legacy_choice(row, parameters).kind, "flat")
            self.assertIn(choice.kind, ("ridge", "shed"))
            result, envelope, _, _ = case(row, parameters, choice, compare=True)
            self.assertEqual(result["comparison_status"], "pass")
            self.assertLessEqual(abs(result["symmetric_volume_error_m3"]), 1e-7)
            self.assertGreaterEqual(choice.eave_m, 2.64)
            self.assertEqual(row["properties"], original["properties"])
            self.assertEqual(row["polygon"].wkb, original["polygon"].wkb)
            self.assertEqual(row["context"], original["context"])
            np.testing.assert_allclose(
                envelope.mesh["vertices"][:, 2].max(),
                parameters["buildingHeight"],
                rtol=0,
                atol=1e-12,
            )

    def test_explicit_roof_types_and_nonresidential_use_are_not_overridden(self):
        for updates in (
            {"roof:shape": "flat"},
            {"roof_shape": "flat"},
            {"roof:shape": "hipped"},
            {"building": "industrial"},
            {"building": "church"},
            {"building": "water_tower"},
        ):
            row = sources()[0]
            row["properties"].update(updates)
            self.assertEqual(choose(row, attributes(row["properties"])).kind, "flat")

    def test_unclassified_clipped_elevated_hidden_or_narrow_bodies_keep_alternative(
        self,
    ):
        for mutation in ("unknown", "clipped", "elevated", "hidden", "narrow", "large"):
            row = sources()[0]
            if mutation == "unknown":
                row["context"] = {}
            elif mutation == "clipped":
                row["clipped"] = True
            elif mutation == "elevated":
                row["properties"]["render_min_height"] = 1.0
            elif mutation == "hidden":
                row["properties"]["hide_3d"] = True
            elif mutation == "narrow":
                row["polygon"] = shapely.box(0, 0, 4, 30)
            else:
                row["polygon"] = shapely.box(0, 0, 20, 20)
            self.assertEqual(choose(row, attributes(row["properties"])).kind, "flat")

    def test_supplied_storeys_reserve_headroom_before_inferred_roof(self):
        row = sources()[0]
        row["properties"].update(render_height=9.0)
        for key in ("levels", "building:levels"):
            row["properties"][key] = 3
            choice = choose(row, attributes(row["properties"]))
            self.assertGreaterEqual(choice.eave_m, 3 * 2.64)
            row["properties"][key] = 4
            self.assertEqual(choose(row, attributes(row["properties"])).kind, "flat")
            del row["properties"][key]

    def test_tiny_legacy_roof_retains_existing_choice(self):
        row = sources()[0]
        row["polygon"] = shapely.box(0, 0, 5, 10)
        parameters = attributes(row["properties"])
        self.assertEqual(choose(row, parameters), legacy_choice(row, parameters))

    def test_invalid_optional_levels_retain_valid_body_instead_of_omitting_it(self):
        for value in ("bad", "NaN", -1, 0, 2.5, True):
            row = sources()[0]
            row["properties"]["building:levels"] = value
            self.assertEqual(choose(row, attributes(row["properties"])).kind, "flat")

    def test_conditioned_section_oracle_rejects_one_millimetre_geometry_shift(self):
        row = sources()[2]
        parameters = attributes(row["properties"])
        _, envelope, _, _ = case(row, parameters, choose(row, parameters))
        displaced = measures(envelope).translate((0.001, 0, 0))
        with self.assertRaisesRegex(ValueError, "sections differ"):
            sections(envelope, displaced)


if __name__ == "__main__":
    unittest.main()
