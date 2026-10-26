"""Separate immutable scene bodies preserve all imports and single-body behavior."""

import unittest
from unittest.mock import patch

import numpy as np
from render_transport_research import import_scene


class SceneChecks(unittest.TestCase):
    def test_existing_single_body_delegates_without_copy(self):
        row = {"id": "old"}
        value = np.array([[1.0, 2, 3]])
        expected = value, {"topology_exact": True}
        with patch(
            "render_transport_research.import_body", return_value=expected
        ) as importer:
            self.assertIs(import_scene(row), expected)
        importer.assert_called_once_with(row)

    def test_multiple_bodies_retain_every_position_hash_and_proof(self):
        rows = [{"id": "road", "mesh_sha256": "a"}, {"id": "plant", "mesh_sha256": "b"}]
        values = [np.array([[1.0, 2, 3]]), np.array([[4.0, 5, 6]])]
        with patch(
            "render_transport_research.import_body",
            side_effect=[(v, {"topology_exact": True}) for v in values],
        ):
            actual, proof = import_scene({"bodies": rows})
        np.testing.assert_array_equal(actual, np.vstack(values))
        self.assertEqual([p["mesh_sha256"] for p in proof["parts"]], ["a", "b"])
        self.assertTrue(all(p["topology_exact"] for p in proof["parts"]))

    def test_empty_duplicate_or_unbounded_bodies_fail_before_import(self):
        for bodies in (
            [],
            [{"id": "x"}, {"id": "x"}],
            [{"id": str(i)} for i in range(65)],
        ):
            with patch("render_transport_research.import_body") as importer:
                with self.assertRaisesRegex(ValueError, "bounded distinct"):
                    import_scene({"bodies": bodies})
                importer.assert_not_called()


if __name__ == "__main__":
    unittest.main()
