"""A complete verified baseline prefix can avoid unused geometry construction."""

import json
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import shapely
import stair_consumer_inputs as consumer
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame
from stair_occupancy_verify import check_export, read


class BaselinePrefixChecks(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="outshine-baseline-prefix-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.resident = self.root / "resident"
        self.resident.mkdir()
        self.original = {
            "source_ids": ["first"],
            "frame": "baseline-prefix",
            "origin_lat_lon": [1, 2],
            "support_sha256": "support-identity",
        }
        (self.resident / "resident.json").write_text(json.dumps(self.original))
        self.paths = tuple(self.root / name for name in ("module-one", "module-two"))
        for i, path in enumerate(self.paths):
            path.mkdir()
            (path / "comparison.json").write_text(
                json.dumps(
                    {"candidate_source_ids": ["first", "second", "third"][: i + 2]}
                )
            )
        self.manifest = dict(
            self.original,
            source_ids=["first", "second", "third"],
            root_resident_sha256=digest(self.resident / "resident.json"),
            parent_reports=[digest(p / "comparison.json") for p in self.paths],
        )
        self.state = (
            SimpleNamespace(
                patches=[SimpleNamespace(frame=LocalFrame("baseline-prefix"))]
            ),
            (),
            (),
            (),
            self.manifest,
        )
        self.report = self.prefix(1)

    def prefix(self, count):
        return {
            "resident": str(self.resident),
            "root_resident_sha256": self.manifest["root_resident_sha256"],
            "prior_modules": list(map(str, self.paths[:count])),
            "parent_reports": self.manifest["parent_reports"][:count],
            "source_ids": self.manifest["source_ids"][: count + 1],
            "frame": "baseline-prefix",
        }

    def test_root_intermediate_and_complete_state_are_exact_prefixes(self):
        for count in range(3):
            self.assertTrue(
                consumer._baseline_is_prefix(self.prefix(count), self.state)
            )

    def test_source_subset_or_other_ids_are_not_the_whole_prefix(self):
        for ids in (["first"], ["second", "first"], ["first", "other"], []):
            with self.assertRaisesRegex(ValueError, "whole baseline"):
                consumer._baseline_is_prefix(
                    dict(self.report, source_ids=ids), self.state
                )

    def test_root_and_parent_bindings_reject_reordered_or_changed_versions(self):
        alternatives = (
            dict(self.report, root_resident_sha256="different"),
            dict(self.prefix(2), prior_modules=list(map(str, self.paths[::-1]))),
            dict(self.report, parent_reports=["different"]),
            dict(self.prefix(2), prior_modules=[str(self.paths[0])] * 2),
            dict(
                self.report, prior_modules=[str(self.root / str(i)) for i in range(33)]
            ),
        )
        for report in alternatives:
            with self.assertRaises(ValueError):
                consumer._baseline_is_prefix(report, self.state)
        for path in (
            self.resident / "resident.json",
            self.paths[0] / "comparison.json",
        ):
            before = path.read_bytes()
            path.write_bytes(before + b" ")
            with self.assertRaises(ValueError):
                consumer._baseline_is_prefix(self.report, self.state)
            path.write_bytes(before)

    def test_frame_origin_support_and_current_source_bindings_are_required(self):
        for field, value in (
            ("frame", "other"),
            ("origin_lat_lon", [2, 1]),
            ("support_sha256", "other"),
            ("source_ids", ["second", "first", "third"]),
        ):
            manifest = dict(self.manifest, **{field: value})
            with self.assertRaisesRegex(ValueError, "whole baseline"):
                consumer._baseline_is_prefix(self.report, (*self.state[:4], manifest))
        with self.assertRaisesRegex(ValueError, "whole baseline"):
            consumer._baseline_is_prefix(dict(self.report, frame="other"), self.state)

    def test_missing_provenance_requires_the_full_path(self):
        self.assertFalse(consumer._baseline_is_prefix(self.report, None))
        for field in (
            "root_resident_sha256",
            "parent_reports",
            "frame",
            "support_sha256",
        ):
            manifest = deepcopy(self.manifest)
            del manifest[field]
            self.assertFalse(
                consumer._baseline_is_prefix(self.report, (*self.state[:4], manifest))
            )
        for field in ("root_resident_sha256", "parent_reports", "frame"):
            report = deepcopy(self.report)
            del report[field]
            self.assertFalse(consumer._baseline_is_prefix(report, self.state))

    def test_candidate_checks_bytes_without_constructing_unused_reference_volumes(self):
        path = self.root / "volumes.json"
        path.write_text(
            json.dumps(
                [
                    {
                        "owner_id": "first-volume",
                        "source_ids": ["first", "second"],
                        "geometry_wkb": shapely.box(0, 0, 1, 1).wkb_hex,
                        "purpose": "transport.body-and-headroom",
                        "origin_xy_m": [0, 0],
                        "lower_plane_m": [0, 0, 0],
                        "upper_plane_m": [1, 0, 0],
                    }
                ]
            )
        )
        report = dict(self.report, volumes={"sha256": digest(path)})
        (self.root / "comparison.json").write_text(json.dumps(report))
        (self.root / "verification.json").write_text(
            json.dumps(
                {
                    "valid_saved_occupancy": True,
                    "products_report_sha256": digest(self.root / "comparison.json"),
                }
            )
        )
        with (
            patch.object(
                consumer,
                "load_chain",
                side_effect=AssertionError("already verified prefix must not rebuild"),
            ),
            patch.object(
                consumer,
                "read",
                side_effect=AssertionError("unused reference volumes must not decode"),
            ),
            patch.object(
                consumer, "actual_sources", side_effect=RuntimeError("past admission")
            ),
        ):
            with self.assertRaisesRegex(RuntimeError, "past admission"):
                consumer.candidate_inputs(self.root, self.state)
            path.write_bytes(path.read_bytes() + b" ")
            with self.assertRaisesRegex(ValueError, "saved volume export"):
                consumer.candidate_inputs(self.root, self.state)

    def test_full_reader_keeps_ownership_and_affine_geometry_validation(self):
        path = self.root / "volumes.json"
        path.write_text("[]")
        report = {"volumes": {"sha256": digest(path)}}
        check_export(path, report)
        with self.assertRaisesRegex(ValueError, "ownership missing"):
            read(path, report)
        path.write_text(
            json.dumps(
                [
                    {
                        "owner_id": "inverted",
                        "geometry_wkb": shapely.box(0, 0, 1, 1).wkb_hex,
                        "purpose": "transport.body-and-headroom",
                        "origin_xy_m": [0, 0],
                        "lower_plane_m": [0, 0, 0],
                        "upper_plane_m": [-1, 0, 0],
                    }
                ]
            )
        )
        with self.assertRaisesRegex(ValueError, "positive affine volume"):
            read(path, {"volumes": {"sha256": digest(path)}})
        too_large = SimpleNamespace(
            stat=lambda: SimpleNamespace(st_size=16 * 1024**2 + 1)
        )
        with (
            patch(
                "stair_occupancy_verify.digest",
                side_effect=AssertionError("must not hash over-budget bytes"),
            ),
            self.assertRaisesRegex(ValueError, "saved volume export"),
        ):
            check_export(too_large, report)


if __name__ == "__main__":
    unittest.main()
