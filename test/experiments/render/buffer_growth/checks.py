import gzip
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from replay import MAX_BYTES, Arena, replay

FIXTURES = Path(__file__).parent / "fixtures"


def grow(arena, required, policy="doubling_max"):
    arena.request(("G", "0", 0, required, 8, 1, 1, 0, 0, 0, 0), policy, False)


class GrowthChecks(unittest.TestCase):
    def test_large_reservations_do_not_skip_past_the_requested_range(self):
        arena = Arena()
        grow(arena, 64)
        grow(arena, 192)
        self.assertEqual(arena.buffers[0].capacity, 192)
        self.assertEqual(arena.copied, 64)
        self.assertEqual(arena.allocations, 2)

    def test_original_doubling_preserves_unaligned_byte_reservations(self):
        for initial in (3, 5, 65):
            with self.subTest(initial=initial):
                arena = Arena()
                for required in (initial, initial * 2):
                    arena.request(
                        ("G", "0", 0, required, 8, 0, 1, 0, 0, 0, 0),
                        "doubling",
                        False,
                    )
                self.assertEqual(arena.buffers[0].capacity, initial * 2)

    def test_small_requests_keep_geometric_headroom_and_amortized_copy_bound(self):
        arena = Arena()
        for required in range(4, 65540, 4):
            grow(arena, required)
            self.assertGreaterEqual(arena.buffers[0].capacity, required)
            self.assertLess(arena.buffers[0].capacity, required * 2)
            self.assertLess(arena.copied, arena.buffers[0].capacity)
        self.assertLessEqual(arena.allocations, 16)

    def test_growth_saturates_without_wrapping_or_looping(self):
        arena = Arena()
        grow(arena, MAX_BYTES - 7)
        grow(arena, MAX_BYTES - 3)
        self.assertEqual(arena.buffers[0].capacity, MAX_BYTES)
        self.assertEqual(arena.copied, MAX_BYTES - 7)

    def test_changed_usage_preserves_capacity_and_released_sources_reset_it(self):
        arena = Arena()
        grow(arena, 64)
        arena.request(("G", "0", 0, 16, 32, 1, 1, 0, 0, 0, 0), "doubling_max", False)
        self.assertEqual(arena.buffers[0].capacity, 64)
        self.assertEqual(arena.copied, 64)
        arena.request(("X", "0", 0, 0, 8, 0, 0, 0, 0, 0, 0), "doubling_max", False)
        self.assertFalse(arena.buffers[0].present)
        self.assertEqual(arena.buffers[0].capacity, 0)

    def test_native_traces_reproduce_every_observed_state_and_final_buffer(self):
        manifest = json.loads((FIXTURES / "manifest.json").read_text())
        self.assertEqual(manifest["schema"], "outshine/buffer-demand-trace/1")
        for fixture in manifest["fixtures"]:
            with self.subTest(trace=fixture["file"]):
                path = FIXTURES / fixture["file"]
                self.assertEqual(
                    hashlib.sha256(path.read_bytes()).hexdigest(), fixture["sha256"]
                )
                self.assertEqual(
                    hashlib.sha256(gzip.decompress(path.read_bytes())).hexdigest(),
                    fixture["decoded_sha256"],
                )
                baseline = replay(path, "doubling", True)
                self.assertEqual(baseline["counts"], fixture["events"])
                original = next(
                    arena
                    for arena in baseline["arenas"]
                    if arena["owner"] == fixture["primary_owner"]
                )
                self.assertEqual(original["streams"], fixture["streams"])
                self.assertEqual(original["capacity"], fixture["capacity"])
                self.assertEqual(original["copy_bytes"], fixture["observed_copy_bytes"])
                candidate = replay(path, "doubling_max", False)
                changed = next(
                    arena
                    for arena in candidate["arenas"]
                    if arena["owner"] == fixture["primary_owner"]
                )
                self.assertEqual(candidate["counts"], baseline["counts"])
                self.assertLess(changed["capacity"], original["capacity"])

    def test_corrupt_native_observations_and_unbounded_requests_are_refused(self):
        good = ["R,0,0,0,0,0,0,0,0,0,0", "G,0,0,64,8,1,1,0,0,0,0"]
        rows = [
            "G,0,0,128,8,1,1,1,63,11,0",
            "G,0,17,64,8,1,1,0,0,0,0",
            f"G,0,0,{MAX_BYTES + 1},8,1,1,0,0,0,0",
        ]
        with tempfile.TemporaryDirectory(
            prefix="outshine-buffer-growth-checks-"
        ) as directory:
            path = Path(directory) / "invalid.csv"
            for row in rows:
                with self.subTest(row=row):
                    path.write_text("\n".join([*good, row]) + "\n")
                    with self.assertRaises(ValueError):
                        replay(path, "doubling", True)


if __name__ == "__main__":
    unittest.main()
