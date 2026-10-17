"""Root profile metadata must preserve the actual historical recipe and source order."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
from camera_height_inputs import digest
from stair_cycle_inputs import load
from stair_local_prepare import root_rows
from stair_module_inputs import FIXTURE, source_context


class ResidentProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import json

        cls.reference = FIXTURE.with_name(
            json.loads(FIXTURE.read_text())["reference_fixture"]
        )
        cls.loaded = load(cls.reference)
        cls.rows = cls.loaded[1]
        cls.sources = source_context(cls.reference)[1]
        cls.state = {
            "report": {
                "fixture_sha256": digest(cls.reference),
                "candidate_source_ids": [r["source_id"] for r in cls.rows],
                "after": {"body_sha256": "verified-body-binding"},
            }
        }

    def test_actual_axes_widths_and_altered_axis_are_preserved(self):
        with patch("stair_cycle_inputs.load", return_value=self.loaded):
            rows = root_rows(self.reference, self.state)
        self.assertEqual(len(rows), 32)
        equal = [
            np.array_equal(r["xy"], self.sources[r["source_id"]]["xy_m"]) for r in rows
        ]
        self.assertEqual(sum(equal), 31)
        target = next(r for r in rows if r["source_id"].endswith("32651280:3"))
        self.assertEqual(target["width_m"], 2.0)
        self.assertEqual(
            target["resident_profile_provenance"]["body_sha256"],
            "verified-body-binding",
        )
        for before, after in zip(self.rows, rows, strict=True):
            np.testing.assert_array_equal(before["xy"], after["xy"])
            self.assertEqual(before["width_m"], after["width_m"])
            self.assertNotIn("original", after)

    def test_wrong_recipe_is_refused_before_loading(self):
        state = copy.deepcopy(self.state)
        state["report"]["fixture_sha256"] = "wrong"
        with patch("stair_cycle_inputs.load") as loader:
            with self.assertRaisesRegex(ValueError, "verified root recipe"):
                root_rows(self.reference, state)
            loader.assert_not_called()

    def test_missing_or_reordered_sources_are_refused(self):
        for keys in (
            self.state["report"]["candidate_source_ids"][:-1],
            list(reversed(self.state["report"]["candidate_source_ids"])),
        ):
            state = copy.deepcopy(self.state)
            state["report"]["candidate_source_ids"] = keys
            with (
                patch("stair_cycle_inputs.load", return_value=self.loaded),
                self.assertRaisesRegex(ValueError, "ordered root source"),
            ):
                root_rows(self.reference, state)
