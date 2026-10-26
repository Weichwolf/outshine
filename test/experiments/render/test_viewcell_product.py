"""Stored support preserves original fields and rejects unmet demand before IO."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from packed_normals import pack
from viewcell_product import cook, load_product, query


class ViewcellProductTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="outshine-viewcell-test-")
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "product"
        self.profile = {"width": 2, "height": 2, "focal_px": 1}
        self.box = np.array([[-0.1, -0.1, 0], [0.1, 0.1, 0]])
        self.source = {
            "positions": np.array([[-1, -1, 1], [-1, 1, 1], [1, -1, 1]], np.float32),
            "indices": np.array([[0, 1, 2]]),
            "uv": np.array([[512, 0], [512, 2], [514, 0]], np.float32),
            "normal": pack(np.tile([0, 0, -1.0], (3, 1))),
            "metres": np.array([[0, 0], [0, 20], [20, 0]], np.float32),
            "colour": np.array(
                [[1, 0, 0, 0.1], [0, 1, 0, 0.5], [0, 0, 1, 0.9]], np.float32
            ),
            "binding": np.array([[5, 91, 3, 7, 0, 0]], np.int32),
        }

    def test_affine_fields_and_alpha_roundtrip(self):
        cook(self.source, self.box, self.path, {"input": "known-plane"}, self.profile)
        product = load_product(self.path, [[0, 0, 0]], {"input": "known-plane"})
        depth, fields, owner, tag = query(
            product, np.zeros((1, 3)), np.array([[-0.5, -0.5, 1]])
        )
        np.testing.assert_array_equal(depth, [1])
        np.testing.assert_array_equal(owner, [91])
        np.testing.assert_array_equal(tag, [0])
        np.testing.assert_array_equal(fields["code"], [2])
        np.testing.assert_allclose(fields["coordinates"], [[0.5, 0.5]])
        np.testing.assert_allclose(fields["metres"], [[5, 5]])
        np.testing.assert_allclose(fields["colour"], [[0.5, 0.25, 0.25, 0.4]])
        with self.assertRaisesRegex(ValueError, "near-clip guard"):
            query(product, np.zeros((1, 3)), np.array([[0, 0, 100]]))
        with self.assertRaisesRegex(ValueError, "outside supported domain"):
            query(product, np.array([[0.11, 0, 0]]), np.array([[0, 0, 1]]))

    def test_domain_and_identity_rejected_before_payload_open(self):
        cook(self.source, self.box, self.path, {"input": "plane"}, self.profile)
        (self.path / "surfaces.npz").unlink()
        with self.assertRaisesRegex(ValueError, "outside supported domain"):
            load_product(self.path, [[0.11, 0, 0]], {"input": "plane"})
        with self.assertRaisesRegex(ValueError, "identity mismatch"):
            load_product(self.path, [[0, 0, 0]], {"input": "other"})
        with self.assertRaises(FileNotFoundError):
            load_product(self.path, [[0, 0, 0]], {"input": "plane"})

    def test_empty_support_and_default_fields(self):
        self.source["indices"] = self.source["indices"][:, ::-1]
        self.source["colour"][:] = 1
        self.source["metres"][:] = 0
        cook(self.source, self.box, self.path, {}, self.profile)
        product = load_product(self.path, [[0, 0, 0]], {})
        depth, fields, owner, tag = query(
            product, np.zeros((1, 3)), np.array([[0, 0, 1]])
        )
        self.assertTrue(np.isinf(depth[0]))
        np.testing.assert_array_equal(owner, [-1])
        np.testing.assert_array_equal(tag, [-1])
        self.assertEqual(fields["colour"].shape, (1, 4))
        self.assertEqual(product["header"]["omitted_sources"][0]["source"], 91)


if __name__ == "__main__":
    unittest.main()
