"""Independent small matrices, energy, sides and analytic geometric transfer."""

import unittest

import numpy as np
from exchange_operator import build_links, gather_apply, mix_sides, prepare_csr
from scatter_reference import rectangle_factor
from scatter_scene import ScatterScene


class ExchangeChecks(unittest.TestCase):
    def test_sparse_matches_explicit_matrix_and_bounded_gather(self):
        links = np.array(
            ((1, 1, -1, 3), (0, 2, 3, -1), (0, 0, 0, 0), (-1, -1, -1, -1)), np.int32
        )
        expected = np.array(
            ((0, 0.5, 0, 0.25), (0.25, 0, 0.25, 0.25), (1, 0, 0, 0), (0, 0, 0, 0))
        )
        values = np.arange(12, dtype=float).reshape(4, 3)
        sparse = prepare_csr(links)
        np.testing.assert_array_equal(sparse.toarray(), expected)
        np.testing.assert_array_equal(sparse @ values, expected @ values)
        for rows in (1, 2, 4):
            np.testing.assert_array_equal(
                gather_apply(links, values, rows), expected @ values
            )
        self.assertTrue((sparse.sum(1) <= 1).all())

    def test_material_changes_preserve_geometry_and_bound_white_furnace(self):
        incoming = np.ones((4, 3))
        optical = np.array(((0.2, 0.3, 0.1, 0.1, 0.2, 0.3), (0.3, 0.2, 0.1, 0, 0, 0)))
        expected = np.repeat(optical[:, :3] + optical[:, 3:], 2, axis=0)
        np.testing.assert_allclose(mix_sides(incoming, optical), expected)
        np.testing.assert_array_equal(mix_sides(np.zeros_like(incoming), optical), 0)

    def test_sheet_side_exchange_is_explicit(self):
        incident = np.array(((1, 2, 3), (4, 5, 6)), float)
        optics = np.array(((0.2, 0.2, 0.2, 0.1, 0.1, 0.1),))
        result = mix_sides(incident, optics)
        np.testing.assert_allclose(result[0], 0.2 * incident[0] + 0.1 * incident[1])
        np.testing.assert_allclose(result[1], 0.1 * incident[0] + 0.2 * incident[1])
        np.testing.assert_array_equal(mix_sides(2 * incident, optics), 2 * result)

    def test_virtual_differential_receiver_obeys_rectangle_factor(self):
        v = np.array(((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), float)
        f = np.array(((0, 1, 2), (0, 2, 3)))
        scene = ScatterScene(v, f)
        points = np.array(((0, 0, 0), (0, 0, 1)), float)
        normals = np.tile((0, 0, 1.0), (2, 1))
        links = build_links(
            scene,
            points,
            normals,
            65536,
            lambda ids, p: np.ones(len(ids), int),
            packet=8192,
        )
        factor = float(np.mean(links[0] >= 0))
        self.assertAlmostEqual(
            factor, rectangle_factor(1, 1, 1), delta=3 / np.sqrt(65536) + 1e-6
        )
        self.assertTrue(np.all(links[0][links[0] >= 0] == 3))

    def test_geometry_samples_survive_packet_changes(self):
        v = np.array(((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), float)
        f = np.array(((0, 1, 2), (0, 2, 3)))
        points = np.array(((0, 0, 0), (0, 0, 1)), float)
        normals = np.tile((0, 0, 1.0), (2, 1))
        answers = [
            build_links(
                ScatterScene(v, f),
                points,
                normals,
                64,
                lambda ids, p: np.ones(len(ids), int),
                packet=p,
            )
            for p in (19, 256)
        ]
        np.testing.assert_array_equal(*answers)


if __name__ == "__main__":
    unittest.main()
