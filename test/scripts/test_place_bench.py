#!/usr/bin/env python3
"""Exercise actual driver orchestration without requiring a graphics device."""

import contextlib
import copy
import io
import json
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from place_bench import PROFILES, benchmark_places, number

ROOT = Path(__file__).resolve().parents[2]
MATRIX = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


class PlaceBench(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-linux-bench-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.options = SimpleNamespace(bench_profile="rtx3060", reports=self.root,
                                       max_seconds=900, driver="test driver", check_budget=False,
                                       commit="test commit")
        self.source = ROOT / "test/outshine/data/places/Husum.scenario"
        self.calls = []

    def invoke(self, prepared, view, options, folder, frames, angular, linear, **kwargs):
        root = ET.fromstring(prepared.read_text())
        render = root.find("render")
        rate = int(render.get("fps"))
        self.calls.append((folder.parent.name, folder.name, frames, rate, angular, linear))
        index = int(folder.name) if folder.name != "warmup" else 0
        # Three distinct values prove aggregation uses all runs, not just the last.
        timing = (999, 3, 20, 7)[index]
        distribution = {"p50_ms": timing, "p99_ms": timing * 2}
        velocity = [float(v) for v in linear.split(",")]
        report = {
            "profile": {"width_px": int(render.get("widthPx")),
                        "height_px": int(render.get("heightPx")), "fps": rate},
            "motion": {"frames": frames, "angular_velocity_deg_s": list(map(float, angular.split(","))),
                       "linear_velocity_mps": velocity,
                       "start": {"world_from_camera": MATRIX, "position_m": [0, 0, 0]},
                       "finish": {"world_from_camera": MATRIX,
                                  "position_m": [v * frames / rate for v in velocity]}},
            "cpu_times": {"frame": distribution, "samples": [{"frame_ms": timing}] * frames},
            "gpu_times": {"frame": distribution,
                          "samples": [{"submission": i + 1, "elapsed_ms": timing}
                                      for i in range(frames)]},
            "process_memory": {"peak_resident_bytes": 2**20 * index},
            "gpu_memory": {"resource_allocated_bytes": 2**20 * index},
            "phases_ms": {"preload": timing}, "metrics": [],
            "measurement_complete": True, "world_complete": True, "unrefined_frames": 0,
            "gpu": {"rtx3060": "RTX 3060", "iris": "Iris Xe Graphics", "a18": "Apple A18 Pro"}
                   [self.options.bench_profile], "cpu": "test CPU",
            "backend": "Metal" if self.options.bench_profile == "a18" else "Vulkan",
            "build": {"commit": "test commit"},
        }
        return copy.deepcopy(report)

    def run_driver(self, invoke=None):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr), patch(
            "place_bench.driver_metadata", return_value={"devices": []}
        ):
            code = benchmark_places({"Husum": self.source}, ["Husum"],
                                    self.options, invoke or self.invoke)
        return code, json.loads((self.root / "results.json").read_text()), stdout.getvalue()

    def test_profiles_measure_300_frame_turn_and_ten_second_flight_three_times(self):
        for profile, (width, height, rate) in PROFILES.items():
            with self.subTest(profile=profile):
                self.options.bench_profile = profile
                self.calls.clear()
                code, result, table = self.run_driver()
                self.assertEqual(code, 0)
                self.assertEqual((result["width_px"], result["height_px"]), (width, height))
                self.assertEqual(len(self.calls), 12)  # warmup + three repetitions per case
                for case, run, frames, hz, angular, linear in self.calls:
                    self.assertEqual(hz, rate)
                    self.assertEqual(frames, rate * 10 if case == "Flug" else 300)
                    self.assertEqual(angular, f"0,{-360 * rate // 300},0"
                                     if case == "Drehung" else "0,0,0")
                    self.assertEqual(linear, "0,0,-100" if case == "Flug" else "0,0,0")
                for row in result["rows"]:
                    self.assertEqual(row["cpu_p50_ms"], 7)
                    self.assertEqual(row["gpu_p99_ms"], 14)
                    self.assertEqual(row["warm_load_ms"], 7)
                    self.assertIsNone(row["draws_last"])
                    self.assertIsNone(row["triangles_last"])
                    self.assertIsNone(row["gpu_pass_ms"])
                    self.assertTrue(row["complete"])
                self.assertIn(f"GPU: {result['rows'][0]['gpu']}; Treiber: test driver", table)
                self.assertEqual(table.count("| Husum |"), 3)

    def test_failed_repetition_cannot_publish_a_successful_partial_median(self):
        def failing(*args, **kwargs):
            if args[3].name == "2":
                raise RuntimeError("device lost")
            return self.invoke(*args, **kwargs)

        code, result, table = self.run_driver(failing)
        self.assertEqual(code, 1)
        self.assertEqual(len(result["rows"]), 3)
        self.assertTrue(all(not row["complete"] for row in result["rows"]))
        self.assertTrue(all("cpu_p50_ms" not in row for row in result["rows"]))
        self.assertIn("nein", table)

    def test_incomplete_world_retains_times_and_explicit_failure(self):
        def incomplete(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            report["world_complete"] = False
            report["unrefined_frames"] = 1
            return report

        code, result, _ = self.run_driver(incomplete)
        self.assertEqual(code, 1)
        self.assertTrue(all(row["cpu_p99_ms"] == 14 for row in result["rows"]))
        self.assertTrue(all(not row["complete"] for row in result["rows"]))

    def test_wrong_profile_and_motion_fail_the_row(self):
        for changed in ("profile", "motion", "gpu", "commit"):
            with self.subTest(changed=changed):
                def wrong(*args, **kwargs):
                    report = self.invoke(*args, **kwargs)
                    if changed == "profile":
                        report["profile"]["width_px"] = 32
                    elif changed == "motion":
                        report["motion"]["finish"]["position_m"] = [1, 0, 0]
                    elif changed == "gpu":
                        report["gpu"] = "llvmpipe"
                    else:
                        report["build"]["commit"] = "old commit"
                    return report

                code, result, _ = self.run_driver(wrong)
                self.assertEqual(code, 1)
                self.assertTrue(all(not row["complete"] for row in result["rows"]))

    def test_missing_gpu_times_are_unavailable_and_fail_budget_check(self):
        def unavailable(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            report["gpu_times"]["frame"] = None
            return report

        self.options.check_budget = True
        code, result, _ = self.run_driver(unavailable)
        self.assertEqual(code, 1)
        self.assertTrue(all(row["gpu_p99_ms"] is None for row in result["rows"]))
        self.assertEqual(number(None), "n/v")
        self.assertEqual(number(17, 1000 / 60), "**17.00**")

    def test_startup_and_peak_keep_spikes_and_their_actual_run_and_frame(self):
        def spikes(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            run = int(args[3].name) if args[3].name != "warmup" else 0
            report["cpu_times"]["samples"][0] = {"frame_ms": 30 + run}
            report["cpu_times"]["samples"][-1] = {"frame_ms": 100 + run}
            report["gpu_times"]["samples"][0]["elapsed_ms"] = 40 + run
            report["gpu_times"]["samples"][-1]["elapsed_ms"] = 200 + run
            report["gpu_times"]["samples"].reverse()
            return report

        code, result, table = self.run_driver(spikes)
        self.assertEqual(code, 0)
        for row in result["rows"]:
            self.assertEqual(row["cpu_extremes"], {"startup_p99_ms": 32, "max_ms": 102,
                                                   "worst_run": 2, "worst_frame": row["frames"]})
            self.assertEqual(row["gpu_extremes"], {"startup_p99_ms": 42, "max_ms": 202,
                                                   "worst_run": 2, "worst_frame": row["frames"]})
        self.assertIn("**102.00** (L2/F300)", table)


if __name__ == "__main__":
    unittest.main()
