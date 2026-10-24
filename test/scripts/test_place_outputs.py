#!/usr/bin/env python3
"""Protect the latest successful capture, references and unrelated filesystem entries."""

import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from places import keep_latest


class PlaceOutputs(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-place-outputs-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.shots = self.work / "shots"
        self.shots.mkdir()
        self.source = self.work / "incoming.png"
        self.source.write_bytes(b"new")
        self.old = self.shots / "Place-11111111.png"
        self.old.write_bytes(b"previous successful image")

    def test_missing_new_output_preserves_previous_success(self):
        self.source.unlink()
        with self.assertRaises(FileNotFoundError):
            keep_latest(self.source, self.shots, "Place")
        self.assertEqual(self.old.read_bytes(), b"previous successful image")

    def test_success_replaces_only_earlier_regular_files_of_the_same_place(self):
        foreign = (
            "PlaceWest-11111111.png",
            "Place-11111111-reference.png",
            "Place-zzzzzzzz.png",
            "Place.writing",
            "Other-11111111.png",
        )
        for name in foreign:
            (self.shots / name).write_bytes(b"foreign")
        directory = self.shots / "Place-33333333.png"
        directory.mkdir()
        link = self.shots / "Place-44444444.png"
        link.symlink_to(self.source)

        latest, digest = keep_latest(self.source, self.shots, "Place")
        self.assertEqual(digest, hashlib.sha256(b"new").hexdigest())
        self.assertEqual(latest.name, f"Place-{digest[:8]}.png")
        self.assertEqual(latest.read_bytes(), b"new")
        self.assertFalse(self.old.exists())
        for name in foreign:
            self.assertEqual((self.shots / name).read_bytes(), b"foreign")
        self.assertTrue(directory.is_dir())
        self.assertTrue(link.is_symlink())
        self.assertEqual(
            keep_latest(self.source, self.shots, "Place"), (latest, digest)
        )

    def test_failed_write_preserves_previous_success_and_removes_its_temporary_file(
        self,
    ):
        with (
            patch("places.os.fsync", side_effect=OSError("injected write failure")),
            self.assertRaises(OSError),
        ):
            keep_latest(self.source, self.shots, "Place")
        self.assertEqual(self.old.read_bytes(), b"previous successful image")
        self.assertEqual(list(self.shots.iterdir()), [self.old])

    def test_atomic_replace_does_not_write_through_an_existing_output_symlink(self):
        digest = hashlib.sha256(b"new").hexdigest()
        target = self.work / "unrelated.png"
        target.write_bytes(b"unrelated")
        output = self.shots / f"Place-{digest[:8]}.png"
        output.symlink_to(target)
        latest, _ = keep_latest(self.source, self.shots, "Place")
        self.assertEqual(latest, output)
        self.assertFalse(latest.is_symlink())
        self.assertEqual(latest.read_bytes(), b"new")
        self.assertEqual(target.read_bytes(), b"unrelated")


if __name__ == "__main__":
    unittest.main()
