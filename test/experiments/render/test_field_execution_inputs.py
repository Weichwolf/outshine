"""Actual request bytes, effective cameras and diagnostic authority stay separate."""

import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from field_execution_inputs import read_execution_inputs
from field_native_fixture import DATA, digest, document


class ExecutionInputTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="outshine-execution-inputs-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.manifest = document(DATA / "manifest.json")
        self.manifest_sha = digest(DATA / "manifest.json")
        self.record = document(DATA / "execution-inputs.json")

    def read(self, record):
        (self.root / "execution-inputs.json").write_text(json.dumps(record))
        return read_execution_inputs(self.root, self.manifest, self.manifest_sha)

    @staticmethod
    def rehash(record, role):
        request = record["requests"][role]
        raw = (json.dumps(request["request"], indent=2) + "\n").encode()
        request["input_sha256"] = hashlib.sha256(raw).hexdigest()

    def test_actual_six_positive_two_negative_requests_are_byte_bound(self):
        parameters, proof = self.read(self.record)
        self.assertEqual(len(parameters), 24)
        self.assertEqual(parameters["stored", "viewer"]["sun_elevation_deg"], 45)
        self.assertEqual(parameters["stored", "light"]["sun_elevation_deg"], 50)
        self.assertNotIn("name", parameters["stored", "viewer"])
        self.assertEqual(parameters["stored", "viewer"], parameters["stored", "repeat"])
        self.assertIsNone(proof["exporter"]["runtime_sha256"])
        self.assertIsNone(proof["world_revision"])
        self.assertIsNone(proof["light_revision"])

    def test_stale_input_bytes_and_changed_fixture_or_binary_fail(self):
        self.record["requests"]["stored"]["request"]["queries"][0][
            "sun_elevation_deg"
        ] = 50
        with self.assertRaisesRegex(ValueError, "request bytes"):
            self.read(self.record)
        for key in ("fixture_manifest_sha256", "native_binary_sha256", "native_commit"):
            changed = document(DATA / "execution-inputs.json")
            changed[key] = "different"
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.read(changed)

    def test_rehashed_capture_observer_basis_or_light_mismatch_stays_invalid(self):
        for field in ("capture", "eye", "basis", "light"):
            changed = copy.deepcopy(self.record)
            query = changed["requests"]["stored"]["request"]["queries"][0]
            if field == "capture":
                query["capture"]["eye_m"][0] += 1
            elif field == "eye":
                query["lighting_camera"]["eye"][0] += 1
            elif field == "basis":
                query["lighting_camera"]["basis"][0][0] = 2
            else:
                query["sun_elevation_deg"] = 50
            self.rehash(changed, "stored")
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.read(changed)

    def test_unknown_camera_flag_and_failed_positive_or_successful_negative_reject(
        self,
    ):
        for role, terminal in (
            ("stored", 1),
            ("invalid-observer", 0),
            ("invalid-capture", 0),
        ):
            changed = copy.deepcopy(self.record)
            changed["requests"][role]["terminal_exit"] = terminal
            with self.subTest(role=role), self.assertRaises(ValueError):
                self.read(changed)
        self.record["requests"]["stored"]["request"]["queries"][0]["lighting_camera"][
            "orthographic"
        ] = True
        self.rehash(self.record, "stored")
        with self.assertRaisesRegex(ValueError, "camera schema"):
            self.read(self.record)

    def test_live_authority_and_invented_historical_runtime_hash_reject(self):
        for key in ("world_revision", "light_revision", "runtime_sha256"):
            changed = copy.deepcopy(self.record)
            target = changed["exporter"] if key == "runtime_sha256" else changed
            target[key] = "invented"
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.read(changed)


if __name__ == "__main__":
    unittest.main()
