"""Kept geometric normals and an actual off-camera ground caster control."""

import unittest

import numpy as np
from compare_vertex_cluster_meshes import ready_source, render
from packed_normals import pack, unpack
from triangle_image_reference import prepare_source


def empty_retained():
    return {
        k: np.empty((0, size), np.int64 if k == "indices" else float)
        for k, size in (
            ("positions", 3),
            ("indices", 3),
            ("vertex_normal", 3),
            ("uv", 2),
            ("metres", 2),
            ("colour", 4),
        )
    }


class ClusterImageTest(unittest.TestCase):
    def test_triangle_normal_words_do_not_become_vertex_averages(self):
        words = pack(np.array([[0, 0, 1], [0, 1, 0]], float))
        arrays = {
            "positions": np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1]], np.float32),
            "indices": np.array([[0, 1, 2], [0, 2, 1]], np.uint16),
            "face_normal_words": words,
        }
        source = ready_source(arrays, empty_retained())
        np.testing.assert_array_equal(
            source["vertex_normal"], np.repeat(unpack(words), 3, axis=0)
        )
        self.assertEqual(len(source["positions"]), 6)

    def test_opaque_hit_ground_caster_and_sky_compose_without_original_depth(self):
        arrays = {
            "positions": np.array([[-1, -1, 1], [1, -1, 1], [0, 1, 1]], np.float32),
            "indices": np.array([[0, 1, 2]], np.uint16),
            "face_normal_words": pack(np.array([[0, 0, 1]], float)),
        }
        source = ready_source(arrays, empty_retained())
        origins = np.array([[0, 0, 3], [2, 0, 3], [10, 10, 3], [0, 0, 3]], float)
        directions = np.array([[0, 0, -1], [-2, 0, -3], [0, 0, -1], [0, 0, 1]], float)
        directions /= np.linalg.norm(directions, axis=1)[:, None]
        profile = {
            "kind": "sun",
            "direction": [0, 0, 1],
            "intensity": 3,
            "ambient": 0.1,
            "sky": [0.1] * 3,
        }
        raw, _ = render(source, prepare_source(source), origins, directions, 0, profile)
        np.testing.assert_array_equal(raw["source_hit"], [True, False, False, False])
        self.assertEqual(raw["receiver_visibility"][1], 0)
        self.assertEqual(raw["receiver_visibility"][2], 1)
        np.testing.assert_allclose(
            raw["rgb"],
            raw["colour"]
            + raw["transmission"][:, None]
            * (raw["receiver_visibility"][:, None] * raw["direct"] + raw["other"]),
        )
        np.testing.assert_allclose(raw["rgb"][3], profile["sky"])
