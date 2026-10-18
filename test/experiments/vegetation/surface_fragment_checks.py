"""Curved alpha counterexamples and independent per-pixel source composition."""

import unittest

import numpy as np
from leaf_sheet import sample_image
from raster_batch import rasterize
from surface_fragments import mesh_hits, resolve, surface_hits


def two_layers():
    square = np.array(((0, 0, 0.2), (1, 0, 0.2), (1, 1, 0.2), (0, 1, 0.2)), float)
    vertices = np.vstack((square, square - np.array((0, 0, 0.2))))
    faces = np.array(((0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7)), np.int32)
    uv = np.array(((0.25, 0.5),) * 4 + ((0.75, 0.5),) * 4)
    return {
        "vertices": vertices,
        "faces": faces,
        "uv": uv,
        "normals": np.tile((0.0, 0.0, 1.0), (12, 1)),
    }


def image(alpha):
    a = np.asarray(alpha, float)
    return np.stack((a * 0.5, a * 0.5, a, a), axis=-1)


class AlphaFragments(unittest.TestCase):
    def test_transparent_front_does_not_hide_opaque_back(self):
        mesh = two_layers()
        normal = image(np.tile((0.0, 0.0, 1.0, 1.0), (4, 1)))
        bounds = (np.zeros(2), np.ones(2))
        packet = surface_hits(
            mesh,
            np.eye(3),
            bounds,
            8,
            owner=10,
            colour=normal,
            normal=normal,
            basis=np.eye(3),
        )
        result = resolve((packet,), np.array(((0, 0, 1),)), 8)
        np.testing.assert_array_equal(result[..., 0], 1)
        np.testing.assert_array_equal(result[..., 1], 1)
        np.testing.assert_array_equal(result[..., 2], 0)
        _, uv = rasterize(
            mesh["vertices"], mesh["faces"], mesh["uv"][mesh["faces"]], *bounds, 8
        )
        old = sample_image(normal, uv)[..., 3]
        np.testing.assert_array_equal(old, 0)

    def test_shared_diagonal_is_owned_once_but_distinct_layers_survive(self):
        mesh = two_layers()
        normal = image(np.full((4, 4), 0.5))
        packet = surface_hits(
            mesh,
            np.eye(3),
            (np.zeros(2), np.ones(2)),
            8,
            colour=normal,
            normal=normal,
            basis=np.eye(3),
        )
        self.assertEqual(len(packet.pixel), 2 * 8 * 8)
        result = resolve((packet,), np.array(((0, 0, 1),)), 8)
        np.testing.assert_allclose(result[..., 0], 0.75, rtol=0, atol=2e-14)
        np.testing.assert_allclose(result[..., 1], 0.75, rtol=0, atol=2e-14)

    def test_separate_source_owners_are_not_deduplicated(self):
        mesh = two_layers()
        mesh["faces"] = np.tile(mesh["faces"][:2], (2, 1))
        owners = np.array((0, 0, 2**64 - 1, 2**64 - 1), np.uint64)
        triangle, _, pixel, _, _ = mesh_hits(
            mesh, np.eye(3), (np.zeros(2), np.ones(2)), 8, owner=owners
        )
        self.assertEqual(len(pixel), 2 * 8 * 8)
        self.assertEqual(set(map(int, owners[triangle])), {0, 2**64 - 1})

    def test_face_order_and_packet_order_keep_coincident_source_ties_stable(self):
        mesh = two_layers()
        bounds = (np.zeros(2), np.ones(2))
        packet = surface_hits(mesh, np.eye(3), bounds, 8, owner=2)
        reversed_mesh = mesh | {
            "faces": mesh["faces"][::-1],
            "normals": mesh["normals"].reshape(-1, 3, 3)[::-1].reshape(-1, 3),
        }
        other = surface_hits(reversed_mesh, np.eye(3), bounds, 8, owner=2)
        lights = np.array(((0, 0, 1), (0, 1, 0), (0, 0, -1)), float)
        np.testing.assert_array_equal(
            resolve((packet,), lights, 8), resolve((other,), lights, 8)
        )
        # Same positions, different IDs and signed normals: lowest original ID wins.
        flipped = mesh | {"normals": -mesh["normals"]}
        lower = surface_hits(flipped, np.eye(3), bounds, 8, owner=0)
        a = resolve((packet, lower), lights, 8)
        b = resolve((lower, packet), lights, 8)
        np.testing.assert_array_equal(a, b)
        np.testing.assert_array_equal(a[..., 1], 0)
        np.testing.assert_array_equal(a[..., 4], 1)

    def test_raw_moment_is_not_normalized_and_rotates_before_light_response(self):
        mesh = two_layers()
        mesh = mesh | {"faces": mesh["faces"][:2]}
        # Unit mass but normal-vector length0.5 must stay0.5.
        normal = np.broadcast_to((0.5, 0.5, 0.75, 1.0), (4, 4, 4)).copy()
        basis = np.array(((0, 0, 1), (0, 1, 0), (-1, 0, 0)), float)
        p = surface_hits(
            mesh,
            np.eye(3),
            (np.zeros(2), np.ones(2)),
            4,
            colour=normal,
            normal=normal,
            basis=basis,
        )
        r = resolve((p,), np.array(((1, 0, 0), (-1, 0, 0)), float), 4)
        np.testing.assert_array_equal(r[..., 1], 0.5)
        np.testing.assert_array_equal(r[..., 2], 0)
        np.testing.assert_array_equal(r[..., 3], 0)
        np.testing.assert_array_equal(r[..., 4], 0.5)

    def test_depth_composition_matches_an_independent_scalar_loop(self):
        mesh = two_layers()
        normal = image(np.full((4, 4), 0.4))
        p = surface_hits(
            mesh,
            np.eye(3),
            (np.zeros(2), np.ones(2)),
            4,
            colour=normal,
            normal=normal,
            basis=np.eye(3),
        )
        lights = np.array(((0.6, 0, 0.8), (-0.6, 0, -0.8)), float)
        r = resolve((p,), lights, 4)
        expected = np.zeros_like(r)
        for pixel in range(16):
            indices = np.flatnonzero(p.pixel == pixel)
            indices = indices[np.argsort(-p.depth[indices], kind="stable")]
            transmission = 1.0
            for i in indices:
                expected.reshape(16, -1)[pixel, 0] += transmission * p.alpha[i]
                dots = p.moment[i] @ lights.T
                expected.reshape(16, -1)[pixel, 1:3] += transmission * np.maximum(
                    dots, 0
                )
                expected.reshape(16, -1)[pixel, 3:] += transmission * np.maximum(
                    -dots, 0
                )
                transmission *= 1 - p.alpha[i]
        np.testing.assert_allclose(r, expected, rtol=0, atol=2e-14)

    def test_capacity_is_explicit_and_empty_group_is_zero(self):
        with self.assertRaisesRegex(ValueError, "capacity"):
            mesh_hits(
                two_layers(), np.eye(3), (np.zeros(2), np.ones(2)), 8, max_fragments=0
            )
        np.testing.assert_array_equal(
            resolve((), np.array(((0, 0, 1),)), 4), np.zeros((4, 4, 3))
        )


if __name__ == "__main__":
    unittest.main()
