"""Transfer parsing and all-level range coverage, independent of native structs."""

import struct
import unittest

import numpy as np
from shadow_bounds import cluster_sphere
from shadow_bounds_oracle import containment
from shadow_capture import CLUSTER, records


def fixture(gap=False):
    vertices = np.zeros((6, 6), dtype="<f4")
    vertices[1, 0], vertices[2, 1] = 1, 1
    vertices[3:] = vertices[:3]
    vertices[3:, 0] += 100
    indices = np.array([0, 1, 2, 3, 4, 5], dtype="<u4")
    clusters = np.zeros(2, CLUSTER)
    clusters["bounds"][:, 3] = 2
    clusters["bounds"][1, 0] = 100
    clusters["range"] = [[3 if gap else 0, 3], [3, 3]]
    clusters["level"] = [0, 1]
    row = np.eye(4, dtype="<f4")
    row[0, 3] = 100
    return b"".join(
        (
            struct.pack("<7Q", 0x819B0A0D, 6, 6, 2, 1, 24, 52),
            vertices.tobytes(),
            indices.tobytes(),
            clusters.tobytes(),
            row.tobytes(),
        )
    )


class ShadowCaptureTests(unittest.TestCase):
    def test_original_index_stream_all_levels_and_row_major_translation(self):
        (piece,) = records(fixture())
        self.assertEqual(piece["provenance"]["cluster_levels"], [0, 1])
        self.assertEqual(piece["provenance"]["uncovered_index_references"], 0)
        np.testing.assert_array_equal(piece["indices"], [[0, 1, 2], [3, 4, 5]])
        bound = cluster_sphere(
            piece["spheres"], [(f * 3, n * 3) for f, n in piece["ranges"]], 6
        )
        self.assertIsNotNone(bound)
        self.assertEqual(containment(piece["vertices"], bound)["outside"], 0)
        self.assertGreater(
            containment(piece["vertices"], piece["spheres"][0])["outside"], 0
        )
        self.assertEqual(piece["instances"][0, 0, 3], 100)
        self.assertEqual(piece["instances"][0, 3, 0], 0)
        (gap,) = records(fixture(gap=True))
        self.assertEqual(gap["provenance"]["uncovered_index_references"], 3)
        self.assertIsNone(
            cluster_sphere(
                gap["spheres"], [(f * 3, n * 3) for f, n in gap["ranges"]], 6
            )
        )

    def test_truncation_footer_magic_stride_and_out_of_range_index_rejected(self):
        original = fixture()
        corrupt = [original[:-1], original + b"x"]
        for offset, value, fmt in [
            (0, 0, "<Q"),
            (48, 48, "<Q"),
            (56 + 6 * 24, 999, "<I"),
        ]:
            data = bytearray(original)
            struct.pack_into(fmt, data, offset, value)
            corrupt.append(data)
        for data in corrupt:
            with self.assertRaises(ValueError):
                list(records(data))


if __name__ == "__main__":
    unittest.main()
