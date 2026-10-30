"""One verified in-call state can serve only its exact whole input lineage."""

import json
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame
from rail_occupancy_checks import products
from rail_occupancy_inputs import bound_state
from stair_occupancy_plant_query import inputs


class BoundStateChecks(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="outshine-bound-state-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.resident = self.root / "resident"
        self.resident.mkdir()
        (self.resident / "resident.json").write_text('{"input":"original"}')
        self.prior = tuple(self.root / name for name in ("first", "second"))
        for i, path in enumerate(self.prior):
            path.mkdir()
            (path / "comparison.json").write_text(json.dumps({"parent": i}))
        self.manifest = {
            "source_ids": ["original"],
            "root_resident_sha256": digest(self.resident / "resident.json"),
            "parent_reports": [digest(p / "comparison.json") for p in self.prior],
        }
        self.state = (
            SimpleNamespace(patches=[SimpleNamespace(frame=LocalFrame("bound"))]),
            (),
            (),
            (),
            self.manifest,
        )
        self.products = replace(
            products(), state=self.state, state_inputs=(self.resident, self.prior)
        )
        self.baseline = self.root / "baseline"
        self.baseline.mkdir()
        (self.baseline / "comparison.json").write_text("{}")
        self.buildings = self.root / "buildings"
        self.buildings.mkdir()
        (self.buildings / "building-reservations.json").write_text("[]")
        self.report = {
            "baseline": str(self.baseline),
            "baseline_report_sha256": digest(self.baseline / "comparison.json"),
            "resident": str(self.resident),
            "prior_modules": list(map(str, self.prior)),
            "source_ids": ["original"],
            "frame": "bound",
            "artifacts": {
                "building-reservations.json": digest(
                    self.buildings / "building-reservations.json"
                )
            },
        }
        self.receipt = self.root / "reader.json"
        self.save_report()

    def save_report(self):
        (self.buildings / "comparison.json").write_text(json.dumps(self.report))
        self.receipt.write_text(
            json.dumps(
                {
                    "report_sha256": digest(self.buildings / "comparison.json"),
                    "all_source_attributes_roots_heights_and_whole_body_conflicts_checked": True,
                    "actual_adjusted_bodies": 0,
                }
            )
        )

    def test_reuse_retains_exact_owned_state_without_second_replay(self):
        self.assertIs(
            bound_state(self.products, self.resident / ".", list(self.prior)),
            self.state,
        )
        with (
            patch(
                "stair_occupancy_plant_query.load_chain",
                side_effect=AssertionError("must not replay again"),
            ),
            patch("stair_occupancy_plant_query.data_for", return_value={}) as data,
        ):
            result = inputs(
                self.buildings,
                self.receipt,
                state_loader=lambda *args: bound_state(self.products, *args),
            )
        self.assertIs(result[1], self.state)
        data.assert_called_once_with(self.baseline, self.state)

    def test_default_reader_still_loads_its_own_whole_state(self):
        with (
            patch(
                "stair_occupancy_plant_query.load_chain", return_value=self.state
            ) as replay,
            patch("stair_occupancy_plant_query.data_for", return_value={}),
        ):
            self.assertIs(inputs(self.buildings, self.receipt)[1], self.state)
        replay.assert_called_once_with(self.resident, list(self.prior))

    def test_other_paths_or_parent_order_fail_even_with_same_ids(self):
        for resident, prior in (
            (self.root / "other", self.prior),
            (self.resident, self.prior[::-1]),
            (self.resident, self.prior[:1]),
        ):
            with self.assertRaisesRegex(ValueError, "paths or order"):
                bound_state(self.products, resident, prior)
        with self.assertRaisesRegex(ValueError, "paths or order"):
            bound_state(
                replace(self.products, state_inputs=None), self.resident, self.prior
            )

    def test_changed_root_or_parent_bytes_and_missing_binding_facts_fail(self):
        for path in (
            self.resident / "resident.json",
            self.prior[0] / "comparison.json",
        ):
            before = path.read_bytes()
            path.write_bytes(before + b" ")
            with self.assertRaisesRegex(ValueError, "reports changed"):
                bound_state(self.products, self.resident, self.prior)
            path.write_bytes(before)
        for field in ("root_resident_sha256", "parent_reports"):
            manifest = {
                key: value for key, value in self.manifest.items() if key != field
            }
            altered = replace(self.products, state=(*self.state[:4], manifest))
            with self.assertRaisesRegex(ValueError, "reports changed"):
                bound_state(altered, self.resident, self.prior)

    def test_rebound_wrong_sources_or_frame_fail_before_data_work(self):
        for field, wrong in (("source_ids", ["different"]), ("frame", "different")):
            before = self.report[field]
            self.report[field] = wrong
            self.save_report()
            with (
                patch(
                    "stair_occupancy_plant_query.data_for",
                    side_effect=AssertionError("must reject first"),
                ),
                self.assertRaisesRegex(ValueError, "source order or named frame"),
            ):
                inputs(
                    self.buildings,
                    self.receipt,
                    state_loader=lambda *args: bound_state(self.products, *args),
                )
            self.report[field] = before
        self.save_report()


if __name__ == "__main__":
    unittest.main()
