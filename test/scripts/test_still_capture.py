#!/usr/bin/env python3
"""Exercise default-view accumulation and failed output through the application."""

import os
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get("OUTSHINE_CLIENT", ROOT / "build/outshine")).resolve()
SCENE = """<scenario>
<world lat="0" lon="0" sightM="1000" vegetation="no">
<relief kind="sineGrid" amplitudeM="0"/>
<osm><area kind="building" heightM="20"
points="0.0007,0.0001 0.0007,0.0003 0.0009,0.0003 0.0009,0.0001"/></osm></world>
<render widthPx="160" heightPx="90" fps="60" antialiasing="temporal"/>
<lighting/><clock start="2026-06-21T12:00:00Z" live="no"/>
<views><view id="station" fovDeg="55" offsetX="2" offsetY="3" offsetZ="4">
<at lat="0" lon="0" heightM="80" samplesHeight="no"/>
<lookAt x="22" y="58" z="-56"/><up x="0" y="1" z="0"/>
</view></views></scenario>"""


class StillCapture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-still-capture-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.source = self.work / "scene.scenario"
        self.source.write_text(SCENE)

    def capture(self, output, *arguments):
        return subprocess.run(
            [
                str(CLIENT),
                "run",
                "--rows",
                "--stats",
                "--offline",
                "--cache-dir",
                str(self.work / "sources"),
                "--into",
                str(output),
                *arguments,
                str(self.source),
                "Still",
            ],
            check=False,
            capture_output=True,
            text=True,
            cwd=ROOT,
            timeout=30,
        )

    def test_default_camera_accumulates_and_saves_a_complete_image(self):
        output = self.work / "captures"
        result = self.capture(output)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        rows = [
            line.split("\t")
            for line in result.stdout.splitlines()
            if line.startswith("ROW\t")
        ]
        self.assertEqual(len(rows), 1, result.stdout)
        self.assertEqual(
            (rows[0][1], rows[0][3], rows[0][7], rows[0][18], rows[0][19]),
            ("Still", "1", "120", "0", "-"),
        )
        image = output / f"Still-{rows[0][2]}.png"
        self.assertTrue(image.is_file())
        self.assertEqual(image.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")
        stats = {
            row[2]: row[3]
            for row in (line.split("\t") for line in result.stdout.splitlines())
            if len(row) == 5 and row[0] == "STAT"
        }
        self.assertEqual(stats["status"], "ok")
        self.assertEqual(stats["refined"], "1")

    def test_failed_output_is_not_reported_as_a_successful_capture(self):
        output = self.work / "occupied"
        output.write_bytes(b"existing content")
        result = self.capture(output)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(output.read_bytes(), b"existing content")
        self.assertIn("capture output directory", result.stdout + result.stderr)
        self.assertIn("\tstatus\tfailed\t", result.stdout)

    def test_selected_view_captures_the_requested_refined_world(self):
        output = self.work / "selected"
        result = self.capture(
            output, "--view", "station", "--at-seconds", "0", "--quality", "refined"
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("STAT\tStill\trequested_quality\trefined\tquality", result.stdout)
        self.assertEqual(len(list(output.glob("*.png"))), 1)

    def test_unknown_view_is_rejected_before_unavailable_world_data(self):
        source = ET.fromstring(SCENE)
        world = source.find("world")
        world.remove(world.find("relief"))
        world.remove(world.find("osm"))
        ET.ElementTree(source).write(self.source)
        result = self.capture(
            self.work / "missing", "--view", "missing", "--at-seconds", "0"
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("view selection: the scenario declares no view", result.stderr)
        self.assertNotIn("preload", result.stderr)


if __name__ == "__main__":
    unittest.main()
