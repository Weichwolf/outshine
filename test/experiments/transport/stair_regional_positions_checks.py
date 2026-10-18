"""Real independent neighboring origins and exact shared encoded boundary ownership."""

import unittest
from pathlib import Path

import numpy as np
import shapely
from stair_regional_positions import encode_regions, reconstruct
from stair_regional_precision import boundary_vertices, encode
from stair_regional_snapshot_verify import whole_case


def terrain():
    path = (
        Path(__file__).parents[2]
        / "outshine/data/transport/regional-terrain-topology.npz"
    )
    with np.load(path, allow_pickle=False) as data:
        return data["xy"], data["faces"]


class RegionalPositionStreams(unittest.TestCase):
    def test_independent_reader_rejects_a_changed_interior_or_neighbor_point(self):
        points, faces = terrain()
        anchors = boundary_vertices(faces)
        origin, peer_origin = np.array([-704.0, 320.0]), np.array([-768.0, 384.0])
        peer, _world = encode(points[anchors], peer_origin)
        streams = encode_regions(points, origin, [(anchors, peer, peer_origin)])
        names = ("positions", "origins", "origin_ids")
        data = dict(zip(names, streams, strict=True))
        whole_case(data, points, faces, anchors, origin, peer_origin)
        interior = next(i for i in range(len(points)) if i not in set(anchors))
        for vertex in (interior, int(anchors[0])):
            changed = {k: v.copy() for k, v in data.items()}
            changed["positions"][vertex, 0] += 0.01
            with self.assertRaises(ValueError):
                whole_case(changed, points, faces, anchors, origin, peer_origin)

    def test_real_neighbor_origins_need_more_than_independent_single_origin_rounding(
        self,
    ):
        points, faces = terrain()
        selected = boundary_vertices(faces)
        origin = np.array([-704.0, 320.0])
        for peer_origin in (np.array([-768.0, 384.0]), np.array([-640.0, 256.0])):
            peer, world = encode(points, peer_origin)
            with self.assertRaisesRegex(ValueError, "not exactly representable"):
                encode(
                    points, origin, anchor_ids=selected, anchor_world_m=world[selected]
                )
            results = []
            for method in ("copy", "rebase_if_exact"):
                streams = encode_regions(
                    points,
                    origin,
                    [(selected, peer[selected], peer_origin)],
                    method=method,
                )
                actual = reconstruct(*streams)
                np.testing.assert_array_equal(actual[selected], world[selected])
                triangles = actual[faces]
                a, b = (
                    triangles[:, 1] - triangles[:, 0],
                    triangles[:, 2] - triangles[:, 0],
                )
                self.assertTrue(np.all(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] > 0))
                self.assertTrue(shapely.coverage_is_valid(shapely.polygons(triangles)))
                results.append(actual)
            np.testing.assert_array_equal(*results)

    def test_three_dimensions_and_actual_peer_bytes_survive_without_requantization(
        self,
    ):
        points = np.array(
            [[1e9 + 0.125, 1e9 + 0.25, 1e9 + 0.5], [1e9 + 1, 1e9 + 2, 1e9 + 3]]
        )
        peer_origin = np.full(3, 1e9)
        peer, world = encode(points, peer_origin)
        streams = encode_regions(
            points,
            np.zeros(3),
            [(np.array([0]), peer[[0]], peer_origin)],
            method="copy",
        )
        actual = reconstruct(*streams)
        np.testing.assert_array_equal(actual[0], world[0])
        np.testing.assert_array_equal(streams[0][0].view("u4"), peer[0].view("u4"))

    def test_legacy_peer_can_rebase_exactly_without_an_extra_per_vertex_stream(self):
        points, faces = terrain()
        selected = boundary_vertices(faces)
        peer, world = encode(points, np.zeros(2))
        streams = encode_regions(
            points, np.array([-704.0, 320.0]), [(selected, peer[selected], np.zeros(2))]
        )
        self.assertEqual(len(streams[1]), 1)
        self.assertEqual(streams[2].nbytes, 0)
        np.testing.assert_array_equal(reconstruct(*streams)[selected], world[selected])

    def test_ambiguous_owners_incomplete_streams_and_budget_violations_are_rejected(
        self,
    ):
        points, _faces = terrain()
        peer, _world = encode(points, np.zeros(2))
        group = (np.array([0]), peer[[0]], np.zeros(2))
        with self.assertRaisesRegex(ValueError, "disjoint"):
            encode_regions(points, np.zeros(2), [group, group])
        with self.assertRaises(ValueError):
            encode_regions(points, np.zeros(2), [group] * 33)
        for origins, ids in (
            (np.zeros((2, 2)), np.zeros(0, dtype="u1")),
            (np.zeros((2, 2)), np.full(len(points), 2, dtype="u1")),
        ):
            with self.assertRaises(ValueError):
                reconstruct(peer, origins, ids)


if __name__ == "__main__":
    unittest.main()
