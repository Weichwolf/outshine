"""Outer support properties, independent exact coverage and finite work limits."""

import itertools
import unittest
from fractions import Fraction
from unittest.mock import patch

import numpy as np
import source_raster_candidates as candidates
from native_source_triangle_materials import DATA, bound_selection, raster_selection
from native_world_inputs import load


def uploads():
    view = np.zeros(56, "<f4")
    view[:16] = np.eye(4, dtype="<f4").ravel(order="F")
    placement = np.zeros((1, 36), "<u4")
    placement[0, :16] = view[:16].view("<u4")
    return view.view("<u4"), placement


def source(vertices, faces):
    return {"positions": np.asarray(vertices, "<f4"), "indices": np.asarray(faces)}


def exact_covered(vertices, point):
    """Closed triangle test with rational arithmetic, independent of dilation."""
    v = [[Fraction(float(x)) for x in row] for row in vertices]
    p = [Fraction(float(x)) for x in point]
    signs = []
    for a, b in zip(v, v[1:] + v[:1], strict=True):
        signs.append((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]))
    return all(s >= 0 for s in signs) or all(s <= 0 for s in signs)


class OuterSupport(unittest.TestCase):
    def run_projected(self, vertices, error, **kwargs):
        mesh = source(
            np.c_[vertices, np.full(len(vertices), 0.5)],
            np.arange(len(vertices)).reshape(-1, 3),
        )
        with patch.object(candidates, "projection", return_value=(vertices, error)):
            return candidates.select(
                mesh, *uploads(), [4, 4], raster_guard_px=0.0, **kwargs
            )

    def test_perturbed_triangles_cannot_lose_exact_rational_hits(self):
        rng = np.random.default_rng(1861)
        # Independently perturb every vertex to every corner of its uncertainty box.
        vertices = rng.integers(1, 31, (36, 2)).astype(float) / 8
        error = np.full(vertices.shape, 0.125)
        selected, _ = self.run_projected(vertices, error)
        required = set()
        offsets = list(itertools.product((-0.125, 0.125), repeat=2))
        centers = list(itertools.product((0.5, 1.5, 2.5, 3.5), repeat=2))
        for face, nominal in enumerate(vertices.reshape(-1, 3, 2)):
            for perturbation in itertools.product(offsets, repeat=3):
                if any(exact_covered(nominal + perturbation, p) for p in centers):
                    required.add(face)
                    break
        self.assertTrue(required)
        self.assertTrue(required.issubset(set(selected)))

    def test_thin_disconnected_overlap_winding_and_degenerate(self):
        vertices = np.array(
            [
                [0.5, 0.5],
                [3.5, 0.5],
                [0.5, 0.5 + 2**-30],
                [0.5, 0.5],
                [0.5, 0.5 + 2**-30],
                [3.5, 0.5],
                [1.5, 1.5],
                [1.5, 1.5],
                [1.5, 1.5],
                [2.0, 2.0],
                [2.1, 2.0],
                [2.0, 2.1],
            ]
        )
        selected, work = self.run_projected(vertices, np.zeros_like(vertices))
        np.testing.assert_array_equal(selected, [0, 1, 2])
        self.assertEqual(work["tested_pairs"], 9)

    def test_bounded_chunks_do_not_change_selection(self):
        vertices = np.tile([[0.1, 0.1], [3.9, 0.1], [0.1, 3.9]], (20, 1))
        expected, _ = self.run_projected(vertices, np.zeros_like(vertices))
        for chunk in (1, 7, candidates.MAX_CHUNK):
            actual, work = self.run_projected(
                vertices, np.zeros_like(vertices), chunk_pairs=chunk
            )
            np.testing.assert_array_equal(actual, expected)
            self.assertLessEqual(work["max_chunk_pairs"], chunk)
            self.assertEqual(work["tested_pairs"], 320)

    def test_budget_falls_back_before_pair_expansion(self):
        vertices = np.array([[0.1, 0.1], [3.9, 0.1], [0.1, 3.9]])
        selected, work = self.run_projected(
            vertices, np.zeros_like(vertices), pairs_most=15
        )
        np.testing.assert_array_equal(selected, [0])
        self.assertEqual(work["reason"], "triangle_pixel_pair_budget")
        self.assertEqual(work["tested_pairs"], 0)


class UploadProjection(unittest.TestCase):
    def setUp(self):
        self.mesh = source(
            [[-0.4, -0.4, 0.5], [0.4, -0.4, 0.5], [0, 0.4, 0.5]], [[0, 1, 2]]
        )
        self.view, self.placement = uploads()

    def test_upload_order_bound_contains_sequential_float32_models(self):
        rng = np.random.default_rng(42)
        p = rng.uniform(-0.2, 0.2, (64, 3)).astype("<f4")
        p[:, 2] += 0.5
        matrix = np.eye(4, dtype="<f4")
        matrix[:3, 3] = [10000, -20000, 30000]
        self.placement[0, :16] = matrix.ravel(order="F").view("<u4")
        self.view.view("<f4")[48:51] = -matrix[:3, 3]
        center, error = candidates.projection(
            p, self.view, self.placement, np.array([32, 32])
        )
        for order in (range(4), range(3, -1, -1)):
            stable = np.zeros((len(p), 4), "<f4")
            homogeneous = np.c_[p, np.ones(len(p), "<f4")]
            for column in order:
                stable += homogeneous[:, column, None] * matrix[:, column]
            stable += np.r_[self.view.view("<f4")[48:51], np.float32(0)]
            screen = (stable[:, :2] / stable[:, 3, None] + np.float32(1)) * np.float32(
                16
            )
            self.assertTrue((abs(screen.astype(float) - center) <= error).all())

    def test_clip_subnormal_nonfinite_and_overflow_keep_original(self):
        for value in (0.0, 1.0, -0.1, float("inf"), float("nan")):
            view = self.view.copy()
            view.view("<f4")[10] = value
            if value == 1.0:
                view.view("<f4")[14] = 0.5
            with self.subTest(value=value):
                ids, work = candidates.select(
                    self.mesh, view, self.placement, [4, 4], raster_guard_px=0.0
                )
                np.testing.assert_array_equal(ids, [0])
                self.assertFalse(work["complete_model"])
        for value in (
            np.nextafter(np.float32(0), np.float32(1)),
            np.finfo(np.float32).max,
        ):
            view = self.view.copy()
            view.view("<f4")[0] = value
            _, work = candidates.select(
                self.mesh, view, self.placement, [4, 4], raster_guard_px=0.0
            )
            self.assertTrue(work["original_group_retained"])

    def test_invalid_budgets_and_layout_are_rejected(self):
        for kwargs in (
            {"pairs_most": -1},
            {"chunk_pairs": 0},
            {"raster_guard_px": -1.0},
            {"raster_guard_px": float("nan")},
        ):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                candidates.select(
                    self.mesh,
                    self.view,
                    self.placement,
                    [4, 4],
                    **({"raster_guard_px": 0.0} | kwargs),
                )
        with self.assertRaises(ValueError):
            candidates.select(
                self.mesh, self.view[:55], self.placement, [4, 4], raster_guard_px=0.0
            )

    def test_extreme_finite_guard_retains_original_before_overflow(self):
        ids, work = candidates.select(
            self.mesh,
            self.view,
            self.placement,
            [4, 4],
            raster_guard_px=float(np.finfo(float).max),
        )
        np.testing.assert_array_equal(ids, [0])
        self.assertEqual(work["reason"], "raster_guard_covers_viewport")
        self.assertTrue(work["original_group_retained"])

    def test_unknown_later_vertex_block_retains_the_whole_original(self):
        mesh = source(np.tile(self.mesh["positions"], (2, 1)), [[0, 1, 2], [3, 4, 5]])
        mesh["positions"][5, 0] = np.nextafter(np.float32(0), np.float32(1))
        ids, work = candidates.select(
            mesh,
            self.view,
            self.placement,
            [4, 4],
            raster_guard_px=0.0,
            vertex_batch_size=3,
        )
        np.testing.assert_array_equal(ids, [0, 1])
        self.assertEqual(work["reason"], "clip_or_arithmetic_unknown")
        self.assertEqual(work["tested_pairs"], 0)
        with self.assertRaises(ValueError):
            candidates.select(
                mesh,
                self.view,
                self.placement,
                [4, 4],
                raster_guard_px=0.0,
                vertex_batch_size=0,
            )

    def test_source_words_and_winding_remain_untouched(self):
        self.mesh["positions"][2, 0] = -0.0
        before = {k: a.tobytes() for k, a in self.mesh.items()}
        _, work = candidates.select(
            self.mesh, self.view, self.placement, [4, 4], raster_guard_px=0.0
        )
        self.assertTrue(work["complete_model"])
        self.assertFalse(work["qualified_gpu_raster"])
        self.assertEqual(before, {k: a.tobytes() for k, a in self.mesh.items()})


class NativeCounterexample(unittest.TestCase):
    def test_vertex_batches_preserve_projection_and_selected_original_ids(self):
        manifest, _, _, mesh = load(DATA, attributes=True)
        bound_selection(DATA, mesh)
        with np.load(
            DATA / "native-visible-triangles.npz", allow_pickle=False
        ) as archive:
            for pose in ("near-direction", "revealing-side"):
                view, placement = (
                    archive[pose + "/view_words"],
                    archive["placements/words"],
                )
                whole = candidates.projection(
                    mesh["positions"],
                    view,
                    placement,
                    np.asarray(manifest["raster"]),
                    vertex_batch_size=candidates.MAX_VERTICES,
                )
                for size in (512, 8192, 32768):
                    batched = candidates.projection(
                        mesh["positions"],
                        view,
                        placement,
                        np.asarray(manifest["raster"]),
                        vertex_batch_size=size,
                    )
                    for actual, expected in zip(batched, whole, strict=True):
                        np.testing.assert_array_equal(
                            actual.view(np.uint64), expected.view(np.uint64)
                        )
                for guard in (0.0, 1 / 16, 0.5):
                    expected, _ = candidates.select(
                        mesh,
                        view,
                        placement,
                        manifest["raster"],
                        raster_guard_px=guard,
                        vertex_batch_size=candidates.MAX_VERTICES,
                    )
                    for size in (512, 8192, 32768):
                        actual, _ = candidates.select(
                            mesh,
                            view,
                            placement,
                            manifest["raster"],
                            raster_guard_px=guard,
                            vertex_batch_size=size,
                        )
                        np.testing.assert_array_equal(actual, expected)

    def test_each_pose_independently_contains_native_reference_owners(self):
        manifest, _, _, mesh = load(DATA, attributes=True)
        bound_selection(DATA, mesh)
        with np.load(
            DATA / "native-visible-triangles.npz", allow_pickle=False
        ) as archive:
            for pose in ("near-direction", "revealing-side"):
                ids, work = candidates.select(
                    mesh,
                    archive[pose + "/view_words"],
                    archive["placements/words"],
                    manifest["raster"],
                    raster_guard_px=0.0,
                )
                self.assertEqual(
                    len(np.setdiff1d(archive[pose + "/triangle_ordinal"], ids)), 0
                )
                self.assertLess(len(ids), len(mesh["indices"]) // 10)
                self.assertTrue(work["complete_model"])
                self.assertFalse(work["qualified_gpu_raster"])
        ids, binding = raster_selection(mesh, manifest, DATA, 0.0)
        self.assertEqual(len(binding["poses"]), 2)
        self.assertFalse(binding["qualified_gpu_raster"])
        self.assertLess(len(ids), len(mesh["indices"]) // 10)


if __name__ == "__main__":
    unittest.main()
