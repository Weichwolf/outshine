#!/usr/bin/env python3
"""Exercise the external Places driver at every supported frame-rate profile."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get("OUTSHINE_CLIENT", ROOT / "build/outshine")).resolve()
PLACE = os.environ.get("OUTSHINE_PROFILE_PLACE", "Husum")


def capture(work, rate, preload=120):
    return subprocess.run(
        [
            sys.executable,
            "-B",
            str(ROOT / "test/scripts/places.py"),
            "--client",
            str(CLIENT),
            "--catalog",
            str(ROOT / "test/outshine/data/places"),
            "--fps",
            str(rate),
            "--preload-seconds",
            str(preload),
            "--reports",
            str(work / f"reports-{rate}"),
            "--shots",
            str(work / "build/shots/places"),
            PLACE,
        ],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        timeout=preload + 130,
    )


class PlaceProfiles(unittest.TestCase):
    def test_requested_rate_drives_real_capture(self):
        with tempfile.TemporaryDirectory(
            prefix="outshine-place-profiles-"
        ) as directory:
            work = Path(directory)
            for rate in (25, 30, 60):
                with self.subTest(rate=rate):
                    result = capture(work, rate)
                    self.assertEqual(
                        result.returncode, 0, result.stdout + result.stderr
                    )
                    rows = [
                        json.loads(line) for line in result.stdout.splitlines() if line
                    ]
                    self.assertEqual(len(rows), 1, result.stdout)
                    row = rows[0]
                    self.assertEqual(
                        (row["place"], row["frames"], row["turn_degrees"]),
                        (PLACE, rate, 360),
                        result.stdout,
                    )
                    report = json.loads(Path(row["report"]).read_text())
                    self.assertTrue(report["measurement_complete"])
                    self.assertTrue(report["world_complete"])
                    self.assertEqual(report["unrefined_frames"], 0)
                    self.assertEqual(report["motion"]["frames"], rate)
                    self.assertEqual(
                        report["motion"]["angular_velocity_deg_s"], [0, -360, 0]
                    )
                    self.assertEqual(report["profile"]["pacing"], "unpaced")
                    self.assertGreater(report["phases_ms"]["unpaced_window"], 0.0)
                    self.assertAlmostEqual(report["motion"]["frames"] * report["motion"]["time_step_s"], 1.0)
                    profile = report["profile"]
                    self.assertEqual(
                        (profile["width_px"], profile["height_px"], profile["fps"]),
                        (1280, 720, rate),
                    )
                    self.assertAlmostEqual(profile["budget_ms"], 1000 / rate, places=6)
                    pictures = list(
                        (work / "build/shots/places").glob(f"{PLACE}-*.png")
                    )
                    self.assertEqual(
                        len(pictures), 1, "only the latest successful shot remains"
                    )
                    self.assertEqual(
                        pictures[0].name, f"{PLACE}-{row['sha256'][:8]}.png"
                    )


if __name__ == "__main__":
    if sys.argv[1:] == ["--prepare"]:
        with tempfile.TemporaryDirectory(
            prefix="outshine-place-profile-prepare-"
        ) as directory:
            result = capture(Path(directory), 60, preload=600)
            if result.returncode:
                raise SystemExit(result.stdout + result.stderr)
    else:
        unittest.main()
