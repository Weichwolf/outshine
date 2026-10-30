"""The real shared fixture stays byte-bound, deduplicated and non-World."""

import json
import os
import tempfile
import unittest
from pathlib import Path

from field_external_depth import compose_registered
from field_native_fixture import DATA, read_fixture


class NativeFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, cls.status, cls.samples, cls.work = read_fixture()

    def test_real_native_payload_aliases_and_cameras_are_preserved(self):
        buffers = {
            id(value)
            for sample in self.samples.values()
            for value in (sample.rgba, sample.reverse_z)
        }
        expected = {v["array"] for v in self.manifest["arrays"].values()}
        self.assertEqual(len(buffers), len(expected))
        self.assertEqual(self.work["physical_arrays"], len(expected))
        self.assertEqual(self.work["logical_arrays"], 48)
        for sample in self.samples.values():
            self.assertFalse(
                sample.rgba.flags.writeable or sample.reverse_z.flags.writeable
            )
            self.assertIsNone(sample.binding.world_revision)
            self.assertIsNone(sample.binding.lighting)
            self.assertEqual(len(sample.binding.sample_revision), 2)
        viewer, moved = (self.samples["stored", k] for k in ("viewer", "moved"))
        self.assertEqual(viewer.binding.capture, moved.binding.capture)
        self.assertNotEqual(viewer.binding.pbr_view, moved.binding.pbr_view)

    def test_complete_known_fixture_points_do_not_retire_a_world_parent(self):
        a, b = self.samples["stored", "viewer"], self.samples["terrain", "viewer"]
        actual = compose_registered(a, b, wanted_a=a.binding, wanted_b=b.binding)
        self.assertEqual(actual.reason, "unknown_world_revision")
        self.assertIsNone(actual.rgba)
        fixture = compose_registered(
            a, b, wanted_a=a.binding, wanted_b=b.binding, diagnostic_fixture=True
        )
        self.assertEqual(fixture.domain, "fixture")
        self.assertEqual(self.status["admission"], "diagnostic-only")

    def test_unknown_lighting_cases_cannot_be_mixed_by_the_same_observer(self):
        a, b = self.samples["stored", "moved"], self.samples["terrain", "light"]
        self.assertEqual(a.binding.pbr_view, b.binding.pbr_view)
        actual = compose_registered(
            a, b, wanted_a=a.binding, wanted_b=b.binding, diagnostic_fixture=True
        )
        self.assertIsNone(actual.rgba)

    def test_metadata_world_forgery_or_wrong_physical_alias_reject(self):
        with tempfile.TemporaryDirectory(prefix="outshine-native-fixture-") as stage:
            root = Path(stage)
            folder = root / DATA.name
            folder.mkdir()
            for name in (
                "manifest.json",
                "reference-status.json",
                "controls.json",
                "samples.npz",
            ):
                if name == "samples.npz":
                    os.link(DATA / name, folder / name)
                else:
                    (folder / name).write_bytes((DATA / name).read_bytes())
            original_parent = root / "building-depth-samples"
            original_parent.mkdir()
            (original_parent / "manifest.json").write_bytes(
                (DATA.parent / "building-depth-samples/manifest.json").read_bytes()
            )
            (original_parent / "original-geometry").symlink_to(
                DATA.parent / "building-depth-samples/original-geometry",
                target_is_directory=True,
            )
            status = dict(self.status, world_revision=self.status["fixture_revision"])
            (folder / "reference-status.json").write_text(json.dumps(status))
            with self.assertRaises(ValueError):
                read_fixture(folder)
            (folder / "reference-status.json").write_text(json.dumps(self.status))
            manifest = json.loads((folder / "manifest.json").read_text())
            manifest["arrays"]["stored/viewer/depth"]["sha256"] = "0" * 64
            (folder / "manifest.json").write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                read_fixture(folder)


if __name__ == "__main__":
    unittest.main()
