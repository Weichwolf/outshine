"""Typed cache headers, artifact/receipt binding and zero organ-array materialization."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np
from camera_height_inputs import digest
from recipe_plant_operators import prepared
from stair_plant_prior import physical_form, previous, prototype_layout, reuse_prototype


def fixture(root, dtype="f8", definition=None):
    # Trusted-receipt stand-in for I/O tests only; real admission uses the full
    # existing independent emitter/reader, never this miniature triangle.
    source = root / "prior"
    output = source / "prototype"
    output.mkdir(parents=True)
    np.savez_compressed(
        output / "complete.npz",
        vertices=np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=dtype),
        faces=np.array([[0, 1, 2]], np.uint32),
        leaf_faces=np.array([False]),
    )
    proto = {
        "vertices": 3,
        "triangles": 1,
        "definition": definition or prepared().definition_fingerprint,
        "recipe": {"wood": {"seed": 718}},
        "artifacts": {"complete.npz": digest(output / "complete.npz")},
    }
    (output / "prototype.json").write_text(json.dumps(proto))
    (source / "plants.json").write_text("[]\n")
    (source / "landcover-ledger.json").write_text("[]\n")
    report = {
        "commit": "unit-fixture",
        "prototype_vertices": 3,
        "prototype_triangles": 1,
        "prototype_array_bytes": 84,
        "artifacts": {
            str(p.relative_to(source)): digest(p)
            for p in source.rglob("*")
            if p.is_file()
        },
    }
    (source / "comparison.json").write_text(json.dumps(report))
    receipt = root / "receipt.json"
    receipt.write_text(
        json.dumps(
            {
                "report_sha256": digest(source / "comparison.json"),
                "producer_commit": "unit-fixture",
                "whole_form_replayed_exact": True,
                "all_physical_bounds_and_SITE_products_verified": True,
                "actual_complete_plants": 0,
            }
        )
    )
    return source, receipt, proto


class PriorPlantControls(unittest.TestCase):
    def test_verified_typed_payload_is_linked_without_numpy_load_or_emission(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-plant-cache-"
        ) as root:
            root = Path(root)
            source, receipt, _ = fixture(root)
            before = digest(source / "prototype/complete.npz")
            with (
                patch("numpy.load", side_effect=AssertionError("full array decode")),
                patch(
                    "stair_plant_geometry.prototype",
                    side_effect=AssertionError("emission"),
                ),
            ):
                prior = previous(source, receipt)
                proto, artifacts, work = physical_form(root / "cached", prior)
            self.assertEqual(proto, prior["prototype"])
            self.assertEqual(work["materialized_organ_array_bytes"], 0)
            self.assertEqual(work["logical_prototype_array_bytes"], 84)
            self.assertEqual(work["copied_payload_bytes"], 0)
            self.assertEqual(work["linked_files"], 2)
            self.assertEqual(before, digest(root / "cached/complete.npz"))
            self.assertEqual(before, digest(source / "prototype/complete.npz"))
            self.assertEqual(artifacts["prototype/complete.npz"], before)

    def test_changed_payload_and_unverified_receipt_are_refused(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-plant-cache-"
        ) as root:
            source, receipt, _ = fixture(Path(root))
            data = json.loads(receipt.read_text())
            data["whole_form_replayed_exact"] = False
            receipt.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                previous(source, receipt)
            data["whole_form_replayed_exact"] = True
            receipt.write_text(json.dumps(data))
            with (source / "prototype/complete.npz").open("ab") as output:
                output.write(b"changed")
            with self.assertRaisesRegex(ValueError, "artifact changed"):
                previous(source, receipt)

    def test_a_hash_consistent_but_wrong_array_abi_is_not_reinterpreted(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-plant-cache-"
        ) as root:
            source, receipt, proto = fixture(Path(root), dtype="f4")
            with self.assertRaisesRegex(ValueError, "shape/dtype"):
                previous(source, receipt)
            with self.assertRaises(ValueError):
                prototype_layout(
                    source / "prototype/complete.npz", dict(proto, triangles=2)
                )

    def test_changed_definition_cannot_reuse_old_physical_buffers(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-plant-cache-"
        ) as root:
            root = Path(root)
            source, receipt, _ = fixture(root, definition="0" * 64)
            prior = previous(source, receipt)
            self.assertFalse(prior["compatible"])
            with self.assertRaisesRegex(ValueError, "matching current form"):
                reuse_prototype(prior, root / "must-not-exist")
            self.assertFalse((root / "must-not-exist").exists())


if __name__ == "__main__":
    unittest.main()
