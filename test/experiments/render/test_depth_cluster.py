"""Planar reconstruction, hidden-surface counterexample and packed contracts."""

import unittest

import numpy as np
from audit_depth_visibility import classify
from depth_cluster import RECORD, TRACE, bake, fields, prepare, reference
from raster_depth_cluster import raster
from source_billboard import image_rays
from source_far_reprojection import prepare_source
from source_surface_fields import relight
from test_source_billboard import flat_source


def source_layers():
    source = flat_source()
    source["uv"] = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], np.float32)
    source["metres"] = source["uv"].copy()
    source["domain"] = np.zeros(2, np.uint8)
    return source


class DepthClusterTests(unittest.TestCase):
    def setUp(self):
        self.source = source_layers()
        self.view = (np.array([0.0, 0.0, 4.0]), np.diag([1.0, 1.0, -1.0]), 32.0, 64, 64)
        self.cache = bake(self.source, prepare_source(self.source), self.view)

    def test_packed_layout_sample_binding_and_capture_pose(self):
        self.assertEqual(RECORD.itemsize, 40)
        self.assertEqual(TRACE.itemsize, 16)
        self.assertTrue((self.cache["trace"]["weights"].sum(1) == 1 << 30).all())
        lo, width, height = np.array([20, 20]), 24, 24
        o, d = image_rays(
            self.view, np.array([lo, lo + [width, height]]), width, height
        )
        expected = reference(self.source, prepare_source(self.source), o, d)[0]
        t, slots, _ = raster(
            self.cache, prepare(self.cache), self.view, lo, width, height
        )
        np.testing.assert_array_equal(np.isfinite(t), np.isfinite(expected))
        np.testing.assert_allclose(t[np.isfinite(t)], 4, atol=1e-4)
        self.assertTrue((slots[np.isfinite(t)] >= 0).all())

    def test_query_needs_no_source_geometry_and_keeps_dynamic_light(self):
        self.source.clear()
        t, slots, _ = raster(
            self.cache, prepare(self.cache), self.view, np.array([24, 24]), 16, 16
        )
        f = fields(self.cache, slots)
        self.assertGreater(
            relight(f, [0, 0, 1], t).sum(), 4 * relight(f, [0, 0, -1], t).sum()
        )

    def test_adaptive_merge_keeps_one_original_face_and_capture_coverage(self):
        adaptive = bake(
            self.source, prepare_source(self.source), self.view, adaptive=True
        )
        self.assertGreater(adaptive["merged_full_pixels"], 0)
        self.assertLess(len(adaptive["data"]), len(self.cache["data"]))
        self.assertEqual(
            int((adaptive["data"]["span"].astype(int) ** 2).sum()),
            len(self.cache["data"]),
        )
        lookup = {
            int(p): int(f)
            for p, f in zip(
                self.cache["data"]["pixel"], self.cache["trace"]["face"], strict=True
            )
        }
        stride = self.cache["width"] * 2
        for row, trace in zip(adaptive["data"], adaptive["trace"], strict=True):
            if row["span"] == 2:
                ids = [
                    int(row["pixel"]) + offset for offset in (0, 1, stride, stride + 1)
                ]
                self.assertEqual({lookup[i] for i in ids}, {int(trace["face"])})
        args = (self.view, np.array([20, 20]), 24, 24)
        a = raster(self.cache, prepare(self.cache), *args)[0]
        b = raster(adaptive, prepare(adaptive), *args)[0]
        np.testing.assert_array_equal(np.isfinite(a), np.isfinite(b))

    def test_single_front_depth_cannot_reveal_hidden_background(self):
        # A nearer narrow quad hides the middle of a larger background quad.
        # Translation reveals background that was never in the original cache.
        source = source_layers()
        front = source["positions"].copy() * [0.05, 0.05, 1]
        front[:, 2] = 3.8
        source["positions"] = np.vstack((source["positions"], front)).astype(np.float32)
        source["indices"] = np.vstack((source["indices"], source["indices"] + 4))
        for key in ("normal", "code", "colour_index", "domain", "uv", "metres"):
            source[key] = np.concatenate((source[key], source[key]))
        cache = bake(source, prepare_source(source), self.view)
        moved = (self.view[0] + [1, 0, 0], *self.view[1:])
        lo, width, height = np.array([12, 12]), 40, 40
        o, d = image_rays(moved, np.array([lo, lo + [width, height]]), width, height)
        exact = reference(source, prepare_source(source), o, d)[0]
        depth, _, _ = raster(cache, prepare(cache), moved, lo, width, height)
        self.assertGreater(int((np.isfinite(exact) & ~np.isfinite(depth)).sum()), 0)
        audit = classify(
            source, prepare_source(source), cache, moved, lo, width, height
        )
        self.assertGreater(audit["missing_previously_occluded_or_backfacing"], 0)
        self.assertEqual(
            audit["missing_subpixels"],
            int((np.isfinite(exact) & ~np.isfinite(depth)).sum()),
        )


if __name__ == "__main__":
    unittest.main()
