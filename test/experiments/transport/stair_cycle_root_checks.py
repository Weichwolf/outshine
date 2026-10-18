"""Explicit root reconstruction never masquerades as a historical comparison."""

import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from camera_height_inputs import digest
from stair_cycle_experiment import base
from stair_cycle_inputs import FIXTURE
from stair_cycle_verify import displayed


class CycleRootInputs(unittest.TestCase):
    def test_reconstruction_binds_only_the_actual_versioned_source_recipe(self):
        args = SimpleNamespace(
            reconstruct=True, base_network=None, base_terrain=None, fixture=FIXTURE
        )
        self.assertEqual(
            base(args), {"mode": "reconstruct", "fixture_sha256": digest(FIXTURE)}
        )

    def test_incomplete_baseline_and_mixed_reconstruction_are_rejected_before_io(self):
        missing = Path("nonexistent-comparison-product")
        for reconstruct, network, terrain in (
            (False, None, None),
            (False, missing, None),
            (False, None, missing),
            (True, missing, None),
            (True, None, missing),
            (True, missing, missing),
        ):
            with (
                self.subTest(reconstruct=reconstruct, network=network, terrain=terrain),
                self.assertRaises(ValueError),
            ):
                base(
                    SimpleNamespace(
                        reconstruct=reconstruct,
                        base_network=network,
                        base_terrain=terrain,
                        fixture=FIXTURE,
                    )
                )

    def test_reconstruction_reader_rejects_a_fabricated_before_image_claim(self):
        with tempfile.TemporaryDirectory(prefix="outshine-transport-root-test-") as tmp:
            output = Path(tmp)
            with self.assertRaisesRegex(ValueError, "previous comparison image"):
                displayed(
                    output,
                    {"before_display_sha256": "0" * 64},
                    None,
                    None,
                    None,
                    comparison=False,
                )
            (output / "before-display.npz").touch()
            with self.assertRaisesRegex(ValueError, "previous comparison image"):
                displayed(output, {}, None, None, None, comparison=False)


if __name__ == "__main__":
    unittest.main()
