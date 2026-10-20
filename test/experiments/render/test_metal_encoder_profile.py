import copy
import tempfile
import unittest
from pathlib import Path

from metal_encoder_profile import benchmark_window, gpurow_window


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


class MetalBenchmarkWindow(unittest.TestCase):
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
