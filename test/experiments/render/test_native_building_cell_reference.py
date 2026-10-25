"""Complete source identity, explicit missing geometry and joined native ranges."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from building_source_tree import source_bounds
from compare_building_regional_hierarchy import rest_price
from native_building_cell_reference import join_sources, load_cells
from parent_surface_inputs import open_parent

DATA = Path(__file__).resolve().parents[2] / "outshine/data/render/building-full-view"


def triangle(identity):
    return {
        "positions": np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], np.float32),
        "indices": np.array([[0, 1, 2]], np.int64),
        "binding": np.array([[0, identity, 0, 0, 0, 0]], np.int32),
    }


class NativeBuildingCellReference(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with open_parent(DATA / "original-inputs.tar.gz") as parent:
            _, _, cls.frame = source_bounds(parent.plans, {25, 33, 34, 41})

    def test_join_preserves_source_identity_and_offsets_indices_and_slots(self):
        first, second = triangle(7), triangle(13)
        joined = join_sources([first, second])
        np.testing.assert_array_equal(joined["indices"], [[0, 1, 2], [3, 4, 5]])
        np.testing.assert_array_equal(joined["binding"][:, :2], [[0, 7], [1, 13]])
        np.testing.assert_array_equal(second["indices"], [[0, 1, 2]])
        self.assertEqual(second["binding"][0, 0], 0)

    def test_missing_original_is_required_and_unpriced(self):
        failed = [{"source": 2, "reason": "explicit missing original geometry"}]
        result = rest_price(triangle(1), {"needs_sources": [1, 2]}, failed)
        self.assertEqual(result["required_original_sources"], [1, 2])
        self.assertEqual(result["resolved_original_sources"], [1])
        self.assertEqual(result["known_original_triangles"], 1)
        self.assertEqual(result["unpriced_originals"], failed)
        with self.assertRaisesRegex(ValueError, "must remain required"):
            rest_price(triangle(1), {"needs_sources": [1]}, failed)

    def test_delivered_cells_cover_every_source_or_explicit_failed_original(self):
        source, receipt = load_cells(
            DATA / "native-cells/manifest.json",
            DATA / "original-inputs.tar.gz",
            {25, 33, 34, 41},
            self.frame,
        )
        failures = {f["source"] for f in receipt["unresolved_originals"]}
        self.assertEqual(failures, {3825, 3827})
        self.assertEqual(len(source["indices"]), 46889)
        known = set(map(int, np.unique(source["binding"][:, 1])))
        self.assertEqual(
            known | failures, set(map(int, self.frame["selected_sources"]))
        )
        self.assertFalse(known & failures)
        self.assertEqual(
            sum(c["kind"] == "archived geometry replay" for c in receipt["cells"]), 1
        )

    def test_changed_control_hash_and_wrong_archive_are_rejected(self):
        manifest = json.loads((DATA / "native-cells/manifest.json").read_text())
        for entry in manifest["cells"]:
            entry["file"] = str((DATA / "native-cells" / entry["file"]).resolve())
        with tempfile.TemporaryDirectory(
            prefix="outshine-native-controls-"
        ) as temporary:
            path = Path(temporary) / "manifest.json"
            manifest["cells"][0]["sha256"] = "0" * 64
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                load_cells(path, DATA / "original-inputs.tar.gz", {25}, self.frame)
            manifest["archive"]["sha256"] = "0" * 64
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "archive must match"):
                load_cells(path, DATA / "original-inputs.tar.gz", {25}, self.frame)

    def test_one_cell_cannot_claim_a_complete_regional_source_frame(self):
        with self.assertRaisesRegex(ValueError, "every selected original"):
            load_cells(
                DATA / "native-cells/manifest.json",
                DATA / "original-inputs.tar.gz",
                {25},
                self.frame,
            )


if __name__ == "__main__":
    unittest.main()
