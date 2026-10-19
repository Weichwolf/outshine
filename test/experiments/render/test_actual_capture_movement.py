"""Actual native byte parity, unknown identity and independent hit controls."""

import unittest
from pathlib import Path

import numpy as np
from check_building_capture_inputs import VERTEX, expanded_stream, load
from compare_actual_capture_movement import errors
from native_capture_meshes import REPRESENTATIONS, independent_depth, mesh, sample
from triangle_image_reference import prepare_source

FIXTURE = (
    Path(__file__).resolve().parents[2]
    / "outshine/data/render/building-capture-movement/native-cell.json.gz"
)


class ActualCaptureMovement(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.document = load(FIXTURE)
        cls.call = cls.document["results"][0]["projection_calls"][0]

    def test_array_adapter_preserves_every_expanded_native_attribute_byte(self):
        for representation in REPRESENTATIONS:
            actual = mesh(self.call, representation)
            encoded = b"".join(
                VERTEX.pack(
                    *actual["positions"][i],
                    *actual["uv"][i],
                    int(actual["normal"][i]),
                    *actual["colour"][i],
                    *actual["metres"][i],
                )
                for i in actual["indices"].ravel()
            )
            chunks = []
            for source in self.call["capture"]["sources"]:
                chunks.extend(
                    [source["native_fine_reference"]]
                    if representation == "Fine"
                    else source["reference" if representation == "Full" else "captured"]
                )
            expected = b"".join(
                expanded_stream(chunk, name)
                for chunk in chunks
                for name in ("walls", "roofs")
            )
            self.assertEqual(encoded, expected, representation)

    def test_native_source_slots_and_unknown_whole_mesh_faces_are_retained(self):
        actual = mesh(self.call, "Captured")
        self.assertEqual(len(actual["indices"]), 691)
        for slot, member in enumerate(self.call["members"]):
            binding = actual["binding"][actual["binding"][:, 0] == slot]
            self.assertTrue((binding[:, 1] == member["source"]).all())
            if self.call["capture"]["sources"][slot]["emission"] == 0:
                self.assertTrue((binding[:, 2:4] == -1).all())

    def test_ray_reader_interpolates_at_the_hit_and_culls_original_winding(self):
        source = {
            "positions": np.array([[0, 0, 2], [0, 2, 2], [2, 0, 2]], np.float32),
            "indices": np.array([[0, 1, 2]]),
            "vertex_normal": np.tile([0, 0, -1.0], (3, 1)),
            "colour": np.eye(3, 4),
            "uv": np.array([[0, 0], [0, 1], [1, 0]]),
            "metres": np.array([[2, 3], [2, 5], [4, 3]]),
        }
        origins = np.array([[0.5, 0.5, 0], [3, 3, 0], [0.5, 0.5, 3]])
        directions = np.array([[0, 0, 1], [0, 0, 1], [0, 0, -1]])
        depth, faces, fields = sample(
            source, prepare_source(source), origins, directions
        )
        np.testing.assert_array_equal(faces, [0, -1, -1])
        self.assertEqual(depth[0], 2)
        np.testing.assert_array_equal(fields["colour"][0], [0.5, 0.25, 0.25])
        np.testing.assert_array_equal(fields["uv"][0], [0.25, 0.25])
        np.testing.assert_array_equal(fields["metres"][0], [2.5, 3.5])
        control = independent_depth(
            source["positions"][source["indices"]], origins, directions
        )
        np.testing.assert_array_equal(depth, control)

    def test_coverage_and_source_errors_are_not_hidden_by_common_hits(self):
        fields = {
            "normal": np.tile([0, 0, 1], (3, 1)),
            "colour": np.ones((3, 3)),
            "uv": np.zeros((3, 2)),
            "metres": np.zeros((3, 2)),
        }
        first = (np.array([2, 2, np.inf]), np.array([0, 0, -1]), fields)
        second = (np.array([2.5, np.inf, 2]), np.array([0, -1, 0]), fields)
        result = errors(
            first,
            second,
            ({"binding": np.array([[0, 3]])}, {"binding": np.array([[0, 4]])}),
        )
        self.assertEqual(result["lost_samples"], 1)
        self.assertEqual(result["gained_samples"], 1)
        self.assertEqual(result["different_known_source_samples"], 1)
        self.assertEqual(result["max_depth_difference_m"], 0.5)


if __name__ == "__main__":
    unittest.main()
