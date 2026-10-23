"""Shared original-derived ready input and independent exact native field control."""

import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
from attributed_prisms import native_inputs
from full_view_reference import load
from prepared_surface_recipes import load_recipes


class ReadyFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view"
        )
        cls.ready = cls.root / "prism-ready"
        cls.manifest = json.loads((cls.ready / "manifest.json").read_text())
        cls.source, cls.document, _, _ = load(cls.root / "cell.json.gz")

    def test_manifest_binds_unchanged_ready_arrays_to_original_input(self):
        manifest = self.manifest
        for name, entry in manifest["files"].items():
            data = (self.ready / name).read_bytes()
            self.assertEqual(len(data), entry["bytes"])
            self.assertEqual(hashlib.sha256(data).hexdigest(), entry["sha256"])
            with np.load(self.ready / name, allow_pickle=False) as arrays:
                self.assertEqual(
                    sum(arrays[key].nbytes for key in arrays.files),
                    manifest["array_payload_bytes"][name],
                )
        for name, key in [
            ("manifest.json", "original_manifest_sha256"),
            ("original-inputs.tar.gz", "original_archive_sha256"),
        ]:
            self.assertEqual(
                hashlib.sha256((self.root / name).read_bytes()).hexdigest(),
                manifest[key],
            )

    def test_complete_BSC3_partition_and_exact_fallback(self):
        recipes, _ = load_recipes(self.root / "cell.json.gz", self.document)
        source = self.source
        with np.load(self.ready / "product.npz", allow_pickle=False) as arrays:
            analytic = set(arrays["prism_binding"][:, 0])
            fallback = set(arrays["fallback_source_id"])
            self.assertFalse(analytic & fallback)
            self.assertEqual(analytic | fallback, set(recipes))
            self.assertEqual(analytic | fallback, set(source["binding"][:, 1]))
            self.assertEqual((len(analytic), len(fallback)), (557, 19))
            faces = arrays["prism_fallback_faces"]
            self.assertEqual(len(faces), 887)
            np.testing.assert_array_equal(
                arrays["fallback_source_id"], source["binding"][faces, 1]
            )
            corners = arrays["fallback_indices"]
            for stored, original in [
                ("positions", "positions"),
                ("uv", "uv"),
                ("normal", "normal"),
                ("metres", "metres"),
            ]:
                np.testing.assert_array_equal(
                    arrays["fallback_" + stored][corners],
                    source[original][source["indices"][faces]],
                )

    def test_exact_control_roundtrips_every_original_corner_and_source(self):
        source = self.source
        inputs = native_inputs(source)
        indices = source["indices"]
        attributes = np.column_stack((inputs["uv"], source["metres"]))
        self.assertTrue(np.all(source["colour"] == 1))
        with np.load(self.ready / "exact-control.npz", allow_pickle=False) as arrays:
            np.testing.assert_array_equal(
                arrays["positions"][arrays["position_indices"]],
                source["positions"][indices].view(np.uint32),
            )
            np.testing.assert_array_equal(
                arrays["attributes"][arrays["attribute_indices"]],
                attributes[indices].view(np.uint32),
            )
            lookup = arrays["normal_code_index"]
            np.testing.assert_array_equal(
                arrays["normal_table"][lookup], source["normal"][indices[:, 0]]
            )
            np.testing.assert_array_equal(
                arrays["code_table"][lookup], inputs["flat_codes"]
            )
            owner = np.repeat(arrays["source_ids"], np.diff(arrays["source_offsets"]))
            np.testing.assert_array_equal(owner, source["binding"][:, 1])
            self.assertEqual(len(owner), 14169)
