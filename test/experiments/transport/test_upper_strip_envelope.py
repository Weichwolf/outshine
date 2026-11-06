import unittest
from collections import Counter

import numpy as np
from compare_upper_strip_envelope import run_savings
from upper_strip_envelope import lower_quad, stitch_quad, upper_quad


def distances(points, triangles):
    """Independent point/triangle plane and edge opponent; no ribbon or roof model."""
    best = np.full(len(points), np.inf)
    for a, b, c in triangles:
        ab, ac = b - a, c - a
        normal = np.cross(ab, ac)
        normal2 = normal @ normal
        if normal2 > 0:
            signed = (points - a) @ normal
            projected = points - signed[:, None] * normal / normal2
            gram = np.array([[ab @ ab, ab @ ac], [ab @ ac, ac @ ac]])
            if np.linalg.det(gram) > 0:
                uv = np.linalg.solve(
                    gram, np.array([(projected - a) @ ab, (projected - a) @ ac])
                ).T
                inside = (uv >= 0).all(axis=1) & (uv.sum(axis=1) <= 1)
                best[inside] = np.minimum(best[inside], signed[inside] ** 2 / normal2)
        for start, end in [(a, b), (b, c), (c, a)]:
            edge = end - start
            length2 = edge @ edge
            fraction = (
                np.clip((points - start) @ edge / length2, 0, 1)
                if length2
                else np.zeros(len(points))
            )
            delta = points - start - fraction[:, None] * edge
            best = np.minimum(best, np.sum(delta * delta, axis=1))
    return np.sqrt(best)


def samples(triangles):
    bary = np.array(
        [(a / 16, b / 16, 1 - (a + b) / 16) for a in range(17) for b in range(17 - a)]
    )
    return np.einsum("ni,tij->tnj", bary, triangles).reshape(-1, 3)


class UpperStripEnvelope(unittest.TestCase):
    def check_meshes(self, rings):
        candidate = upper_quad(*rings)
        self.assertIsNotNone(candidate)
        source = np.array(rings).reshape(-1, 3)
        indices = np.array(
            [
                [a, a + 4, a + 1] if side == 0 else [a + 1, a + 4, a + 5]
                for a in range(3)
                for side in range(2)
            ]
        )
        original = source[indices]
        coarse = candidate["positions"][candidate["triangles"]]
        slack = 128 * np.finfo(float).eps * max(1, np.abs(source).max())
        self.assertLessEqual(
            distances(samples(original), coarse).max(),
            candidate["position_bound_m"] + slack,
        )
        self.assertLessEqual(
            distances(samples(coarse), original).max(),
            candidate["position_bound_m"] + slack,
        )
        np.testing.assert_array_equal(candidate["positions"], source[[0, 3, 7, 4]])
        self.assertTrue(
            np.all(
                np.cross(coarse[:, 1] - coarse[:, 0], coarse[:, 2] - coarse[:, 0])[:, 1]
                > 0
            )
        )
        return candidate

    def test_planar(self):
        rings = np.array(
            [[[x, 0, z] for x in [-2, -1, 1, 2]] for z in [0, 4]], dtype=float
        )
        self.assertEqual(self.check_meshes(rings)["position_bound_m"], 0)

    def test_saddle_and_both_diagonals(self):
        for sign in [-1, 1]:
            rings = np.array(
                [
                    [[x, sign * x * bank, z] for x in [-2, -1, 1, 2]]
                    for z, bank in [(0, 0.5), (4, -0.5)]
                ]
            )
            candidate = self.check_meshes(rings)
            self.assertEqual(candidate["roof_gap_m"], 2)
            for triangle in candidate["positions"][candidate["triangles"]]:
                a, b, c = triangle
                uv = np.linalg.solve(
                    np.array([b[[0, 2]] - a[[0, 2]], c[[0, 2]] - a[[0, 2]]]).T,
                    [0 - a[0], 2 - a[2]],
                )
                self.assertAlmostEqual(
                    a[1] + uv[0] * (b[1] - a[1]) + uv[1] * (c[1] - a[1]), 1
                )

    def test_stored_rotated_and_translated_source(self):
        for angle in [0, 0.2, 1.3, 2.7]:
            rings = np.array(
                [
                    [
                        [x, 0.02 * x + z * 0.03 + x * z * 0.004, z]
                        for x in [-3, -1, 1, 3]
                    ]
                    for z in [0, 10]
                ],
                dtype=float,
            )
            rotation = np.array(
                [
                    [np.cos(angle), 0, np.sin(angle)],
                    [0, 1, 0],
                    [-np.sin(angle), 0, np.cos(angle)],
                ]
            )
            stored = ((rings @ rotation.T) + [12345.625, 120, -5312.25]).astype(
                np.float32
            )
            self.check_meshes(stored)

    def test_invalid_and_folded_domains(self):
        rings = np.array(
            [[[x, 0, z] for x in [-2, -1, 1, 2]] for z in [0, 4]], dtype=float
        )
        self.assertIsNone(upper_quad(rings[0], rings[0]))
        self.assertIsNone(upper_quad(rings[0], rings[1, ::-1]))
        self.assertIsNone(upper_quad(rings[0, [0, 2, 1, 3]], rings[1]))
        rings[0, 0, 0] = np.nan
        with self.assertRaises(ValueError):
            upper_quad(*rings)

    def test_lower_roof_is_the_opposite_envelope(self):
        rings = np.array(
            [
                [[x, x * bank, z] for x in [-2, -1, 1, 2]]
                for z, bank in [(0, 0.5), (4, -0.5)]
            ]
        )
        lower = lower_quad(*rings)
        self.assertIsNotNone(lower)
        for triangle in lower["positions"][lower["triangles"]]:
            a, b, c = triangle
            uv = np.linalg.solve(
                np.array([b[[0, 2]] - a[[0, 2]], c[[0, 2]] - a[[0, 2]]]).T,
                [0 - a[0], 2 - a[2]],
            )
            self.assertAlmostEqual(
                a[1] + uv[0] * (b[1] - a[1]) + uv[1] * (c[1] - a[1]), -1
            )
        triangles = lower["positions"][lower["triangles"]]
        self.assertTrue(
            np.all(
                np.cross(
                    triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
                )[:, 1]
                < 0
            )
        )

    def test_run_contacts_keep_every_original_boundary_edge(self):
        rings = np.array(
            [
                [[x, 0.02 * x + z * 0.03 + x * z * 0.004, z] for x in [-3, -1, 1, 3]]
                for z in [0, 10]
            ],
            dtype=np.float32,
        )
        candidate = stitch_quad(upper_quad(*rings), *rings, start=True, end=True)
        edges = Counter(
            tuple(sorted((int(t[i]), int(t[(i + 1) % 3]))))
            for t in candidate["triangles"]
            for i in range(3)
        )
        actual = {
            tuple(sorted(tuple(p) for p in candidate["positions"][[a, b]]))
            for (a, b), count in edges.items()
            if count == 1
        }
        original = rings.reshape(-1, 3)
        expected = {
            tuple(sorted((tuple(original[a]), tuple(original[b]))))
            for a, b in [(0, 1), (1, 2), (2, 3), (4, 5), (5, 6), (6, 7), (0, 4), (3, 7)]
        }
        self.assertEqual(actual, expected)
        self.assertEqual(len(candidate["triangles"]), 6)
        source_triangles = original[
            np.array(
                [
                    [a, a + 4, a + 1] if side == 0 else [a + 1, a + 4, a + 5]
                    for a in range(3)
                    for side in range(2)
                ]
            )
        ]
        actual_triangles = candidate["positions"][candidate["triangles"]]
        slack = 128 * np.finfo(float).eps * np.abs(original).max()
        self.assertLessEqual(
            distances(samples(source_triangles), actual_triangles).max(),
            candidate["position_bound_m"] + slack,
        )
        self.assertLessEqual(
            distances(samples(actual_triangles), source_triangles).max(),
            candidate["position_bound_m"] + slack,
        )

    def test_isolated_spans_cannot_claim_contact_savings(self):
        self.assertEqual(run_savings([0.001, 0.1, 0.001], 4, 0.005), (0, 0))
        self.assertEqual(
            run_savings([0.001, 0.001, 0.1, 0.001, 0.001, 0.001], 4, 0.005), (12, 5)
        )
        self.assertEqual(run_savings([0.001, 0.001, np.inf, 0.001], 8, np.inf), (8, 2))


if __name__ == "__main__":
    unittest.main()
