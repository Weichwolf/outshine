"""Historical original bytes, immutable arrays and strict reader separation."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from native_river_inputs import read_v2, surface_rings

FIXTURES = Path(__file__).parent / "fixtures"


class NativeRiverInputsTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(
            (FIXTURES / "feldkirch-ill-native-inputs.json").read_text()
        )
        self.original = self.manifest["historical_v2"]

    def test_original_decode_consumes_all_bytes_and_preserves_selected_originals(self):
        data = read_v2(FIXTURES / self.original["file"], self.original["sha256"])
        self.assertEqual(data["consumed_bytes"], 257884)
        self.assertEqual(
            (len(data["surfaces"]), len(data["rings"]), len(data["courses"])),
            (550, 564, 935),
        )
        selected = self.manifest["original_selection"]["selected_surface_indices"]
        with np.load(FIXTURES / "feldkirch-ill-profile.npz") as old:
            np.testing.assert_array_equal(selected, old["surface_source_indices"])
            np.testing.assert_array_equal(
                data["surfaces"][selected]["level_m"], old["surfaces"][:, 2]
            )
            rings = [r for i in selected for r in surface_rings(data, i)]
            np.testing.assert_array_equal([len(r) for r in rings], old["rings"][:, 1])
            self.assertEqual(sum(map(len, rings)), 485)
        self.assertFalse(data["points_lat_lon"].flags.writeable)
        with self.assertRaises(ValueError):
            data["points_lat_lon"][0, 0] = 0

    def test_observation_is_hash_bound_but_cannot_be_decoded_as_original_v2(self):
        row = self.manifest["observed_v3"]
        path = FIXTURES / row["file"]
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), row["sha256"])
        with self.assertRaisesRegex(ValueError, "WATER2 required"):
            read_v2(path, row["sha256"])

    def test_truncation_trailing_bytes_and_wrong_digest_are_rejected(self):
        data = (FIXTURES / self.original["file"]).read_bytes()
        with tempfile.TemporaryDirectory(prefix="outshine-tests.river-inputs.") as root:
            path = Path(root) / "water.native"
            for changed in (data[:80], data + b"X"):
                path.write_bytes(changed)
                with self.assertRaises(ValueError):
                    read_v2(path, hashlib.sha256(changed).hexdigest())
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "digest"):
                read_v2(path, "0" * 64)

    def test_original_case_keeps_island_provenance_and_physical_centroid_parity(self):
        from compare_native_water_frame import compare, original_case

        data, manifest, legacy = original_case(
            FIXTURES / "feldkirch-ill-native-inputs.json"
        )
        with np.load(FIXTURES / "feldkirch-ill-profile.npz", allow_pickle=False) as old:
            arrays, report = compare(data, manifest, legacy, old)
        self.assertEqual(report["canonical_reference_coverage"]["holes"], 1)
        self.assertEqual(report["canonical_reference_coverage"]["components"], 1)
        self.assertTrue(all(n > 0 for n in report["canonical_member_reference_counts"]))
        np.testing.assert_array_equal(
            arrays["source_original_indices"],
            manifest["original_selection"]["selected_surface_indices"],
        )
        self.assertEqual(report["courses"]["original_segments"], 121)
        self.assertEqual(report["courses"]["omitted_segments"], [])
        self.assertEqual(report["fragment_facets"]["queries_inside"], 461)
        self.assertLess(
            report["fragment_facets"]["independent_proj_inverse_height_error_max_m"],
            64 * np.spacing(6378137.0),
        )


if __name__ == "__main__":
    unittest.main()
