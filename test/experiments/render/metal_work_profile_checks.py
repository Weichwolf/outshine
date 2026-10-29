"""Measured GPU windows retain overlapping compute/raster intervals and missing data."""

import unittest
from copy import deepcopy

from metal_work_profile import (
    frame_timings,
    parse_compute_records,
    parse_records,
    summarize_compute,
)

COMPUTE = (
    "METAL_COMPUTE serial=11 pass=1 valid=1 duration_ns=7800000 "
    "start_ns=100200000 end_ns=108000000 dispatches=2 groups=64 "
    "declared_threads=8192 unavailable_dispatches=0 pipeline=selection,scan\n"
    "METAL_COMPUTE_WORK serial=11 pass=1 declared_threads=8192 pipeline=selection\n"
)
RASTER = (
    "METAL_PASS serial=11 pass=0 valid=1 vertex_ns=300000 fragment_ns=1000000 "
    "vertex_start_ns=100000000 vertex_end_ns=100300000 "
    "fragment_start_ns=100000000 fragment_end_ns=101000000 pipeline=sky.frag.spv\n"
    "METAL_PASS serial=11 pass=2 valid=1 vertex_ns=7000000 fragment_ns=7000000 "
    "vertex_start_ns=103000000 vertex_end_ns=110000000 "
    "fragment_start_ns=104000000 fragment_end_ns=111000000 pipeline=terrain\n"
    "METAL_COMMAND serial=10 valid=1 gpu_ns=99000000\n"
    "METAL_COMMAND serial=11 valid=1 gpu_ns=12000000\n"
)


def benchmark(*durations):
    count = len(durations)
    return {
        "benchmark_version": 1,
        "mode": "scenario",
        "measurement_complete": True,
        "world_complete": True,
        "unrefined_frames": 0,
        "motion": {"frames": count},
        "gpu_times": {
            "complete": True,
            "rendered_frames": count,
            "timed_frames": count,
            "unavailable_frames": 0,
            "frame": {
                "samples": count,
                "mean_ms": sum(durations) / count if count else 0,
                "max_ms": max(durations, default=0),
            },
            "samples": [
                {"device": 1, "submission": 401 + at, "elapsed_ms": duration}
                for at, duration in enumerate(durations)
            ],
        },
    }


def timed(text, report=None):
    raster, _, frames = parse_records(text)
    return frame_timings(text, raster, frames, report or benchmark(12))


class ComputeIntervals(unittest.TestCase):
    def test_preparation_is_excluded_and_stage_intervals_are_not_added(self):
        text = COMPUTE + COMPUTE.replace("serial=11", "serial=10") + RASTER
        raster, _, frames = parse_records(text)
        compute, work = parse_compute_records(text)
        summary = summarize_compute(compute, work, frames)
        self.assertEqual(summary["selection,scan"]["samples"], 1)
        self.assertEqual(summary["selection,scan"]["duration_ms_mean"], 7.8)
        result = frame_timings(text, raster, frames, benchmark(12), compute)
        self.assertEqual(result["frames"][0]["gpu_ms"], 12)
        self.assertEqual(
            [p["kind"] for p in result["peak_passes"]], ["raster", "compute", "raster"]
        )
        self.assertAlmostEqual(
            sum(p["interval_span_ms"] for p in result["peak_passes"]), 16.8
        )

    def test_unavailable_time_is_not_zero_and_missing_work_is_not_complete(self):
        text = COMPUTE.replace("valid=1 duration_ns=7800000", "valid=0 duration_ns=0")
        text = text.replace(
            "start_ns=100200000 end_ns=108000000", "start_ns=0 end_ns=0"
        )
        text = text.replace("unavailable_dispatches=0", "unavailable_dispatches=1")
        compute, work = parse_compute_records(text)
        summary = summarize_compute(compute, work, {11})["selection,scan"]
        self.assertIsNone(summary["duration_ms_mean"])
        self.assertIsNone(summary["declared_threads_mean"])
        self.assertEqual(summary["unavailable_dispatches"], 1)
        raster, _, frames = parse_records(RASTER)
        result = frame_timings(RASTER, raster, frames, benchmark(12), compute)
        self.assertIsNone(result["peak_passes"][1]["interval_span_ms"])

    def test_duplicate_or_inconsistent_compute_samples_are_rejected(self):
        with self.assertRaises(ValueError):
            parse_compute_records(COMPUTE + COMPUTE)
        with self.assertRaises(ValueError):
            parse_compute_records(
                COMPUTE.replace("duration_ns=7800000", "duration_ns=1")
            )


class BenchmarkFrames(unittest.TestCase):
    def test_legacy_and_unpaced_reports_retain_the_same_measured_gpu_window(self):
        legacy = timed(RASTER)
        report = benchmark(12)
        report["benchmark_version"] = 2
        report["profile"] = {"pacing": "unpaced"}
        current = timed(RASTER, report)
        self.assertEqual(current["frames"], legacy["frames"])
        self.assertEqual(current["peak_passes"], legacy["peak_passes"])
        self.assertEqual((legacy["benchmark_version"], legacy["pacing"]), (1, "paced"))
        self.assertEqual((current["benchmark_version"], current["pacing"]), (2, "unpaced"))

    def test_unknown_schema_or_inconsistent_pacing_is_rejected(self):
        for version, profile in ((0, {}), (3, {}), (2, {}), (2, {"pacing": "paced"})):
            with self.subTest(version=version, profile=profile):
                report = benchmark(12)
                report["benchmark_version"] = version
                report["profile"] = profile
                with self.assertRaises(ValueError):
                    timed(RASTER, report)

    def test_invalid_command_records_and_duplicate_raster_passes_are_rejected(self):
        for text in (
            RASTER.replace("gpu_ns=12000000", "gpu_ns=-1"),
            RASTER.replace("gpu_ns=12000000", "missing=1"),
            RASTER.replace("serial=11 valid=1 gpu_ns", "serial=11 valid=2 gpu_ns"),
            RASTER + RASTER.splitlines()[0] + "\n",
        ):
            with self.assertRaises(ValueError):
                timed(text)

    def test_preload_sky_is_excluded_without_assuming_equal_submission_ids(self):
        preload = RASTER.splitlines()[0].replace("serial=11", "serial=9") + "\n"
        preload += "METAL_COMMAND serial=9 valid=1 gpu_ns=99000000\n"
        result = timed(preload + RASTER)
        self.assertEqual(
            result["frames"],
            [
                {
                    "capture_frame": 1,
                    "serial": 11,
                    "native_submission": 401,
                    "gpu_ms": 12,
                }
            ],
        )

    def test_actual_ordered_sequence_selects_both_frames_and_the_peak(self):
        next_frame = RASTER.splitlines()[0].replace("serial=11", "serial=13") + "\n"
        next_frame += "METAL_COMMAND serial=13 valid=1 gpu_ns=13000000\n"
        result = timed(RASTER + next_frame, benchmark(12, 13))
        self.assertEqual([row["serial"] for row in result["frames"]], [11, 13])
        self.assertEqual(result["peak_frame"], 2)

    def test_ambiguous_repeated_window_and_missing_command_are_rejected(self):
        repeated = RASTER.splitlines()[0].replace("serial=11", "serial=12") + "\n"
        repeated += "METAL_COMMAND serial=12 valid=1 gpu_ns=12000000\n"
        for text in (
            RASTER + repeated,
            RASTER.replace("METAL_COMMAND serial=11 valid=1 gpu_ns=12000000\n", ""),
        ):
            with self.assertRaises(ValueError):
                timed(text)

    def test_incomplete_benchmark_is_rejected(self):
        for key, value in (
            ("mode", "calibration"),
            ("world_complete", False),
            ("measurement_complete", False),
            ("unrefined_frames", 1),
        ):
            report = benchmark(12)
            report[key] = value
            with self.assertRaises(ValueError):
                timed(RASTER, report)
        for key, value in (
            ("complete", False),
            ("timed_frames", 0),
            ("rendered_frames", 2),
            ("unavailable_frames", 1),
        ):
            report = benchmark(12)
            report["gpu_times"][key] = value
            with self.assertRaises(ValueError):
                timed(RASTER, report)

    def test_invalid_and_mismatched_gpu_times_are_rejected(self):
        for value in (float("nan"), float("inf"), -1, 13):
            with self.assertRaises(ValueError):
                timed(RASTER, benchmark(value))

    def test_print_rounding_is_bounded_to_half_a_nanosecond(self):
        self.assertEqual(
            timed(RASTER, benchmark(12.0000004))["frames"][0]["gpu_ms"], 12
        )
        with self.assertRaises(ValueError):
            timed(RASTER, benchmark(12.0000006))

    def test_mixed_devices_and_unordered_native_submissions_are_rejected(self):
        original = benchmark(12, 13)
        for key, value in (("device", 2), ("submission", 400)):
            report = deepcopy(original)
            report["gpu_times"]["samples"][1][key] = value
            with self.assertRaises(ValueError):
                timed(RASTER, report)


if __name__ == "__main__":
    unittest.main()
