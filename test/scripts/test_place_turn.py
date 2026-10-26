#!/usr/bin/env python3
"""Check the geodetic turn contract independently of rendering and network inputs."""

import math
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from place_turn import camera_centre, turn_axis, turn_scenario, vector


class PlaceTurn(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-place-turn-")
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / "camera.scenario"
        self.root = ET.fromstring("""<scenario>
<world lat="48" lon="12"/>
<render widthPx="160" heightPx="90"/>
<clock start="2026-01-02T03:04:05Z" live="no"/>
<views><view id="station" fovDeg="30.68">
<at lat="48" lon="12" heightM="492" bearingDeg="185" pitchDeg="-1.6"/>
</view></views></scenario>""")

    def prepare(self, rate=60):
        original = ET.tostring(self.root, encoding="unicode")
        self.source.write_text(original)
        declared, view, velocity = turn_scenario(self.source, rate)
        self.assertEqual(self.source.read_text(), original)
        self.assertEqual(view, "station")
        return ET.fromstring(declared), velocity

    def test_profiles_preserve_station_lens_and_original_final_pose(self):
        for rate in (25, 30, 60):
            with self.subTest(rate=rate):
                declared, velocity = self.prepare(rate)
                views = list(declared.find("views"))
                self.assertEqual(len(views), rate)
                self.assertEqual(velocity, "0,-360,0")
                self.assertEqual(
                    ET.tostring(views[0]), ET.tostring(self.root.find("views/view"))
                )
                for frame, view in enumerate(views):
                    at = view.find("at")
                    self.assertEqual(
                        tuple(
                            float(at.get(key))
                            for key in ("lat", "lon", "heightM", "pitchDeg")
                        ),
                        (48, 12, 492, -1.6),
                    )
                    self.assertEqual(float(view.get("fovDeg")), 30.68)
                    difference = math.remainder(
                        float(at.get("bearingDeg")) - 185 - 360 * frame / rate, 360
                    )
                    self.assertAlmostEqual(difference, 0, delta=1e-12)

    def test_explicit_target_rotates_clockwise_around_the_offset_eye(self):
        world = self.root.find("world")
        world.set("lat", "0")
        world.set("lon", "0")
        view = self.root.find("views/view")
        view.set("offsetX", "2")
        view.set("offsetY", "3")
        view.set("offsetZ", "4")
        at = view.find("at")
        at.set("lat", "0")
        at.set("lon", "0")
        at.set("heightM", "0")
        at.set("samplesHeight", "yes")
        ET.SubElement(view, "lookAt", x="2", y="13", z="-6")
        ET.SubElement(view, "up", x="0", y="1", z="0")
        declared, _ = self.prepare()
        quarter = list(declared.find("views"))[15]
        for actual, expected in zip(vector(quarter.find("lookAt")), (12, 13, 4)):
            self.assertAlmostEqual(actual, expected, delta=1e-12)
        self.assertEqual(vector(quarter.find("up")), (0, 1, 0))
        self.assertEqual(quarter.find("at").get("samplesHeight"), "yes")

        at.set("heightM", "123")
        shifted, _ = self.prepare()
        for first, second in zip(
            vector(list(shifted.find("views"))[15].find("lookAt")),
            vector(quarter.find("lookAt")),
        ):
            self.assertAlmostEqual(first, second, delta=1e-12)

    def test_remote_station_uses_its_own_geodetic_vertical(self):
        world = ET.Element("world", lat="0", lon="0")
        for latitude, longitude, expected in ((0, 90, (-1, 0, 0)), (90, 0, (0, 0, 1))):
            at = ET.Element("at", lat=str(latitude), lon=str(longitude))
            axis = turn_axis(world, at)
            self.assertAlmostEqual(sum(value * value for value in axis), 1, delta=1e-12)
            for actual, wanted in zip(axis, expected):
                self.assertAlmostEqual(actual, wanted, delta=1e-12)

        view = ET.Element("view", offsetX="2", offsetY="3", offsetZ="4")
        ET.SubElement(view, "at", lat="0", lon="90", heightM="12")
        for actual, wanted in zip(camera_centre(world, view), (6378151, -6378134, 4)):
            self.assertAlmostEqual(actual, wanted, delta=2e-8)


if __name__ == "__main__":
    unittest.main()
