"""Independent small matrices, energy, sides and analytic geometric transfer."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from diffuse_scatter import uniform_samples
from exchange_io import read_exchange, write_exchange
from exchange_light import basis_optics, direct_nodes
from exchange_operator import build_links, gather_apply, mix_sides, prepare_csr
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response
from scatter_reference import rectangle_factor
from scatter_render import camera_packet
from scatter_scene import ScatterScene


class ExchangeChecks(unittest.TestCase):
    def test_current_light_and_material_match_the_existing_sheet_reference(self):
        basis = {
            "points": np.array(((0, 0, 0.2), (0, 0, 0)), float),
            "normals": np.array(((1, 0, 0), (0, 0, 1.0))),
            "leaf_nodes": 1,
            "ground_grid": 1,
        }
        lights = [
            {"direction": [np.sqrt(3) / 2, 0, 0.5], "irradiance_rgb": [3, 2.7, 2.4]},
            {"direction": [-0.6, 0, 0.8], "irradiance_rgb": [0.15, 0.22, 0.35]},
        ]
        states = {
            "a": {
                "leaf_rt": [0.1, 0.2, 0.05, 0.03, 0.08, 0.02],
                "ground_albedo": [0.2, 0.13, 0.055],
                "lights": lights,
            },
            "b": {
                "leaf_rt": [0.05, 0.1, 0.025, 0.06, 0.16, 0.04],
                "ground_albedo": [0.2, 0.13, 0.055],
                "lights": lights,
            },
        }
        scene = ScatterScene(np.empty((0, 3)), np.empty((0, 3), int))
        actual = direct_nodes(scene, basis, states)
        for name, state in states.items():
            optical = basis_optics(basis, state)
            expected = np.zeros((2, 2, 3))
            for light in lights:
                for side in (0, 1):
                    expected[:, side] += (
                        leaf_diffuse_response(
                            optical,
                            basis["normals"],
                            basis["normals"] * (1 if side == 0 else -1),
                            light["direction"],
                        )
                        * light["irradiance_rgb"]
                    )
            np.testing.assert_allclose(
                actual[name], expected.reshape(-1, 3), atol=1e-16
            )
        self.assertEqual(scene.shadow_work["rays"], 4)

    def test_geometry_product_rejects_stale_pose_corruption_and_lighting_fields(self):
        basis = {
            "points": np.array(((0, 0, 1), (0, 0, 0)), float),
            "normals": np.tile((0, 0, 1.0), (2, 1)),
            "leaf_nodes": 1,
            "ground_grid": 1,
            "geometry_sha256": "a" * 64,
            "stand_identity": "b" * 64,
        }
        links = np.full((4, 4), -1, np.int32)
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.vegetation-exchange-"
        ) as tmp:
            path = Path(tmp) / "product"
            header = write_exchange(path, basis, links, 4, 997, 2e-6)
            loaded, actual = read_exchange(path, "a" * 64)
            np.testing.assert_array_equal(actual, links)
            np.testing.assert_array_equal(loaded["points"], basis["points"])
            with self.assertRaises(ValueError):
                read_exchange(path, "c" * 64)
            (path / "geometry.json").write_text(
                json.dumps({**header, "current_light": 1})
            )
            with self.assertRaises(ValueError):
                read_exchange(path, "a" * 64)
            (path / "geometry.json").write_text(json.dumps(header))
            with (path / "geometry.npz").open("ab") as f:
                f.write(b"corrupt")
            with self.assertRaises(ValueError):
                read_exchange(path, "a" * 64)

    def test_default_camera_is_unchanged_and_rotated_rays_are_orthogonal(self):
        view = np.array((np.sqrt(3) / 2, 0, 0.5))
        width, height, samples = 16, 9, 4
        random = uniform_samples(73, 0, width * height * samples)
        ids = np.repeat(np.arange(width * height), samples)
        x = ((ids % width + random[:, 0]) / width - 0.5) * 1.4
        y = (0.5 - (ids // width + random[:, 1]) / height) * (1.4 * height / width)
        expected = (
            4 * view
            + x[:, None] * np.array((0, 1, 0.0))
            + y[:, None] * np.cross(view, (0, 1, 0.0))
        )
        actual, _ = camera_packet(view, width, height, samples, 0, width * height, 73)
        np.testing.assert_array_equal(actual, expected)
        north = np.array((0, np.sqrt(3) / 2, 0.5))
        rotated, directions = camera_packet(
            north, width, height, samples, 0, width * height, 73
        )
        np.testing.assert_allclose((rotated - 4 * north) @ north, 0, atol=1e-15)
        np.testing.assert_allclose(np.linalg.norm(directions, axis=1), 1, atol=1e-15)

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
