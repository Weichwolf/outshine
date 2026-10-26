#!/usr/bin/env python3
"""Exercise external catalogues through generic scenario capture and diagnostics."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from places import turn_scenario

ROOT = Path(__file__).resolve().parents[2]
CLIENT = Path(os.environ.get("OUTSHINE_CLIENT", ROOT / "build/outshine")).resolve()
CATALOG = ROOT / "test/outshine/data/places"
DIAGNOSTIC = ROOT / "test/outshine/data/diagnostic-places"


def scenario(lat=12.5, fov=42, width=321):
    return f'''<scenario>
<world lat="{lat}" lon="-23.75" sightM="1000" vegetation="no">
<relief kind="sineGrid" amplitudeM="0"/></world>
<render widthPx="{width}" heightPx="123"/>
<clock start="2026-01-02T03:04:05Z" live="no"/>
<views><view id="station" person="first" fovDeg="{fov}">
<at lat="{lat}" lon="-23.75" heightM="456.25" bearingDeg="73" pitchDeg="-12" samplesHeight="no"/>
</view></views></scenario>'''


class Catalog(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-place-catalog-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.directory = self.work / "catalogue"
        self.directory.mkdir()

    def run_driver(self, *names, success=True, directory=None):
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(ROOT / "test/scripts/places.py"),
                "--client",
                str(CLIENT),
                "--catalog",
                str(directory or self.directory),
                "--cache-dir",
                str(self.work / "sources"),
                "--reports",
                str(self.work / "reports"),
                "--shots",
                str(self.work / "shots"),
                "--preload-seconds",
                "10",
                *names,
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=40,
            cwd=ROOT,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertTrue(result.stderr or result.stdout)
        return result

    def report(self, name):
        return json.loads((self.work / "reports" / name / "benchmark.json").read_text())

    def test_runtime_content_and_removal(self):
        path = self.directory / "NeverCompiledHere.scenario"
        path.write_text(scenario())
        first = json.loads(self.run_driver().stdout)
        self.assertEqual(first["place"], "NeverCompiledHere")
        before = self.report(first["place"])
        self.assertEqual(before["profile"]["width_px"], 321)
        self.assertEqual(before["motion"]["start"]["perspective_fov_deg"], 42)
        prepared = ET.parse(self.work / "reports/NeverCompiledHere/scene.scenario")
        self.assertEqual(prepared.find("views/view/at").get("lat"), "12.5")

        path.write_text(scenario(lat=18.75, fov=51, width=417))
        self.run_driver()
        after = self.report(first["place"])
        self.assertEqual(after["profile"]["width_px"], 417)
        self.assertEqual(after["motion"]["start"]["perspective_fov_deg"], 51)
        self.assertNotEqual(before["declaration_sha256"], after["declaration_sha256"])
        prepared = ET.parse(self.work / "reports/NeverCompiledHere/scene.scenario")
        self.assertEqual(prepared.find("views/view/at").get("lat"), "18.75")
        self.run_driver("UnknownPlace", success=False)
        path.unlink()
        self.run_driver(success=False)

    def test_sorting_and_no_fixed_count(self):
        for name in ("Zulu", "Alpha"):
            (self.directory / f"{name}.scenario").write_text(scenario())
        names = [
            json.loads(line)["place"] for line in self.run_driver().stdout.splitlines()
        ]
        self.assertEqual(names, ["Alpha", "Zulu"])

    def test_no_fallback(self):
        self.run_driver(success=False, directory=self.directory / "absent")
        self.run_driver(success=False)

    def test_malformed_or_invalid_data(self):
        path = self.directory / "Invalid.scenario"
        for text in (
            "<scenario><broken>",
            "<scenario/>",
            scenario(fov=-1),
            scenario(lat=91),
            scenario(width=0),
            scenario().replace("2026-01-02T03:04:05Z", "invalid"),
        ):
            with self.subTest(text=text):
                path.write_text(text)
                self.run_driver(success=False)

    def test_one_geodetic_view_with_other_camera_modes(self):
        path = self.directory / "MixedViews.scenario"
        local = '<view id="local" placement="local"><at x="0" y="10" z="0"/></view>'
        path.write_text(scenario().replace("</views>", local + "</views>"))
        self.assertEqual(json.loads(self.run_driver().stdout)["place"], "MixedViews")
        self.assertTrue(self.report("MixedViews")["world_complete"])

        second = (
            '<view id="other" person="first" fovDeg="42">'
            '<at lat="13" lon="-23" heightM="500" samplesHeight="no"/>'
            "</view>"
        )
        path.write_text(scenario().replace("</views>", second + "</views>"))
        self.assertIn(
            "exactly one geodetic view", self.run_driver(success=False).stderr
        )

        source = (DIAGNOSTIC / "Hockenheimring.scenario").read_text()
        first = source.index('<view id="overview"')
        last = source.index("</view>", first) + len("</view>")
        path.write_text(source[:first] + source[last:])
        self.run_driver(success=False)

    def test_current_catalog_is_a_twelve_view_sample(self):
        paths = sorted(CATALOG.glob("*.scenario"))
        self.assertEqual(len(paths), 12)
        for path in paths:
            with self.subTest(place=path.stem):
                declared, view, _ = turn_scenario(path, 60)
                root = ET.fromstring(declared)
                self.assertIn("vegetation", root.find("world").attrib)
                self.assertEqual(root.find("render").get("antialiasing"), "temporal")
                self.assertEqual(len(root.find("views")), 60)
                self.assertEqual(root.find("views/view").get("id"), view)

    def test_terrain_relative_historical_cameras(self):
        expected = {
            "Venice": (45.438, 12.3358, 30, "11:11"),
            "CentralPark": (40.7968, -73.952, 218.32, "16:56"),
            "Shibuya": (35.6595, 139.7005, 40, "02:41"),
        }
        for name, (lat, lon, bearing, time) in expected.items():
            with self.subTest(place=name):
                declared, _, _ = turn_scenario(DIAGNOSTIC / f"{name}.scenario", 60)
                root = ET.fromstring(declared)
                view = root.find("views/view")
                at = view.find("at")
                self.assertEqual(
                    tuple(
                        float(at.get(key))
                        for key in ("lat", "lon", "heightM", "bearingDeg", "pitchDeg")
                    ),
                    (lat, lon, 60, bearing, -6),
                )
                self.assertEqual(float(view.get("fovDeg")), 55)
                self.assertEqual(at.get("samplesHeight"), "yes")
                self.assertEqual(
                    tuple(
                        int(root.find("render").get(key))
                        for key in ("widthPx", "heightPx")
                    ),
                    (1280, 720),
                )
                self.assertEqual(
                    root.find("clock").get("start"), f"2026-06-21T{time}:00Z"
                )

    def test_empty_offline_cache_does_not_fall_back_to_global_cache(self):
        cache = self.work / "empty-cache"
        cache.mkdir()
        result = subprocess.run(
            [
                str(CLIENT),
                "run",
                "--stats",
                "--offline",
                "--cache-dir",
                str(cache),
                "--view",
                "overview",
                "--quality",
                "refined",
                str(DIAGNOSTIC / "Hockenheimring.scenario"),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=20,
            cwd=ROOT,
        )
        self.assertNotEqual(result.returncode, 0, result.stdout)
        stats = {
            fields[2]: fields[3]
            for fields in (line.split("\t") for line in result.stdout.splitlines())
            if len(fields) == 5 and fields[0] == "STAT"
        }
        self.assertEqual(stats["status"], "failed")
        self.assertEqual(stats["store_hits"], "0")
        self.assertGreater(int(stats["store_misses"]), 0)
        self.assertGreater(int(stats["provider_starts"]), 0)


if __name__ == "__main__":
    unittest.main()
