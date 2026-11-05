import copy
import tempfile
import unittest
from pathlib import Path

from metal_encoder_profile import benchmark_window, gpurow_window, counter_timeline


def inputs():
    document = {
        "submissions": [
            {"serial": 6, "submission_gpu_ns": 999000000},
            {"serial": 7, "submission_gpu_ns": 2000000},
            {"serial": 8, "submission_gpu_ns": 3000000},
            {"serial": 9, "submission_gpu_ns": 888000000},
        ]
    }
    benchmark = {
        "measurement_complete": True,
        "motion": {"frames": 2},
        "gpu_times": {
            "complete": True,
            "timed_frames": 2,
            "rendered_frames": 2,
            "unavailable_frames": 0,
            "samples": [
                {"device": 1, "submission": 7, "elapsed_ms": 2.0},
                {"device": 1, "submission": 8, "elapsed_ms": 3.0},
            ],
        },
    }
    return document, benchmark


def counter_row(first, last):
    return {"calibration_cpu_ns": [0, 1000000], "calibration_gpu": [0, 1000000],
            "encoders": [{"kind": "compute", "available": True,
                          "ticks": [first, last, 0, 0]}]}


class CounterTimeline(unittest.TestCase):
    def test_overlapping_latencies_are_not_added_as_frame_work(self):
        result = counter_timeline([counter_row(1000000, 21000000),
                                   counter_row(11000000, 31000000)])
        self.assertEqual(result["envelope_sum_ms"], 40)
        self.assertEqual(result["envelope_union_ms"], 30)
        self.assertEqual(result["overlap_ms"], 10)
        self.assertEqual(result["encoder_end_intervals"]["p99_ms"], 10)

    def test_gaps_and_missing_baseline_do_not_invent_samples(self):
        single = counter_timeline([counter_row(1, 1000001)])
        self.assertEqual(single["encoder_end_intervals"]["samples"], 0)
        self.assertIsNone(single["encoder_end_intervals"]["p99_ms"])
        result = counter_timeline([counter_row(1, 1000001), counter_row(3000001, 4000001)])
        self.assertEqual(result["envelope_union_ms"], 2)
        self.assertEqual(result["encoder_end_intervals"]["p99_ms"], 3)

    def test_out_of_order_endings_are_explicit(self):
        result = counter_timeline([counter_row(1, 10000001), counter_row(2, 5000001)])
        self.assertFalse(result["ordered_encoder_endings"])
        self.assertEqual(result["encoder_end_intervals"]["samples"], 0)

    def test_unknown_empty_or_invalid_counters_fail(self):
        for rows in ([], [{**counter_row(1, 2), "encoders": []}],
                     [counter_row(2, 1)], [counter_row(0, 1)]):
            with self.assertRaises(ValueError):
                counter_timeline(rows)

    def test_calibration_and_render_sample_shape_are_checked(self):
        original = counter_row(1, 2)
        for cpu, gpu in (([2, 1], [1, 2]), ([1, 2], [2, 1]),
                         ([0, float("inf")], [0, 1])):
            row = {**original, "calibration_cpu_ns": cpu, "calibration_gpu": gpu}
            with self.assertRaises(ValueError):
                counter_timeline([row])
        original["encoders"][0]["kind"] = "render"
        original["encoders"][0]["ticks"] = [1, 2]
        with self.assertRaises(ValueError):
            counter_timeline([original])


class MetalBenchmarkWindow(unittest.TestCase):
    def test_evicted_preload_is_allowed_only_with_a_complete_matched_window(self):
        document, benchmark = inputs()
        document["dropped_submissions"] = 300
        with self.assertRaises(ValueError):
            benchmark_window(document, benchmark)
        document["retention"] = "latest_completed_submissions"
        self.assertEqual(len(benchmark_window(document, benchmark)), 2)
        document["submissions"] = document["submissions"][2:]
        with self.assertRaises(ValueError):
            benchmark_window(document, benchmark)

    def test_historical_gpurow_keeps_its_exact_tail_sum_check(self):
        document, _ = inputs()
        document["submissions"] = document["submissions"][1:3]
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "shot.log"
            log.write_text("GPUROW\tcontrol\t2\t2\t5\t0\t0\n")
            self.assertEqual(len(gpurow_window(document, log)), 2)
            log.write_text("GPUROW\tcontrol\t2\t2\t6\t0\t0\n")
            with self.assertRaises(ValueError):
                gpurow_window(document, log)

    def test_actual_submissions_exclude_preparation_and_later_readback(self):
        document, benchmark = inputs()
        self.assertEqual(
            [row["serial"] for row in benchmark_window(document, benchmark)], [7, 8]
        )

    def test_equal_total_does_not_admit_a_different_process_window(self):
        document, benchmark = inputs()
        document["submissions"][1]["submission_gpu_ns"] = 3000000
        document["submissions"][2]["submission_gpu_ns"] = 2000000
        with self.assertRaises(ValueError):
            benchmark_window(document, benchmark)

    def test_missing_duplicate_reordered_or_cross_device_samples_fail(self):
        original, benchmark = inputs()
        for samples in (
            [benchmark["gpu_times"]["samples"][0]],
            [benchmark["gpu_times"]["samples"][0]] * 2,
            list(reversed(benchmark["gpu_times"]["samples"])),
            [
                benchmark["gpu_times"]["samples"][0],
                {"device": 2, "submission": 8, "elapsed_ms": 3.0},
            ],
            [
                benchmark["gpu_times"]["samples"][0],
                {"device": 1, "submission": 10, "elapsed_ms": 3.0},
            ],
        ):
            changed = copy.deepcopy(benchmark)
            changed["gpu_times"]["samples"] = samples
            with self.assertRaises(ValueError):
                benchmark_window(original, changed)
        original["submissions"].append(original["submissions"][0])
        with self.assertRaises(ValueError):
            benchmark_window(original, benchmark)

    def test_incomplete_invalid_and_nonfinite_measurements_fail(self):
        document, benchmark = inputs()
        for key, value in (
            ("complete", False),
            ("timed_frames", 1),
            ("rendered_frames", 1),
            ("unavailable_frames", 1),
        ):
            changed = copy.deepcopy(benchmark)
            changed["gpu_times"][key] = value
            with self.assertRaises(ValueError):
                benchmark_window(document, changed)
        for value in (-1.0, float("nan"), float("inf")):
            changed = copy.deepcopy(benchmark)
            changed["gpu_times"]["samples"][0]["elapsed_ms"] = value
            with self.assertRaises(ValueError):
                benchmark_window(document, changed)
        benchmark["measurement_complete"] = False
        with self.assertRaises(ValueError):
            benchmark_window(document, benchmark)


if __name__ == "__main__":
    unittest.main()
