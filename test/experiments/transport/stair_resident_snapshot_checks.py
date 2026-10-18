"""Actual immutable payload identity, profile binding and snapshot baseline countercases."""

import copy
import json
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import stair_module_checks
import stair_resident_profile_checks
from camera_height_inputs import digest
from stair_local_assets import module_road, stored_road
from stair_local_reference import baseline_binding
from stair_resident_snapshot import profile_rows
from stair_resident_snapshot_verify import equal_assets


class SnapshotPayload(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        stair_module_checks.RealGroundModule.setUpClass()
        cls.data = stair_module_checks.RealGroundModule.data

    def test_same_actual_module_payload_retains_physical_faces_and_sha_identity(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-snapshot-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "module.npz"
            np.savez_compressed(path, **self.data)
            before = digest(path)
            with np.load(path) as source:
                kind = type(source)
            getter, seen = kind.__getitem__, []

            def accessed(source, name):
                seen.append(name)
                return getter(source, name)

            with patch.object(kind, "__getitem__", accessed):
                actual = stored_road(path, "local-module")
            self.assertNotIn("surface_vertices", seen)
            self.assertNotIn("targets", seen)
            self.assertEqual(digest(path), before)
            for name in ("body_vertices", "body_faces", "owners"):
                np.testing.assert_array_equal(actual[name], self.data[name])
            np.testing.assert_array_equal(
                actual["top_faces"], self.data["surface_faces"]
            )
            xyz = actual["body_vertices"][actual["bottom_faces"]]
            np.testing.assert_array_equal(
                xyz,
                self.data["body_vertices"][
                    self.data["surface_faces"] + len(self.data["surface_vertices"])
                ],
            )
            self.assertTrue(actual["ground"].all())

    def test_legacy_payload_is_unchanged_and_unknown_mapping_refused_before_io(self):
        road = module_road(self.data)
        with tempfile.TemporaryDirectory(
            prefix="outshine-snapshot-", dir="/tmp"
        ) as temp:
            path = Path(temp) / "road.npz"
            np.savez_compressed(path, **road)
            actual = stored_road(path, "road")
            self.assertEqual(set(actual), set(road))
            for name in road:
                np.testing.assert_array_equal(actual[name], road[name])
            with self.assertRaisesRegex(ValueError, "known immutable"):
                stored_road(path.with_name("absent.npz"), "guessed")
        with self.assertRaisesRegex(ValueError, "paired module"):
            module_road(
                self.data, top_vertex_count=len(self.data["surface_vertices"]) - 1
            )


class SnapshotProfiles(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        stair_resident_profile_checks.ActualResidentCap.setUpClass()
        data = stair_resident_profile_checks.ActualResidentCap
        cls.root = SimpleNamespace(
            identity=data.meta["data_sha256"],
            source_ids=(data.meta["source_id"],),
            rows=(),
            data={"body_vertices": data.vertices, "body_faces": data.faces},
        )
        cls.source = copy.deepcopy(cls.root)
        cls.source.rows = (
            {
                "source_id": data.meta["source_id"],
                "xy": data.meta["xy_m"],
                "width_m": data.meta["width_m"],
            },
        )
        cls.proof = {
            "valid_prepared_root_profiles": True,
            "prepared_manifest_sha256": "actual-manifest-sha",
            "frame": "actual-frame",
            "root_profiles": 1,
        }

    def test_verified_metadata_upgrade_retains_whole_actual_root(self):
        rows = profile_rows(
            self.root, self.source, self.proof, "actual-manifest-sha", "actual-frame"
        )
        self.assertEqual(rows, self.source.rows)
        actual = copy.deepcopy(self.root)
        actual.rows = rows
        self.assertEqual(
            equal_assets([actual], [self.root], self.source),
            len(self.root.data["body_faces"]),
        )

    def test_wrong_identity_vertices_frame_source_order_or_receipt_are_refused(self):
        for kind in ("identity", "vertices", "sources", "frame", "receipt", "count"):
            source, proof = copy.deepcopy(self.source), dict(self.proof)
            if kind == "identity":
                source.identity = "other"
            elif kind == "vertices":
                source.data["body_vertices"][0, 2] += 0.001
            elif kind == "sources":
                source.source_ids = ("other",)
            elif kind == "frame":
                proof["frame"] = "other"
            elif kind == "count":
                proof["root_profiles"] = 2
            else:
                proof["prepared_manifest_sha256"] = "other"
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                profile_rows(
                    self.root, source, proof, "actual-manifest-sha", "actual-frame"
                )

    def test_independent_comparison_rejects_changed_geometry_or_metadata(self):
        for kind in ("face", "width"):
            actual = copy.deepcopy(self.source)
            if kind == "face":
                actual.data["body_faces"][0, 0] += 1
            else:
                actual.rows[0]["width_m"] /= 2
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                equal_assets([actual], [self.root], self.source)


class SnapshotBaseline(unittest.TestCase):
    def test_snapshot_baseline_requires_its_own_verified_resident_manifest(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-snapshot-", dir="/tmp"
        ) as temp:
            root = Path(temp)
            before = {
                "origin_lat_lon": [1, 2],
                "candidate_source_ids": ["original"],
                "artifacts": {"resident.json": "root-sha"},
            }
            path = root / "comparison.json"
            path.write_text(json.dumps(before))
            proof = {
                "valid_prepared_snapshot": True,
                "prepared_manifest_sha256": "root-sha",
                "products_report_sha256": digest(path),
            }
            receipt = root / "verification.json"
            receipt.write_text(json.dumps(proof))
            args = SimpleNamespace(baseline=root, resident=root, prior_module=[])
            manifest = {
                "root_resident_sha256": "root-sha",
                "origin_lat_lon": [1, 2],
                "source_ids": ["original"],
            }
            self.assertEqual(baseline_binding(args, manifest), before)
            with self.assertRaises(ValueError):
                baseline_binding(
                    SimpleNamespace(
                        baseline=root, resident=root / "other", prior_module=[]
                    ),
                    manifest,
                )
            for key in ("prepared_manifest_sha256", "products_report_sha256"):
                receipt.write_text(json.dumps(dict(proof, **{key: "other"})))
                with self.subTest(key=key), self.assertRaises(ValueError):
                    baseline_binding(args, manifest)
