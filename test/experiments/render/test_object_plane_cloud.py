"""Analytic whole-face membership and bounded omission-free failure controls."""

import unittest

import numpy as np
from object_plane_cloud import mip_texels, partition


def squares(z, reverse=False):
    xyz = np.array([[0, 0, z], [1, 0, z], [1, 1, z], [0, 1, z]], float)
    ids = np.array([[0, 1, 2], [0, 2, 3]], np.uint32)
    return xyz, ids[:, ::-1] if reverse else ids


def combine(parts):
    xyz, ids, offset = [], [], 0
    for p, t in parts:
        xyz.append(p)
        ids.append(t + offset)
        offset += len(p)
    return np.concatenate(xyz), np.concatenate(ids)


class PlaneCloud(unittest.TestCase):
    def test_opposite_faces_and_hidden_parallel_sheets_keep_all_originals(self):
        p, t = combine(
            [squares(0), squares(0.05), squares(0.02, True), squares(0.07, True)]
        )
        charts, fallback, reasons, work = partition(p, t, epsilon=0.1, pitch=0.1)
        self.assertEqual(len(charts), 2)
        self.assertEqual(len(fallback), 0)
        self.assertEqual(
            [c.faces.tolist() for c in charts], [[0, 1, 2, 3], [4, 5, 6, 7]]
        )
        self.assertGreater(charts[0].basis[2, 2], 0)
        self.assertLess(charts[1].basis[2, 2], 0)
        self.assertEqual(work["input_triangles"], 8)
        self.assertTrue((reasons == 0).all())

    def test_whole_triangle_not_centroid_decides_slab_membership(self):
        p, t = combine([squares(0), squares(0.05)])
        p = np.vstack((p, [[0, 0, -1], [0.1, 0, 1], [0, 0.1, 0]]))
        t = np.vstack((t, [[8, 9, 10]])).astype(np.uint32)
        charts, fallback, _, _ = partition(p, t, epsilon=0.1, pitch=0.1)
        self.assertEqual(charts[0].faces.tolist(), [0, 1, 2, 3])
        self.assertEqual(fallback.tolist(), [4])

    def test_all_budget_failures_preserve_original_triangle_indices(self):
        p, t = combine([squares(0), squares(0.05)])
        for kwargs, reason in (
            ({"plane_limit": 0}, 2),
            ({"relation_limit": 3}, 3),
            ({"side_limit": 1}, 5),
            ({"texel_limit": 1}, 6),
        ):
            charts, fallback, reasons, _ = partition(
                p, t, epsilon=0.1, pitch=0.1, **kwargs
            )
            self.assertEqual(charts, ())
            self.assertEqual(fallback.tolist(), list(range(4)))
            self.assertTrue((reasons == reason).all())

    def test_every_accepted_vertex_and_affine_interior_stays_in_slab(self):
        rng = np.random.default_rng(14)
        p, t = combine([squares(float(z)) for z in rng.uniform(-0.04, 0.04, 20)])
        charts, _, _, _ = partition(p, t, epsilon=0.1, pitch=0.1)
        for chart in charts:
            triangles = p[t[chart.faces]]
            weights = rng.dirichlet(np.ones(3), 23)
            points = np.einsum("aj,tjk->tak", weights, triangles)
            self.assertLessEqual(abs(points @ chart.basis[2] - chart.offset).max(), 0.1)
            np.testing.assert_allclose(
                chart.basis @ chart.basis.T, np.eye(3), atol=1e-14
            )

    def test_empty_degenerate_and_invalid_inputs(self):
        charts, fallback, _, work = partition(
            np.empty((0, 3)), np.empty((0, 3), np.uint32), epsilon=1, pitch=1
        )
        self.assertEqual(
            (len(charts), len(fallback), work["face_plane_relations"]), (0, 0, 0)
        )
        p, t = squares(0)
        p[2] = p[1]
        _, fallback, reasons, _ = partition(p, t, epsilon=1, pitch=1)
        self.assertEqual(fallback.tolist(), [0, 1])
        self.assertEqual(reasons[0], 1)
        for kwargs in (
            {"epsilon": 0},
            {"pitch": 1e-300},
            {"pitch": float("nan")},
            {"plane_limit": 257},
            {"relation_limit": -1},
        ):
            with self.assertRaises(ValueError):
                partition(p, t, **({"epsilon": 1, "pitch": 1} | kwargs))
        with self.assertRaises(ValueError):
            partition(p * 1e20, t, epsilon=1, pitch=1)

    def test_non_square_mip_layout(self):
        self.assertEqual(mip_texels((3, 7)), 21 + 3 + 1)

    def test_spatial_splits_reduce_empty_rectangles_without_losing_faces(self):
        parts = []
        for i in range(6):
            p, t = squares(0)
            p[:, 0] += 10 * i
            parts.append((p, t))
        p, t = combine(parts)
        a = partition(p, t, epsilon=0.1, pitch=0.1, compact=False)
        b = partition(p, t, epsilon=0.1, pitch=0.1, compact=True)
        self.assertLess(b[3]["chart_texels"], a[3]["chart_texels"])
        self.assertEqual(b[1].size, 0)
        self.assertEqual(
            sorted(np.concatenate([c.faces for c in b[0]]).tolist()),
            list(range(len(t))),
        )


if __name__ == "__main__":
    unittest.main()
