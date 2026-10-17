"""Verify that diagnostic pass intervals belong to the actual captured GPU frames."""

import unittest

from metal_work_profile import frame_timings, parse_records


class MetalFrameAttribution(unittest.TestCase):
    def fixture(self):
        lines = ["METAL_COMMAND serial=1 valid=1 gpu_ns=100000000"]
        for serial, elapsed in ((2, 20000000), (3, 10000000)):
            lines.extend([
                f"METAL_COMMAND serial={serial} valid=1 gpu_ns={elapsed}",
                f"METAL_PASS serial={serial} pass=0 valid=1 vertex_ns=3 fragment_ns=7 calls=1 indirect_calls=0 direct_indices=3 indirect_indices=0 unavailable_indirect=0 vertex_start_ns=10 vertex_end_ns=13 fragment_start_ns=11 fragment_end_ns=18 pipeline=sky.frag.spv",
            ])
        lines.append("GPUROW\tfixture\t2\t2\t30.0000001\t20\t0\t0.001")
        return "\n".join(lines)

    def report(self, text):
        passes, _, frames = parse_records(text)
        return frame_timings(text, passes, frames)

    def test_excludes_preparation_and_keeps_overlap_and_capture_order(self):
        report = self.report(self.fixture())
        self.assertTrue(report["capture_matches_command_timings"])
        self.assertEqual([r["serial"] for r in report["frames"]], [2, 3])
        self.assertEqual(report["peak_frame"], 1)
        self.assertEqual(report["peak_serial"], 2)
        self.assertEqual(report["peak_passes"][0]["interval_span_ms"], 8e-6)
        self.assertEqual(report["peak_passes"][0]["vertex_ms"], 3e-6)
        self.assertEqual(report["peak_passes"][0]["fragment_ms"], 7e-6)

    def test_mismatched_capture_counts_or_sums_are_rejected(self):
        source = self.fixture()
        for changed in (source.replace("\t2\t2\t30", "\t3\t3\t30"),
                        source.replace("30.0000001", "31"),
                        source.replace("\t20\t0", "\t19\t0"),
                        source.replace("\t0\t0.001", "\t1\t0.001")):
            with self.assertRaises(ValueError):
                self.report(changed)

    def test_missing_invalid_or_duplicate_command_records_are_rejected(self):
        source = self.fixture()
        for changed in (source.replace("METAL_COMMAND serial=2 valid=1", "OTHER serial=2 valid=1"),
                        source.replace("serial=2 valid=1 gpu_ns", "serial=2 valid=0 gpu_ns"),
                        source + "\nMETAL_COMMAND serial=2 valid=1 gpu_ns=20000000",
                        source.replace("30.0000001", "nan")):
            with self.assertRaises(ValueError):
                self.report(changed)


if __name__ == "__main__":
    unittest.main()
