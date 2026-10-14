import hashlib
import json
import unittest
from importlib.resources import files

import numpy as np
from outshine_ref.vegetation import LeafOpticsTable


class LeafOpticsAssetTests(unittest.TestCase):
    def test_packaged_table_preserves_source_values_and_spectral_checkpoints(self):
        directory = files("outshine_ref.vegetation") / "data"
        provenance = json.loads(
            (directory / "leaf-optics-provenance-1.json").read_text()
        )
        raw = (directory / provenance["table"]).read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(), provenance["sha256"])
        table = LeafOpticsTable.from_recipe(json.loads(raw))
        self.assertEqual(table.values.shape, (33, 33, 6))
        self.assertEqual(table.values.nbytes, 52272)
        self.assertFalse(provenance["runtime_illumination_baked"])
        self.assertEqual(
            provenance["pigment_axis_order"], ["chlorophyll", "anthocyanin"]
        )
        with self.assertRaises(ValueError):
            table.values.setflags(write=True)
        checks = provenance["validation"]
        result = table.sample(checks["pigments_ug_cm2"])
        error = float(
            np.max(abs(result - np.array(checks["direct_spectral_resolved_rt"])))
        )
        self.assertLessEqual(error, checks["max_abs_error_allowed"])
        self.assertAlmostEqual(error, checks["max_table_error"])
        self.assertTrue(np.all(result >= 0))
        self.assertTrue(
            np.all(result[:, :3].max(axis=1) + result[:, 3:].max(axis=1) <= 1 + 1e-14)
        )


if __name__ == "__main__":
    unittest.main()
