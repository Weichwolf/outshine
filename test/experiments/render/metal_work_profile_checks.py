"""Measured GPU windows retain overlapping compute/raster intervals and missing data."""

import unittest

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
    "GPUROW\tfixture\t1\t1\t12\t12\t0\t0\n"
)


class ComputeIntervals(unittest.TestCase):
    def test_preparation_is_excluded_and_stage_intervals_are_not_added(self):
        text = COMPUTE + COMPUTE.replace("serial=11", "serial=10") + RASTER
        raster, _, frames = parse_records(text)
        compute, work = parse_compute_records(text)
        summary = summarize_compute(compute, work, frames)
        self.assertEqual(summary["selection,scan"]["samples"], 1)
        self.assertEqual(summary["selection,scan"]["duration_ms_mean"], 7.8)
        result = frame_timings(text, raster, frames, compute)
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
        result = frame_timings(RASTER, raster, frames, compute)
        self.assertIsNone(result["peak_passes"][1]["interval_span_ms"])

    def test_duplicate_or_inconsistent_compute_samples_are_rejected(self):
        with self.assertRaises(ValueError):
            parse_compute_records(COMPUTE + COMPUTE)
        with self.assertRaises(ValueError):
            parse_compute_records(
                COMPUTE.replace("duration_ns=7800000", "duration_ns=1")
            )


if __name__ == "__main__":
    unittest.main()
