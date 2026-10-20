"""Finite-lattice conservatism, stale-history recovery and actual native groups."""

import hashlib
import struct
import tarfile
import unittest
from pathlib import Path

import numpy as np
from full_view_reference import load
from native_capture_meshes import fields_at
from native_view_clusters import from_artifact
from two_phase_occlusion import classify, project, pyramid

VIEW = (np.zeros(3), np.eye(3), 4.0, 8, 8)
BOX = np.array([[0, 0], [8, 8]])
BEHIND = np.array([[[-1, -1, 10], [1, 1, 11]]], float)


class DepthTest(unittest.TestCase):
    def test_finite_occluder_and_equal_depth(self):
        history = pyramid(np.full((16, 16), 5.0), VIEW, BOX)
        self.assertTrue(classify(BEHIND, history)[0][0])
        equal = BEHIND.copy()
        equal[:, :, 2] -= 5
        self.assertFalse(classify(equal, history)[0][0])

    def test_hole_near_plane_and_incomplete_crop_keep(self):
        depth = np.full((16, 16), 5.0)
        depth[7, 7] = np.inf
        self.assertFalse(classify(BEHIND, pyramid(depth, VIEW, BOX))[0][0])
        crossing = BEHIND.copy()
        crossing[0, 0, 2] = -0.1
        self.assertFalse(
            classify(crossing, pyramid(np.full((16, 16), 5.0), VIEW, BOX))[0][0]
        )
        crop = np.array([[3, 3], [5, 5]])
        wide = BEHIND.copy()
        wide[0, :, :2] *= 10
        self.assertFalse(
            classify(wide, pyramid(np.full((4, 4), 5.0), VIEW, crop))[0][0]
        )

    def test_stale_rejection_is_only_a_guess(self):
        # A former foreground occluder was removed or the camera exposed a gap.
        old = pyramid(np.full((16, 16), 5.0), VIEW, BOX)
        deferred, _ = classify(BEHIND, old)
        self.assertTrue(deferred.all())
        current = pyramid(np.full((16, 16), np.inf), VIEW, BOX)
        discarded, _ = classify(BEHIND, current, eligible=deferred)
        self.assertFalse(discarded.any())
        self.assertEqual(int((deferred & ~discarded).sum()), 1)

    def test_hierarchy_culls_only_if_all_raw_lattice_points_are_blocked(self):
        rng = np.random.default_rng(891)
        depth = rng.uniform(1, 4, (16, 16))
        depth[::5, ::5] = np.inf
        history = pyramid(depth, VIEW, BOX)
        bounds = []
        for _ in range(256):
            centre = rng.uniform(-5, 5, 3)
            centre[2] = rng.uniform(8, 12)
            bounds.append([centre - [2, 2, 0.5], centre + [2, 2, 0.5]])
        masks, _ = classify(np.array(bounds), history)
        yy, xx = np.meshgrid(
            (np.arange(16) + 0.5) / 2, (np.arange(16) + 0.5) / 2, indexing="ij"
        )
        for bound, culled in zip(bounds, masks, strict=True):
            if not culled:
                continue
            _, (rectangle, nearest) = project(np.array(bound), VIEW)
            inside = (
                (xx >= rectangle[0, 0])
                & (xx <= rectangle[1, 0])
                & (yy >= rectangle[0, 1])
                & (yy <= rectangle[1, 1])
            )
            self.assertTrue(inside.any())
            self.assertTrue(np.isfinite(depth[inside]).all())
            self.assertGreater(nearest, float(depth[inside].max()))
        self.assertTrue(masks.any())

    def test_invalid_fields_fail_closed(self):
        with self.assertRaises(ValueError):
            pyramid(np.full((2, 2), np.nan), VIEW, BOX)
        with self.assertRaises(ValueError):
            pyramid(np.ones((16, 16)), VIEW, BOX, 0)
        self.assertFalse(classify(BEHIND, None)[0].any())


class HitFieldsTest(unittest.TestCase):
    def test_raw_fields_at_known_barycentric_point_and_miss(self):
        source = {
            "positions": np.array([[0, 0, 2], [1, 0, 2], [0, 1, 2]], np.float32),
            "indices": np.array([[0, 1, 2]]),
            "vertex_normal": np.tile([0.0, 0.0, 1.0], (3, 1)),
            "uv": np.array([[0, 0], [2, 0], [0, 4.0]], np.float32),
            "metres": np.array([[0, 0], [3, 0], [0, 6.0]], np.float32),
            "colour": np.array(
                [[1, 0, 0, 1], [0, 1, 0, 1], [0, 0, 1, 1.0]], np.float32
            ),
        }
        fields = fields_at(
            source,
            np.array([[0.25, 0.25, 0], [0.5, 0.5, 0]]),
            np.tile([0.0, 0.0, 1.0], (2, 1)),
            np.array([2, np.inf]),
            np.array([0, -1]),
        )
        np.testing.assert_array_equal(fields["uv"], [[0.5, 1], [0, 0]])
        np.testing.assert_array_equal(fields["metres"], [[0.75, 1.5], [0, 0]])
        np.testing.assert_array_equal(fields["colour"], [[0.5, 0.25, 0.25], [0, 0, 0]])
        np.testing.assert_array_equal(fields["normal"], [[0, 0, 1], [0, 0, 0]])


class OriginalClustersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path("test/outshine/data/render/building-full-view/cell.json.gz")
        cls.source, cls.document, _, _ = load(path)
        with tarfile.open(path.parent / "original-inputs.tar.gz") as archive:
            cls.original = archive.extractfile(
                cls.document["parent"] + "/" + cls.document["view_key"] + ".bin"
            ).read()

    def test_every_original_triangle_belongs_to_one_native_cluster(self):
        g = from_artifact(self.document, self.original, self.source)
        np.testing.assert_array_equal(
            np.sort(g.faces), np.arange(len(self.source["indices"]))
        )
        self.assertEqual(len(g.bounds), 112)
        for a, b, box in zip(g.offsets[:-1], g.offsets[1:], g.bounds, strict=True):
            xyz = self.source["positions"][self.source["indices"][g.faces[a:b]]]
            self.assertTrue(((xyz >= box[0]) & (xyz <= box[1])).all())

    def test_overlapping_cluster_range_rejected_even_with_valid_archive_sha(self):
        original = bytearray(self.original)
        at = len(b"outshine-structure") + 4 + 64
        for size in (24, 24, 4, 4):
            n = struct.unpack_from("<Q", original, at)[0]
            at += 8 + n * size
        count = struct.unpack_from("<Q", original, at)[0]
        self.assertGreater(count, 1)
        struct.pack_into("<I", original, at + 8 + 52 + 40, 0)
        original[-64:] = hashlib.sha256(original[:-64]).hexdigest().encode()
        with self.assertRaises(ValueError):
            from_artifact(self.document, bytes(original), self.source)

    def test_truncated_original_rejected(self):
        with self.assertRaises(ValueError):
            from_artifact(self.document, self.original[:-8], self.source)


if __name__ == "__main__":
    unittest.main()
