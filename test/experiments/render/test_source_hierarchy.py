"""Same source candidates/fallbacks with immutable coarse metadata first."""

import unittest
from fractions import Fraction

import numpy as np
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from source_cluster_bounds import clip_boxes, frame, support_margin
from source_hierarchy import prepare, query
from source_raster_candidates import projection, select
from source_triangle_clusters import build
from test_source_raster_candidates import source, uploads


class StaticGroups(unittest.TestCase):
    def test_original_faces_parts_bounds_and_immutable_ownership(self):
        mesh = source(
            [
                [-0.7, -0.7, 0.5],
                [0.7, -0.7, 0.5],
                [0, 0.7, 0.5],
                [-0.94, -0.94, 0.5],
                [-0.92, -0.94, 0.5],
                [-0.94, -0.92, 0.5],
            ],
            [[0, 1, 2], [3, 4, 5]],
        )
        mesh["parts"] = [
            {"vertices": 3, "triangles": 1},
            {"vertices": 3, "triangles": 1},
        ]
        for order in ("source", "morton"):
            groups = build(mesh, leaf_triangles=128, order=order)
            np.testing.assert_array_equal(groups.faces, [0, 1])
            np.testing.assert_array_equal(groups.offsets, [0, 1, 2])
            for at in range(2):
                p = mesh["positions"][mesh["indices"][at]]
                np.testing.assert_array_equal(groups.bounds[at], [p.min(0), p.max(0)])
            with self.assertRaises(ValueError):
                groups.faces.flags.writeable = True
        prepared = prepare(mesh, leaf_triangles=1)
        original = prepared.positions.copy()
        mesh["positions"][:] = 0
        np.testing.assert_array_equal(prepared.positions, original)

    def test_invalid_part_ranges_do_not_reassign_original_faces(self):
        mesh = source([[0, 0, 0.5], [0.1, 0, 0.5], [0, 0.1, 0.5]], [[0, 1, 2]])
        for parts in (
            [{"vertices": -1, "triangles": 1}, {"vertices": 4, "triangles": 0}],
            [{"vertices": 2, "triangles": 1}, {"vertices": 1, "triangles": 0}],
            [],
        ):
            mesh["parts"] = parts
            with self.subTest(parts=parts), self.assertRaises(ValueError):
                build(mesh)


class Fallbacks(unittest.TestCase):
    def setUp(self):
        self.mesh = source(
            [
                [-0.7, -0.7, 0.5],
                [0.7, -0.7, 0.5],
                [0, 0.7, 0.5],
                [-0.94, -0.94, 0.5],
                [-0.92, -0.94, 0.5],
                [-0.94, -0.92, 0.5],
            ],
            [[0, 1, 2], [3, 4, 5]],
        )
        self.view, self.placement = uploads()

    def compare(self, mesh=None, **options):
        mesh = self.mesh if mesh is None else mesh
        hierarchy = prepare(mesh, leaf_triangles=1)
        expected, cost = select(
            hierarchy.source,
            self.view,
            self.placement,
            [8, 8],
            raster_guard_px=0.0,
            **options,
        )
        actual, work = query(
            hierarchy, self.view, self.placement, [8, 8], raster_guard_px=0.0, **options
        )
        np.testing.assert_array_equal(actual, expected)
        for key in ("complete_model", "reason", "original_group_retained"):
            self.assertEqual(work[key], cost[key])
        return actual, work

    def test_cooperative_budget_returns_whole_original_not_only_local_pages(self):
        ids, work = self.compare(pairs_most=0)
        np.testing.assert_array_equal(ids, [0, 1])
        self.assertEqual(work["reason"], "triangle_pixel_pair_budget")
        self.assertLess(
            work["hierarchy"]["fine_vertex_rows_touched"], len(self.mesh["positions"])
        )

    def test_unreferenced_outside_vertex_keeps_original_without_fine_access(self):
        self.mesh["positions"] = np.r_[
            self.mesh["positions"], np.array([[2, 0, 0.5]], "<f4")
        ]
        _, work = self.compare()
        self.assertEqual(work["hierarchy"]["bypass"], "actual_vertex_outside")
        self.assertEqual(work["hierarchy"]["fine_vertex_rows_touched"], 0)

    def test_root_overestimate_delegates_to_the_unmodified_flat_query(self):
        mesh = source(
            [[0.75, 0.75, 0.5], [-0.75, -0.75, 0.5], [0.25, 0.25, 0.6]], [[0, 1, 2]]
        )
        self.view.view("<f4")[4] = -1
        _, work = self.compare(mesh)
        self.assertEqual(work["hierarchy"]["bypass"], "root_projection")

    def test_subnormal_source_or_nonfinite_upload_preserves_fallback(self):
        self.mesh["positions"][0, 0] = np.nextafter(np.float32(0), np.float32(1))
        _, work = self.compare()
        self.assertEqual(work["hierarchy"]["bypass"], "unsupported_source")
        self.mesh["positions"][0, 0] = -0.7
        self.view.view("<f4")[0] = np.nan
        self.compare()

    def test_empty_demand_is_complete_without_reading_fine_rows(self):
        self.mesh = source(
            [[-0.94, -0.94, 0.5], [-0.92, -0.94, 0.5], [-0.94, -0.92, 0.5]], [[0, 1, 2]]
        )
        ids, work = self.compare()
        self.assertEqual(len(ids), 0)
        self.assertTrue(work["complete_model"])
        self.assertEqual(work["hierarchy"]["fine_vertex_rows_touched"], 0)

    def test_thin_overlapping_reversed_and_degenerate_faces_keep_original_ids(self):
        mesh = source(
            [
                [-0.625, -0.625, 0.5],
                [0.625, -0.625, 0.5],
                [-0.625, -0.625 + 2**-20, 0.5],
                [0.125, 0.125, 0.5],
            ],
            [[0, 1, 2], [2, 1, 0], [0, 1, 2], [3, 3, 3]],
        )
        self.compare(mesh)

    def test_query_limits_and_large_guard_follow_the_original_contract(self):
        h = prepare(self.mesh, leaf_triangles=1)
        for options in (
            {"pairs_most": -1},
            {"chunk_pairs": 0},
            {"vertex_batch_size": 0},
        ):
            with self.subTest(options=options), self.assertRaises(ValueError):
                query(
                    h, self.view, self.placement, [8, 8], raster_guard_px=0.0, **options
                )
        for guard in (8.0, float(np.finfo(float).max)):
            expected, base = select(
                h.source, self.view, self.placement, [8, 8], raster_guard_px=guard
            )
            actual, work = query(
                h, self.view, self.placement, [8, 8], raster_guard_px=guard
            )
            np.testing.assert_array_equal(actual, expected)
            self.assertEqual(work["reason"], base["reason"])


class ModelBounds(unittest.TestCase):
    def test_affine_box_intervals_contain_independent_exact_rational_points(self):
        rng = np.random.default_rng(73)
        for _ in range(12):
            bounds = np.sort(rng.uniform(-1000, 1000, (2, 3)).astype("<f4"), axis=0)
            placement = rng.uniform(-8, 8, (4, 4)).astype("<f4").astype(float)
            matrix = rng.uniform(-1, 1, (4, 4)).astype("<f4").astype(float)
            shift = rng.uniform(-10000, 10000, 4).astype("<f4").astype(float)
            low, high = clip_boxes(bounds[None], (placement, shift, matrix))
            points = rng.uniform(bounds[0], bounds[1], (6, 3)).astype("<f4")
            for point in points:
                value = [Fraction(float(v)) for v in (*point, 1)]
                for transform, add in ((placement, shift), (matrix, np.zeros(4))):
                    value = [
                        sum(
                            Fraction(float(a)) * b
                            for a, b in zip(row, value, strict=True)
                        )
                        + Fraction(float(t))
                        for row, t in zip(transform, add, strict=True)
                    ]
                for axis, q in enumerate(value):
                    self.assertLessEqual(Fraction(float(low[0, axis])), q)
                    self.assertGreaterEqual(Fraction(float(high[0, axis])), q)

    def test_small_random_surfaces_and_reflected_frames_match_flat_candidates(self):
        rng = np.random.default_rng(1866)
        for _ in range(6):
            centers = rng.uniform(-0.65, 0.65, (32, 1, 3))
            centers[:, :, 2] = 0.5
            positions = (
                (centers + rng.uniform(-0.008, 0.008, (32, 3, 3)))
                .reshape(-1, 3)
                .astype("<f4")
            )
            mesh = source(positions, np.arange(96).reshape(-1, 3))
            words, placement = uploads()
            words.view("<f4")[3] = 0.05
            for sign in (-1, 1):
                placement[0, :16].view("<f4")[0] = sign
                for order in ("source", "morton"):
                    h = prepare(mesh, leaf_triangles=1, order=order)
                    expected, base = select(
                        h.source, words, placement, [32, 32], raster_guard_px=0.0
                    )
                    ids, work = query(
                        h, words, placement, [32, 32], raster_guard_px=0.0
                    )
                    np.testing.assert_array_equal(ids, expected)
                    self.assertEqual(work["complete_model"], base["complete_model"])

    def test_uniform_coarse_margin_encloses_existing_fine_vertex_radii(self):
        rng = np.random.default_rng(1866)
        for _ in range(12):
            positions = rng.uniform(-0.2, 0.2, (200, 3)).astype("<f4")
            positions[:, 2] += 0.5
            words, placement = uploads()
            m = np.eye(4, dtype="<f4")
            m[:2, :2] += (rng.uniform(-0.15, 0.15, (2, 2))).astype("<f4")
            m[:3, 3] = [128, -256, 512]
            placement[0, :16] = m.ravel(order="F").view("<u4")
            words.view("<f4")[48:51] = -m[:3, 3]
            operands = frame(words, placement)
            bounds = np.array([positions.min(0), positions.max(0)])
            margin = support_margin(bounds, operands, np.array([32, 32]))
            _, radius = projection(positions, words, placement, np.array([32, 32]))
            self.assertIsNotNone(margin)
            self.assertTrue((radius <= margin).all())


class ActualSource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest, _, _, cls.mesh = load(DATA, attributes=True)
        bound_selection(DATA, cls.mesh)
        cls.hierarchy = prepare(cls.mesh)

    def test_native_poses_and_metadata_chunks_preserve_original_candidates(self):
        with np.load(
            DATA / "native-visible-triangles.npz", allow_pickle=False
        ) as archive:
            for pose in ("near-direction", "revealing-side"):
                for guard in (0.0, 1 / 256):
                    args = (
                        archive[pose + "/view_words"],
                        archive["placements/words"],
                        self.manifest["raster"],
                    )
                    expected, _ = select(
                        self.hierarchy.source, *args, raster_guard_px=guard
                    )
                    for batch in (64, 512, 4096):
                        actual, work = query(
                            self.hierarchy,
                            *args,
                            raster_guard_px=guard,
                            batch_boxes=batch,
                        )
                        np.testing.assert_array_equal(actual, expected)
                        self.assertLess(
                            work["hierarchy"]["fine_vertex_rows_touched"],
                            len(self.mesh["positions"]),
                        )

    def test_complete_turn_and_translation_preserve_candidates_and_whole_reason(self):
        with np.load(
            DATA / "native-visible-triangles.npz", allow_pickle=False
        ) as archive:
            base = archive["near-direction/view_words"]
            placement = archive["placements/words"]
            for angle in range(0, 360, 30):
                radians = np.deg2rad(angle)
                c, s = np.cos(radians), np.sin(radians)
                rotate = np.array(
                    [[c, 0, s, 0], [0, 1, 0, 0], [-s, 0, c, 0], [0, 0, 0, 1]]
                )
                for delta in (np.zeros(3), np.array([100, 25, -200])):
                    words = base.copy()
                    v = words.view("<f4")
                    matrix = (
                        base[:16].view("<f4").reshape(4, 4, order="F").astype(float)
                    )
                    v[:16] = (matrix @ rotate).astype("<f4").ravel(order="F")
                    v[48:51] = (base.view("<f4")[48:51].astype(float) - delta).astype(
                        "<f4"
                    )
                    args = (words, placement, self.manifest["raster"])
                    expected, reference = select(
                        self.hierarchy.source, *args, raster_guard_px=0.0
                    )
                    actual, work = query(self.hierarchy, *args, raster_guard_px=0.0)
                    np.testing.assert_array_equal(actual, expected)
                    for key in ("complete_model", "reason", "original_group_retained"):
                        self.assertEqual(work[key], reference[key])


if __name__ == "__main__":
    unittest.main()
