"""Strict root exclusion, independent exact/F32 checks, unchanged unknown fallback."""

import itertools
import unittest
from fractions import Fraction
from unittest.mock import patch

import numpy as np
from compare_source_hierarchy import changed_frame, equivalent
from native_source_triangle_materials import DATA
from native_world_inputs import load
from source_clip_demand import outside_xy, query
from source_cluster_bounds import frame
from source_hierarchy import prepare
from source_hierarchy import query as old_query
from test_source_raster_candidates import source, uploads


def f32_transform(points, matrix, reverse=False):
    """Independent scalar-lane FP32 multiply/add, no host BLAS operation."""
    order = range(3, -1, -1) if reverse else range(4)
    out = np.zeros((len(points), 4), "<f4")
    for k in order:
        out = np.add(
            out,
            np.multiply(points[:, k, None], matrix[:, k], dtype=np.float32),
            dtype=np.float32,
        )
    return out


class RootClip(unittest.TestCase):
    def setUp(self):
        self.view, self.placement = uploads()

    def compare(self, mesh, **options):
        h = prepare(mesh)
        before = old_query(
            h, self.view, self.placement, [16, 16], raster_guard_px=0.0, **options
        )
        after = query(
            h, self.view, self.placement, [16, 16], raster_guard_px=0.0, **options
        )
        equivalent(after, before)
        return after

    def test_four_strict_xy_halfspaces_skip_old_query_and_all_fine_data(self):
        for axis, sign in itertools.product((0, 1), (-1, 1)):
            p = np.array([[0, 0, 0.5], [0.01, 0.01, 0.5], [0.02, 0, 0.5]], "<f4")
            p[:, axis] += sign * 2
            h = prepare(source(p, [[0, 1, 2]]))
            with patch(
                "source_clip_demand.original_query",
                side_effect=AssertionError("fine path entered"),
            ):
                ids, w = query(
                    h, self.view, self.placement, [16, 16], raster_guard_px=0.0
                )
            self.assertFalse(len(ids))
            self.assertTrue(w["complete_model"])
            self.assertEqual(w["reason"], "source_outside_xy_clip")
            self.assertEqual(w["clip"]["fine_position_rows_touched"], 0)
            self.assertEqual(w["clip"]["fine_index_rows_touched"], 0)
            self.assertEqual(w["clip"]["leaf_headers_tested"], 0)
            self.assertFalse(w["qualified_gpu_raster"])

    def test_equal_and_one_ulp_outside_bounds_do_not_justify_empty(self):
        for x in (1.0, float(np.nextafter(np.float32(1), np.float32(2)))):
            p = np.array([[x, -0.1, 0.5], [x, 0, 0.5], [x, 0.1, 0.5]], "<f4")
            _, w = self.compare(source(p, [[0, 1, 2]]))
            self.assertFalse(w["clip"]["outside_planes"])
            self.assertFalse(w["complete_model"])

    def test_partial_clip_retains_whole_including_unread_indices(self):
        p = [[0.5, 0, 0.5], [2.0, -0.2, 0.5], [2.0, 0.2, 0.5]]
        ids, w = self.compare(source(p, [[0, 1, 2]]))
        np.testing.assert_array_equal(ids, [0])
        self.assertFalse(w["complete_model"])

    def test_near_far_only_outside_remains_unknown_without_pipeline_depth_contract(
        self,
    ):
        for z in (-1.0, 2.0):
            _, w = self.compare(
                source([[-0.1, -0.1, z], [0.1, -0.1, z], [0, 0.1, z]], [[0, 1, 2]])
            )
            self.assertFalse(w["clip"]["outside_planes"])
            self.assertFalse(w["complete_model"])

    def test_subnormal_source_and_nonfinite_upload_never_become_empty(self):
        p = np.array([[2, 0, 0.5], [2, 0.1, 0.5], [2, 0.2, 0.5]], "<f4")
        p[0, 1] = np.nextafter(np.float32(0), np.float32(1))
        _, w = self.compare(source(p, [[0, 1, 2]]))
        self.assertFalse(w["clip"]["outside_planes"])
        self.view.view("<f4")[0] = np.nan
        _, w = self.compare(
            source([[2, 0, 0.5], [2, 0.1, 0.5], [2, 0.2, 0.5]], [[0, 1, 2]])
        )
        self.assertFalse(w["clip"]["outside_planes"])

    def test_overflow_risk_and_invalid_queries_cannot_claim_empty(self):
        p = [[2, 0, 0.5], [2, 0.1, 0.5], [2, 0.2, 0.5]]
        self.view.view("<f4")[0] = np.finfo(np.float32).max
        with np.errstate(over="ignore", invalid="ignore"):
            _, w = self.compare(source(p, [[0, 1, 2]]))
        self.assertFalse(w["clip"]["outside_planes"])
        h = prepare(source(p, [[0, 1, 2]]))
        for opts in ({"pairs_most": -1}, {"batch_boxes": 0}, {"vertex_batch_size": 0}):
            with self.assertRaises(ValueError):
                query(
                    h, self.view, self.placement, [16, 16], raster_guard_px=0.0, **opts
                )

    def test_guard_whole_and_unreferenced_boundary_point_preserve_original_contract(
        self,
    ):
        p = [[2, 0, 0.5], [2, 0.1, 0.5], [2, 0.2, 0.5], [0, 0, 0.5]]
        _, w = self.compare(source(p, [[0, 1, 2]]))
        self.assertFalse(w["clip"]["outside_planes"])
        h = prepare(source(p[:3], [[0, 1, 2]]))
        ids, w = query(h, self.view, self.placement, [16, 16], raster_guard_px=16.0)
        np.testing.assert_array_equal(ids, [0])
        self.assertEqual(w["reason"], "raster_guard_covers_viewport")

    def test_root_upper_encloses_independent_exact_and_ordered_float32_planes(self):
        rng = np.random.default_rng(1873)
        for _ in range(30):
            a = rng.integers(-16, 16, (2, 3)).astype("<f4") / 4
            bounds = np.stack([a.min(0), a.max(0)]).astype("<f4")
            placement = np.eye(4, dtype="<f4")
            placement[:3] = rng.integers(-8, 9, (3, 4)) / 4
            matrix = rng.integers(-8, 9, (4, 4)).astype("<f4") / 8
            shift = rng.integers(-4, 5, 3).astype("<f4") / 4
            self.placement[0, :16] = placement.ravel(order="F").view("<u4")
            self.view.view("<f4")[:16] = matrix.ravel(order="F")
            self.view.view("<f4")[48:51] = shift
            upper = outside_xy(bounds, frame(self.view, self.placement))
            self.assertIsNotNone(upper)
            points = np.array(
                [
                    list(row) + [1.0]
                    for row in itertools.product(
                        *zip(bounds[0], bounds[1], strict=True)
                    )
                ],
                "<f4",
            )
            q = lambda value: Fraction(float(value))
            for point in points:
                stable = [
                    sum(q(placement[i, j]) * q(point[j]) for j in range(4))
                    for i in range(4)
                ]
                world = [stable[i] + (q(shift[i]) if i < 3 else 0) for i in range(4)]
                clip = [
                    sum(q(matrix[i, j]) * world[j] for j in range(4)) for i in range(4)
                ]
                planes = [
                    clip[0] + clip[3],
                    clip[3] - clip[0],
                    clip[1] + clip[3],
                    clip[3] - clip[1],
                ]
                self.assertTrue(
                    all(
                        value <= q(limit)
                        for value, limit in zip(planes, upper, strict=True)
                    )
                )
            for reverse in (False, True):
                stable = f32_transform(points, placement, reverse)
                world = np.add(stable, np.r_[shift, np.float32(0)], dtype=np.float32)
                c = f32_transform(world, matrix, reverse).astype(float)
                planes = np.c_[
                    c[:, 0] + c[:, 3],
                    c[:, 3] - c[:, 0],
                    c[:, 1] + c[:, 3],
                    c[:, 3] - c[:, 1],
                ]
                self.assertTrue((planes <= upper).all())


class WholeSource(unittest.TestCase):
    def test_actual_bound_cases_unchanged_and_seven_turn_misses_are_strictly_empty(
        self,
    ):
        m, _, _, s = load(DATA, attributes=True)
        h = prepare(s)
        with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as a:
            p = a["placements/words"]
            base = a["near-direction/view_words"]
            for pose in ("near-direction", "revealing-side"):
                for guard in (0.0, 1 / 256):
                    equivalent(
                        query(
                            h,
                            a[pose + "/view_words"],
                            p,
                            m["raster"],
                            raster_guard_px=guard,
                        ),
                        old_query(
                            h,
                            a[pose + "/view_words"],
                            p,
                            m["raster"],
                            raster_guard_px=guard,
                        ),
                    )
            for angle in range(0, 360, 30):
                v = changed_frame(base, angle, np.zeros(3))
                old = old_query(h, v, p, m["raster"], raster_guard_px=0.0)
                ids, w = query(h, v, p, m["raster"], raster_guard_px=0.0)
                if angle in (90, 120, 150, 180, 210, 240, 270):
                    self.assertFalse(old[1]["complete_model"])
                    self.assertFalse(len(ids))
                    self.assertIn("bottom", w["clip"]["outside_planes"])
                    points = np.c_[s["positions"], np.ones(len(s["positions"]), "<f4")]
                    M = p[0, :16].view("<f4").reshape(4, 4, order="F")
                    V = v[:16].view("<f4").reshape(4, 4, order="F")
                    shift = np.r_[v.view("<f4")[48:51], np.float32(0)]
                    for reverse in (False, True):
                        c = f32_transform(
                            np.add(
                                f32_transform(points, M, reverse),
                                shift,
                                dtype=np.float32,
                            ),
                            V,
                            reverse,
                        )
                        self.assertTrue((c[:, 1].astype(float) + c[:, 3] < 0).all())
                else:
                    equivalent((ids, w), old)


if __name__ == "__main__":
    unittest.main()
