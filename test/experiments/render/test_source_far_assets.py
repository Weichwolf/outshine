"""Known layered geometry, overflow and unchanged normal/material/barycentric fields."""

import unittest

import numpy as np
from source_far_assets import capture_view


def source_planes():
    xyz = np.array(
        [
            [x, y, -z]
            for z in range(3)
            for x, y in ((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
        ],
        np.float32,
    )
    faces = np.array(
        [[4 * i, 4 * i + 1, 4 * i + 2] for i in range(3)]
        + [[4 * i, 4 * i + 2, 4 * i + 3] for i in range(3)],
        np.uint32,
    )
    layer = np.array([0, 1, 2, 0, 1, 2])
    return {
        "positions": xyz,
        "indices": faces,
        "normal": np.full(6, 123456, np.uint32),
        "code": layer.astype(np.int16),
        "palette": np.eye(4, dtype=np.float32),
        "colour_index": layer.astype(np.uint16),
        "uv": xyz[:, :2].copy(),
        "metres": None,
        "domain": np.zeros(6, np.uint8),
    }


class SourceFarAssetsTests(unittest.TestCase):
    def test_layers_and_source_bound_attributes(self):
        source = source_planes()
        data, info = capture_view(source, [0, 0, -1], [0, 1, 0], resolution=8, layers=3)
        self.assertEqual(info["stored_samples"], 192)
        self.assertEqual(info["overflow_rays"], 0)
        np.testing.assert_allclose(
            data["depth"].reshape(3, 64),
            np.repeat(np.array([[1], [2], [3]]), 64, axis=1),
        )
        np.testing.assert_array_equal(data["normal"], np.full(192, 123456, np.uint32))
        np.testing.assert_array_equal(
            data["code"].reshape(3, 64), np.tile(np.arange(3)[:, None], (1, 64))
        )
        self.assertTrue((data["binding_weights"].sum(1) == 1 << 30).all())
        points = np.einsum(
            "ni,nij->nj",
            data["binding_weights"].astype(float) / (1 << 30),
            source["positions"][source["indices"][data["binding_faces"]]],
        )
        np.testing.assert_allclose(
            data["uv"][data["binding_slots"]], points[:, :2], atol=1e-7
        )

    def test_layer_overflow_is_explicit(self):
        _, info = capture_view(
            source_planes(), [0, 0, -1], [0, 1, 0], resolution=8, layers=2
        )
        self.assertEqual(info["overflow_rays"], 64)
        self.assertEqual(info["dropped_hits"], 64)

    def test_backside_is_empty_and_invalid_capture_dimensions_rejected(self):
        data, info = capture_view(
            source_planes(), [0, 0, 1], [0, 1, 0], resolution=8, layers=2
        )
        self.assertEqual(info["stored_samples"], 0)
        self.assertTrue(np.isnan(data["depth"]).all())
        with self.assertRaises(ValueError):
            capture_view(source_planes(), [0, 0, 1], [0, 1, 0], resolution=0, layers=2)


if __name__ == "__main__":
    unittest.main()
