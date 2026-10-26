"""Current terrain preparation retains baseline admission and skips discarded work."""

import json
import tempfile
import unittest
from contextlib import ExitStack
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import stair_consumer_inputs as consumer
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame, Mesh


def state(name):
    view = SimpleNamespace(patches=[SimpleNamespace(frame=LocalFrame(name))])
    return (
        view,
        (name + "-asset",),
        (name + "-support",),
        (name + "-contact",),
        {"source_ids": ["original"]},
    )


class ConsumerInputs(unittest.TestCase):
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        self.root = Path(
            self.stack.enter_context(
                tempfile.TemporaryDirectory(prefix="outshine-inputs-")
            )
        )
        self.report = {
            "resident": "resident",
            "prior_modules": [],
            "source_ids": ["original"],
        }
        (self.root / "comparison.json").write_text(json.dumps(self.report))
        self.receipt = {
            "valid_saved_occupancy": True,
            "products_report_sha256": digest(self.root / "comparison.json"),
        }
        self.save_receipt()
        source = self.root / "source.json"
        source.write_text(json.dumps({"origin": {"payload_file": "vector.bin"}}))
        self.baseline, self.current = state("baseline"), state("current")
        self.mesh = Mesh(
            np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], float),
            np.array([[0, 1, 2]]),
        )
        self.rows = (object(),)
        self.stack.enter_context(patch.object(consumer, "INPUT", source))
        self.stack.enter_context(
            patch.object(
                consumer,
                "actual_sources",
                return_value=({"origin_lat_lon": [0, 0], "vector": {}},),
            )
        )
        self.stack.enter_context(
            patch.object(consumer, "decode_vector", return_value={})
        )
        self.load = self.stack.enter_context(
            patch.object(consumer, "load_chain", return_value=self.baseline)
        )
        self.read = self.stack.enter_context(
            patch.object(consumer, "read", return_value=(self.rows, []))
        )
        self.whole = self.stack.enter_context(
            patch.object(
                consumer, "whole", return_value=SimpleNamespace(mesh=self.mesh)
            )
        )
        self.sampler = self.stack.enter_context(
            patch.object(consumer, "SurfaceSampler", return_value=object())
        )

    def save_receipt(self):
        (self.root / "verification.json").write_text(json.dumps(self.receipt))

    def test_full_inputs_still_construct_the_baseline_and_keep_reservations(self):
        data = consumer.inputs(self.root)
        self.whole.assert_called_once_with(self.baseline[0])
        self.sampler.assert_called_once_with(self.mesh)
        self.assertIs(data["reservations"], self.rows)
        self.assertEqual(data["frame"], self.baseline[0].patches[0].frame)

    def test_candidate_validates_baseline_but_prepares_only_current_terrain(self):
        data = consumer.candidate_inputs(self.root, self.current)
        self.load.assert_called_once()
        self.read.assert_called_once_with(self.root / "volumes.json", self.report)
        self.whole.assert_called_once_with(self.current[0])
        self.sampler.assert_called_once_with(self.mesh)
        self.assertNotIn("reservations", data)
        self.assertNotIn("report", data)
        self.assertEqual(data["frame"], self.current[0].patches[0].frame)
        for key, index in (
            ("terrain_patches", 0),
            ("assets", 1),
            ("supports", 2),
            ("contacts", 3),
        ):
            self.assertIs(data[key], self.current[index])

    def test_invalid_receipt_and_changed_report_fail_before_state_or_sampler(self):
        for mutation in ("receipt", "report"):
            with self.subTest(mutation=mutation):
                self.receipt["valid_saved_occupancy"] = mutation != "receipt"
                self.receipt["products_report_sha256"] = (
                    "0" * 64
                    if mutation == "report"
                    else digest(self.root / "comparison.json")
                )
                self.save_receipt()
                with self.assertRaisesRegex(ValueError, "verified complete transport"):
                    consumer.candidate_inputs(self.root, self.current)
        self.load.assert_not_called()
        self.sampler.assert_not_called()

    def test_source_mismatch_and_volume_failure_remain_fatal_before_sampler(self):
        self.baseline[4]["source_ids"] = ["different"]
        with self.assertRaisesRegex(ValueError, "source ownership"):
            consumer.candidate_inputs(self.root, self.current)
        self.read.assert_not_called()
        self.baseline[4]["source_ids"] = ["original"]
        self.read.side_effect = ValueError("original volume is invalid")
        with self.assertRaisesRegex(ValueError, "original volume is invalid"):
            consumer.candidate_inputs(self.root, self.current)
        self.sampler.assert_not_called()


if __name__ == "__main__":
    unittest.main()
