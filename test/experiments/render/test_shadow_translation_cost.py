"""Check reuse preconditions and overlap counts against independent pixel enumeration."""

import copy
import gzip
import json
import math
import unittest

from shadow_translation_cost import FIXTURE, compare, overlap

IDENTITY = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]


class ShadowTranslationCost(unittest.TestCase):
    def test_native_matrices_match_observed_packets(self):
        fixture = json.loads(gzip.decompress(FIXTURE.read_bytes()))
        result = compare(fixture)
        self.assertEqual((result["frames"], result["region_updates"]), (60, 36))
        self.assertEqual(result["original_shadow_packets"], 12119)
        self.assertEqual(result["incremental_exposed_texels"], 83963)
        self.assertIsNone(result["incremental_shadow_packets"])
        self.assertIsNone(result["gpu_times_ms"])

    def test_all_axis_signs_diagonals_and_large_jumps_against_pixel_enumeration(self):
        side = 8
        for dx in range(-9, 10):
            for dy in range(-9, 10):
                if dx == dy == 0:
                    continue
                matrix = IDENTITY.copy()
                matrix[12], matrix[13] = 2 * dx / side, -2 * dy / side
                result = overlap(IDENTITY, matrix, side)
                retained = sum(
                    0 <= x - dx < side and 0 <= y - dy < side
                    for y in range(side)
                    for x in range(side)
                )
                self.assertEqual(result["retained"], retained)
                self.assertEqual(result["exposed"], side * side - retained)
                self.assertEqual(result["copy_payload_bytes"], 2 * retained * 4)
                self.assertEqual(result["shift"], [dx, dy])

    def test_fractional_motion_depth_scale_rotation_and_nonfinite_values(self):
        for index in (0, 1, 2, 6, 10, 12, 13, 14, 15):
            matrix = IDENTITY.copy()
            matrix[index] += 0.03125
            self.assertEqual(overlap(IDENTITY, matrix, 8)["retained"], 0)
        matrix = IDENTITY.copy()
        matrix[14] = math.nan
        with self.assertRaises(ValueError):
            overlap(IDENTITY, matrix, 8)
        self.assertIsNone(overlap(IDENTITY, IDENTITY.copy(), 8))

    def test_capture_corruption_cannot_keep_a_matching_claim(self):
        fixture = json.loads(gzip.decompress(FIXTURE.read_bytes()))
        changed = copy.deepcopy(fixture)
        changed["frames"][11]["world_matrices"][2][14] += 0.01
        with self.assertRaisesRegex(ValueError, "observed packet masks differ"):
            compare(changed)
        changed = copy.deepcopy(fixture)
        changed["frames"].pop()
        with self.assertRaises(ValueError):
            compare(changed)


if __name__ == "__main__":
    unittest.main()
