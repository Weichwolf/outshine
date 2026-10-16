"""Independent solids, source-surface preservation and rejected incomplete requests."""

import unittest
from types import SimpleNamespace
from unittest.mock import patch

import manifold3d as manifold
import numpy as np
from crown_union import (
    flat_fragment,
    join_sources,
    overlapped_shoot,
    ring_cells,
    wood_sources,
)
from crown_union_replay import topology
from join_normal_checks import check_weighting
from outshine_ref.vegetation.axis_surface import axis_mesh
from plant_surface_checks import winding_inside
from spatial_crown_replay import check_mesh


def volume(mesh):
    v = mesh["vertices"] - mesh["vertices"].mean(axis=0)
    p = v[mesh["faces"]]
    return np.einsum("ij,ij->i", p[:, 0], np.cross(p[:, 1], p[:, 2])).sum() / 6


def cube(offset, owner):
    raw = manifold.Manifold.cube((1, 1, 1)).to_mesh64()
    vertices = np.asarray(raw.vert_properties)[:, :3] + offset
    faces = np.asarray(raw.tri_verts)
    p = vertices[faces]
    n = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    n /= np.linalg.norm(n, axis=1)[:, None]
    return {
        "vertices": vertices,
        "faces": faces,
        "normals": np.repeat(n, 3, axis=0),
        "source_kind": 0,
        "source_node": owner,
        "cell": -1,
        "forbidden_caps": (),
    }


class CrownUnionChecks(unittest.TestCase):
    def test_existing_scalar_weighting_split_and_hard_edge_controls(self):
        check_weighting()

    def test_shoot_prefix_preserves_original_sides_and_adds_analytic_polygonal_volume(
        self,
    ):
        old = axis_mesh(0.16, 0.4, 0.0005, 0.00012, 3, 4)
        new = overlapped_shoot(old | {"source_node": 65})
        np.testing.assert_array_equal(
            new["vertices"][new["faces"][8:-8]], old["vertices"][old["faces"][:-8]]
        )
        np.testing.assert_array_equal(new["normals"][24:-24], old["normals"][:-24])
        np.testing.assert_allclose(
            new["vertices"][:4], old["vertices"][:4] - (0, 0.0005, 0), atol=1e-18
        )
        self.assertAlmostEqual(volume(new) - volume(old), 2 * 0.0005**3, places=20)
        self.assertEqual(new["source_node"], 65)
        check_mesh({key: new[key] for key in ("vertices", "faces", "normals")})

    def test_ring_cells_keep_each_source_side_and_the_original_enclosed_volume(self):
        mesh = axis_mesh(1.2, 0.5, 0.04, 0.02, 4, 8)
        cells = list(ring_cells(mesh, 5, 7))
        self.assertEqual(len(cells), 4)
        original = mesh["vertices"][mesh["faces"][:-16]]
        actual = np.concatenate([p["vertices"][p["faces"][:16]] for p in cells])
        np.testing.assert_array_equal(actual, original)
        self.assertAlmostEqual(sum(volume(p) for p in cells), volume(mesh), places=14)
        joined, _ = join_sources(cells)
        self.assertAlmostEqual(volume(joined), volume(mesh), places=14)
        check_mesh(joined)

    def test_union_matches_analytic_volume_occupancy_and_source_identity(self):
        sources = [cube((0, 0, 0), 7), cube((0.5, 0, 0), 9)]
        points = np.array(
            ((0.25, 0.3, 0.4), (0.75, 0.3, 0.4), (1.25, 0.3, 0.4), (1.75, 0.3, 0.4))
        )
        for order in (sources, sources[::-1]):
            mesh, work = join_sources(order)
            self.assertAlmostEqual(volume(mesh), 1.5, places=14)
            self.assertAlmostEqual(topology(mesh)["signed_volume_m3"], 1.5, places=14)
            np.testing.assert_array_equal(
                winding_inside(mesh, points), (True, True, True, False)
            )
            self.assertEqual(set(mesh["source_node"]), {7, 9})
            self.assertFalse(work["discarded_flat_fragments"])
            check_mesh(mesh)

    def test_only_tiny_bounded_flat_shells_are_discardable(self):
        v = np.array(((0, 0, 0), (0.01, 0, 0), (0, 0.01, 0), (0.003, 0.003, 1e-16)))
        raw = SimpleNamespace(vert_properties=v, tri_verts=np.empty((4, 3)))
        self.assertIsNotNone(flat_fragment(raw))
        v[-1, -1] = 1e-6
        self.assertIsNone(flat_fragment(raw))
        v[-1, -1] = 1e-16
        raw.tri_verts = np.empty((10, 3))
        self.assertIsNone(flat_fragment(raw))

    def test_missing_complete_output_is_never_a_partial_wood_asset(self):
        sources = [cube((0, 0, 0), 7)]
        with patch(
            "crown_union.manifold.Mesh64",
            side_effect=AssertionError("kernel before demand"),
        ):
            self.assertIsNone(join_sources(sources, max_output_faces=0))
        self.assertIsNone(join_sources(sources, max_output_faces=1))
        with self.assertRaises(ValueError):
            join_sources(sources + [cube((3, 0, 0), 9)])
        with patch(
            "crown_union.ring_cells", side_effect=AssertionError("unbudgeted cells")
        ):
            self.assertIsNone(wood_sources({}, [(0, 1)] * 8193, 1, {}, ()))


if __name__ == "__main__":
    unittest.main()
