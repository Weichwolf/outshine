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
from place_gpu_profile import GROUPS, metal_encoder_result, read_vulkan_profile

ROOT = Path(__file__).resolve().parents[2]
MATRIX = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


class PlaceBench(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-linux-bench-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.options = SimpleNamespace(bench_profile="rtx3060", reports=self.root,
                                       max_seconds=900, driver="test driver", check_budget=False,
                                       commit="test commit", online=True)
        self.source = ROOT / "test/outshine/data/places/Husum.scenario"
        self.calls = []

    def invoke(self, prepared, view, options, folder, frames, angular, linear, **kwargs):
        root = ET.fromstring(prepared.read_text())
        render = root.find("render")
        rate = int(render.get("fps"))
        self.assertEqual(options.online, folder.name == "warmup")
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
            "cpu_times": {"frame": distribution,
                          "work": {"p50_ms": timing / 4, "p99_ms": timing / 2},
                          "samples": [{"frame_ms": timing, "cpu_work_ms": timing / 4,
                                       "sync_wait_ms": timing * .75}] * frames},
            "gpu_times": {"frame": distribution,
                          "samples": [{"submission": i + 1, "elapsed_ms": timing}
                                      for i in range(frames)]},
            "process_memory": {"peak_resident_bytes": 2**20 * index,
                               "peak_footprint_bytes": 2**21 * index},
            "gpu_memory": {"resource_allocated_bytes": 2**20 * index},
            "phases_ms": {"preload": timing}, "metrics": [],
            "image_row_variation": 2.0,
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

    def test_profiles_measure_still_turn_walk_and_flight_three_times(self):
        for profile, (width, height, rate) in PROFILES.items():
            with self.subTest(profile=profile):
                self.options.bench_profile = profile
                self.calls.clear()
                code, result, table = self.run_driver()
                self.assertEqual(code, 0)
                self.assertEqual((result["width_px"], result["height_px"]), (width, height))
                self.assertEqual(len(self.calls), 16)  # warmup + three repetitions per case
                for case, run, frames, hz, angular, linear in self.calls:
                    self.assertEqual(hz, rate)
                    self.assertEqual(frames, {"Still": 300, "Drehung": rate,
                                              "Gehen": rate * 10, "Flug": rate * 10}[case])
                    self.assertEqual(angular, "0,-360,0" if case == "Drehung" else "0,0,0")
                    self.assertEqual(linear, {"Gehen": "0,0,-1.5",
                                              "Flug": "0,0,-200"}.get(case, "0,0,0"))
                for row in result["rows"]:
                    self.assertEqual(row["cpu_p50_ms"], 7)
                    self.assertEqual(row["cpu_work_p50_ms"], 1.75)
                    self.assertEqual(row["gpu_p99_ms"], 14)
                    self.assertEqual(row["warm_load_ms"], 7)
                    self.assertEqual(row["peak_footprint_bytes"], 2**22)
                    self.assertIsNone(row["draws_max"])
                    self.assertIsNone(row["triangles_max"])
                    self.assertTrue(all(p["p99_ms"] is None for p in row["gpu_passes"].values()))
                    self.assertTrue(row["complete"])
                self.assertIn(f"GPU: {result['rows'][0]['gpu']}; Treiber: test driver", table)
                self.assertEqual(table.count("| Husum |"), 4)

    def test_failed_repetition_cannot_publish_a_successful_partial_median(self):
        def failing(*args, **kwargs):
            if args[3].name == "2":
                raise RuntimeError("device lost")
            return self.invoke(*args, **kwargs)

        code, result, table = self.run_driver(failing)
        self.assertEqual(code, 1)
        self.assertEqual(len(result["rows"]), 4)
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

    def test_flat_picture_rejects_a_reported_complete_world_without_discarding_times(self):
        def blank(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            report["image_row_variation"] = .01
            return report

        code, result, table = self.run_driver(blank)
        self.assertEqual(code, 1)
        for row in result["rows"]:
            self.assertFalse(row["complete"])
            self.assertFalse(row["image_nonblank"])
            self.assertEqual(row["image_row_variation_min"], .01)
            self.assertEqual(row["cpu_p99_ms"], 14)
        self.assertIn("Bildstruktur", table)

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
            report["cpu_times"]["samples"][0] = {
                "frame_ms": 40 + run, "cpu_work_ms": 30 + run, "sync_wait_ms": 10}
            report["cpu_times"]["samples"][-1] = {
                "frame_ms": 200 + run, "cpu_work_ms": 100 + run, "sync_wait_ms": 100}
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

    def test_budget_checks_cpu_work_and_gpu_against_each_m1_profile(self):
        self.options.check_budget = True

        def measured(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            report["cpu_times"]["frame"] = {"p50_ms": 12, "p99_ms": 15}
            report["cpu_times"]["work"] = {"p50_ms": 4, "p99_ms": 5}
            report["gpu_times"]["frame"] = {"p50_ms": 8, "p99_ms": 12}
            return report

        code, result, table = self.run_driver(measured)
        self.assertEqual(code, 1)
        self.assertEqual(result["cpu_work_budget_ms"], 4)
        self.assertEqual(result["gpu_budget_ms"], 10)
        self.assertIn("**5.00**", table)
        self.assertIn("**12.00**", table)
        self.assertIn("Frame Wand p99", table)
        self.options.bench_profile = "iris"
        code, result, _ = self.run_driver(measured)
        self.assertEqual(code, 0)
        self.assertEqual(result["cpu_work_budget_ms"], 8)
        self.assertEqual(result["gpu_budget_ms"], 20)

    def test_metal_opaque_time_includes_culling_prefix_and_compaction(self):
        intervals = [("geometryLeafCull.comp.spv", 105, 117),
                     ("geometryPrefix.comp.spv", 142, 148),
                     ("geometryPrefixBlocks.comp.spv", 146, 148),
                     ("geometryPrefixCarry.comp.spv", 148, 149),
                     ("geometryPrefixFinish.comp.spv", 149, 150),
                     ("geometrySelectedCompact.comp.spv", 152, 154)]
        encoders = [{"label": label, "kind": "compute", "available": True,
                     "ticks": [start, end, 0, 0]} for label, start, end in intervals]
        encoders += [{"label": "opaqueGeometry", "kind": "render", "available": True,
                      "ticks": [112, 125, 124, 135]},
                     {"label": "bufferCopy", "kind": "blit", "available": True,
                      "ticks": [154, 174, 0, 0]}]
        measured = metal_encoder_result({"calibration_gpu": [100, 200],
                                         "calibration_cpu_ns": [0, 1_000_000],
                                         "encoders": encoders})
        self.assertAlmostEqual(measured["groups"]["opaque"], .4)
        self.assertAlmostEqual(measured["encoders"]["bufferCopy"], .2)

    def test_profile_matches_measured_device_and_submissions_and_counts_all_passes(self):
        passes = [{"label": name, "kind": "render", "available": True, "gpu_ns": ns,
                   "draw_calls": calls, "input_primitives": triangles}
                  for name, ns, calls, triangles in
                  (("opaqueGeometry", 1e6, 3, 30), ("ground", 2e6, 4, 40),
                   ("sun shadow", 3e6, 2, 20), ("sky", 4e6, 1, 1),
                   ("transmissiveGeometry", 1e6, 1, 5), ("temporalResolve", 1e6, 1, 1))]
        submissions = [{"serial": serial, "dropped_passes": 0, "passes": copy.deepcopy(passes)}
                       for serial in (1, 10, 11, 20)]
        submissions[0]["passes"][0]["draw_calls"] = 9999
        submissions[-1]["passes"][0]["draw_calls"] = 9999
        submissions[2]["passes"][0]["gpu_ns"] = 4e6
        report = {"motion": {"frames": 2}, "gpu_times": {"samples": [
            {"device": 1, "submission": 11}, {"device": 1, "submission": 10}]}}
        path = self.root / "gpu-profile.1.json"
        profile = {"schema": 1, "backend": "Vulkan", "device": 1, "submissions": submissions}
        path.write_text(json.dumps(profile))
        other = copy.deepcopy(profile)
        other["device"] = 2
        other["submissions"][1]["passes"][0]["draw_calls"] = 8888
        (self.root / "gpu-profile.2.json").write_text(json.dumps(other))
        measured = read_vulkan_profile(self.root, report)
        self.assertEqual([f["submission"] for f in measured["frames"]], [10, 11])
        self.assertEqual(measured["draws_max"], 12)
        self.assertEqual(measured["triangles_max"], 97)
        self.assertEqual(measured["gpu_passes"]["opaque"]["p50_ms"], 3)
        self.assertEqual(measured["gpu_passes"]["opaque"]["p99_ms"], 6)
        self.assertEqual(measured["gpu_passes"]["shadows"]["p99_ms"], 3)
        submissions[1]["passes"][0]["available"] = False
        submissions[1]["passes"][0]["gpu_ns"] = None
        submissions[1]["passes"][0]["input_primitives"] = None
        path.write_text(json.dumps(profile))
        unavailable = read_vulkan_profile(self.root, report)
        self.assertIsNone(unavailable["gpu_passes"]["opaque"])
        self.assertIsNone(unavailable["triangles_max"])
        self.assertEqual(unavailable["draws_max"], 12)
        submissions.pop(2)
        path.write_text(json.dumps(profile))
        missing = read_vulkan_profile(self.root, report)
        self.assertIsNone(missing["gpu_passes"])
        self.assertIsNone(missing["draws_max"])
        self.assertIn("every measured GPU submission", missing["error"])

    def test_peak_frame_counts_and_gpu_pass_percentiles_use_three_run_medians(self):
        def profiled(*args, **kwargs):
            report = self.invoke(*args, **kwargs)
            run = int(args[3].name) if args[3].name != "warmup" else 999
            report["gpu_profile"] = {
                "draws_max": run * 10, "triangles_max": run * 100,
                "gpu_passes": {group: {"p50_ms": run, "p99_ms": run * 2, "mean_ms": run}
                               for group in GROUPS}}
            return report

        code, result, table = self.run_driver(profiled)
        self.assertEqual(code, 0)
        for row in result["rows"]:
            self.assertEqual(row["draws_max"], 20)
            self.assertEqual(row["triangles_max"], 200)
            self.assertEqual(row["gpu_passes"]["opaque"]["p99_ms"], 4)
        self.assertIn("S=4.00, O=4.00, A=4.00, W=4.00, P=4.00", table)


if __name__ == "__main__":
    unittest.main()
