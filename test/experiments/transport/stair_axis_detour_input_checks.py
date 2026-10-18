"""Changing a real pier requires a new input, never a relaxed historical pin."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from stair_axis_detour_inputs import current_recipe
from stair_axis_detours import bind

FIXTURES = Path(__file__).with_name("fixtures")


class CurrentPierInputs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with np.load(FIXTURES / "stair-pier-detour-obstacle-vienna.npz") as data:
            cls.old = Mesh(data["vertices"], data["faces"])
        cls.template = json.loads(
            (FIXTURES / "stair-pier-detour-vienna.json").read_text()
        )
        cls.piers = [cls.old] * 124

    def test_actual_height_change_needs_a_new_binding_and_preserves_template(self):
        before = copy.deepcopy(self.template)
        piers = list(self.piers)
        piers[123] = Mesh(self.old.vertices + [0, 0, 2.2e-6], self.old.triangles)
        bind(self.piers, self.template)
        with self.assertRaisesRegex(ValueError, "geometry differs"):
            bind(piers, self.template)
        prepared, changes = current_recipe(self.template, piers)
        bind(piers, prepared)
        self.assertEqual(self.template, before)
        self.assertEqual(len(changes), 1)
        self.assertNotEqual(
            changes[0]["template_pier_xyz_sha256"],
            changes[0]["current_pier_xyz_sha256"],
        )
        with self.assertRaisesRegex(ValueError, "geometry differs"):
            bind(self.piers, prepared)

    def test_current_input_still_rejects_a_later_geometry_change(self):
        prepared, _changes = current_recipe(self.template, self.piers)
        changed = list(self.piers)
        changed[123] = Mesh(self.old.vertices + [0.001, 0, 0], self.old.triangles)
        with self.assertRaisesRegex(ValueError, "geometry differs"):
            bind(changed, prepared)

    def test_missing_or_invalid_explicit_selections_are_rejected(self):
        for index in (-1, 124, True, 1.5):
            row = copy.deepcopy(self.template)
            next(iter(row["source_axis_detours"].values()))["pier_index"] = index
            with self.subTest(index=index), self.assertRaises(ValueError):
                current_recipe(row, self.piers)
        with self.assertRaises(ValueError):
            current_recipe(self.template, [])
        with self.assertRaises(ValueError):
            current_recipe(dict(self.template, source_ids=[]), self.piers)


if __name__ == "__main__":
    unittest.main()
