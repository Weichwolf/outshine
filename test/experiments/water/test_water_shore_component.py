import gzip
import json
import unittest
from pathlib import Path

import numpy as np
from native_water_prepared import bind_export, decode
from water_shore_component import component

FIXTURE = Path(__file__).parents[1] / "transport/fixtures/feldkirch-shore-contact"


class PreparedWaterCapture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = gzip.decompress((FIXTURE / "water.native.gz").read_bytes())

    def test_actual_export_words_bind_current_metadata_and_mesh(self):
        data = decode(self.raw)
        with gzip.open(FIXTURE / "contact.json.gz", "rt") as stream:
            exported = json.load(stream)["water"]
        bind_export(data, exported)
        surface = data["surfaces"][284]
        info = data["inputs"][int(surface["input"])]
        self.assertEqual(info["provider_feature_id"], "134415393")
        self.assertEqual(
            data["sources"][info["source"]]["revision"], "20261025_080012_pt"
        )
        exported["vertices"][0][0] += 1
        with self.assertRaises(ValueError):
            bind_export(data, exported)

    def test_versions_incomplete_and_trailing_payloads_are_refused(self):
        for bad in (b"", b"WATER2xx" + self.raw[8:], self.raw[:-1], self.raw + b"x"):
            with self.assertRaises(ValueError):
                decode(bad)


class NativeComponentTopology(unittest.TestCase):
    def mesh(self):
        xy = np.array(
            [
                [0, 0],
                [4, 0],
                [4, 4],
                [0, 4],
                [1, 1],
                [3, 1],
                [3, 3],
                [1, 3],
                [-1, 0],
                [0, -1],
            ],
            float,
        )
        faces = []
        for i in range(4):
            j = (i + 1) % 4
            faces.extend([[i, j, 4 + j, 0], [i, 4 + j, 4 + i, 0]])
        faces.append([0, 8, 9, 0])
        return {
            "anchor": np.zeros(2),
            "vertices": np.c_[xy, np.zeros(len(xy))],
            "faces": np.array(faces, dtype="<u4"),
            "face_source_offsets": np.arange(10, dtype="<u4"),
            "face_sources": np.zeros(9, dtype="<u4"),
        }

    def test_whole_annulus_has_two_loops_and_does_not_join_point_contact(self):
        data = self.mesh()
        result = component(data, 0)
        self.assertEqual(result["original_face_ids"].tolist(), list(range(8)))
        self.assertEqual(len(result["loop_offsets"]), 3)
        restored = result["original_vertex_ids"][result["faces"]]
        self.assertEqual(restored.tobytes(), data["faces"][:8, :3].tobytes())
        self.assertEqual(result["vertices"].tobytes(), data["vertices"][:8].tobytes())

    def test_selected_nonmanifold_component_and_missing_seed_refuse(self):
        data = self.mesh()
        data["faces"] = np.r_[data["faces"], data["faces"][:1]]
        for seed in (-1, 10):
            with self.assertRaises(ValueError):
                component(data, seed)
        with self.assertRaisesRegex(ValueError, "Nonmanifold"):
            component(data, 0)


if __name__ == "__main__":
    unittest.main()
