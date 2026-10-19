"""Mixed flat program IDs require explicit geometry-only observation."""

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
from source_far_assets import load_source


class MixedCodeTests(unittest.TestCase):
    def test_default_rejects_and_geometry_only_preserves_unknown_programs(self):
        with tempfile.TemporaryDirectory(prefix="outshine-code-control-") as tmp:
            root = Path(tmp)
            (root / "mesh/0").mkdir(parents=True)
            vertices = np.zeros((3, 6), "<u4")
            floats = vertices[:, :5].view("<f4")
            floats[:, :3] = [[0, 0, 0], [1, 0, 0], [0, 1, 0]]
            floats[:, 3] = [256, 512, 512]
            vertices[:, 5] = 0x80008000
            streams = {
                "walls": vertices.tobytes(),
                "roofs": b"",
                "wall_indices": np.array([0, 1, 2], "<u4").tobytes(),
                "roof_indices": b"",
                "colours": b"",
                "metres": b"",
            }
            capture = {}
            for name, data in streams.items():
                (root / "mesh/0" / f"{name}.bin").write_bytes(data)
                capture[name] = {
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            (root / "mesh/receipt.json").write_text(json.dumps({"captures": [capture]}))
            (root / "input.json").write_text("{}")
            with self.assertRaises(ValueError):
                load_source(root)
            result = load_source(root, allow_mixed_codes=True)
            np.testing.assert_array_equal(result["vertex_code"], [1, 2, 2])
            np.testing.assert_array_equal(result["code_uniform"], [False])
            np.testing.assert_array_equal(result["code"], [-32768])
            np.testing.assert_array_equal(result["positions"], floats[:, :3])


if __name__ == "__main__":
    unittest.main()
