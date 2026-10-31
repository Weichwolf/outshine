import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from conforming_body_contact import contact_error
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from pool_terrain_contact import ContactSurface
from shore_adaptive_terrain import load
from shore_bank_inputs import road_rim
from shore_native_terrain import load as uniform_load

FIXTURE = Path(__file__).parents[1] / "transport/fixtures/feldkirch-shore-contact"


class AdaptiveNativeTerrain(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((FIXTURE / "input.json").read_bytes())
        cls.anchor = [9.598371, 47.232575]
        cls.data, cls.note = load(FIXTURE, cls.anchor, cls.manifest)

    def test_all_original_fine_positions_and_height_words_survive(self):
        vertices, _, heights = uniform_load(FIXTURE, self.anchor, self.manifest)
        pages = self.manifest["pages"]
        x0, y0 = min(p["x"] for p in pages), min(p["y"] for p in pages)
        x, y = np.meshgrid(
            x0 * 32 + np.arange(heights.shape[1]), y0 * 32 + np.arange(heights.shape[0])
        )
        lookup = {tuple(key): i for i, key in enumerate(self.data["posting_keys"])}
        ids = [lookup[key] for key in zip(x.ravel(), y.ravel(), strict=True)]
        self.assertEqual(self.data["vertices"][ids].tobytes(), vertices.tobytes())
        self.assertEqual(
            self.data["height_words"][ids].tobytes(),
            heights.ravel().view("<u4").tobytes(),
        )

    def test_actual_adaptive_parent_coverage_uses_no_new_height_samples(self):
        data = self.data
        self.assertEqual(self.note["pages"], 7)
        self.assertEqual(self.note["input_faces"], 14336)
        self.assertEqual(self.note["output_faces"], 14400)
        self.assertEqual(len(data["vertices"]), 7361)
        self.assertEqual(self.note["split_edges"], 64)
        polygons = shapely.polygons(
            data["posting_keys"][data["original_faces"][data["source_parent_faces"]]]
        )
        children = shapely.polygons(data["posting_keys"][data["faces"]])
        self.assertTrue(np.all(shapely.covers(polygons, children)))
        self.assertEqual(
            set(data["source_pages"]),
            {int(p["page"]) for p in self.manifest["neighbor_pages"]["pages"]},
        )
        self.assertTrue(
            shapely.coverage_is_valid(
                shapely.polygons(data["vertices"][data["faces"], :2])
            )
        )

    def test_a_changed_shared_native_word_is_rejected(self):
        with np.load(FIXTURE / "deformed-neighbors.npz", allow_pickle=False) as archive:
            arrays = dict(archive)
        arrays["p19_276118_183901"][17, 33] += 1
        with tempfile.TemporaryDirectory(
            prefix="outshine-tests.outshine_research."
        ) as tmp:
            root = Path(tmp)
            np.savez(root / "deformed-neighbors.npz", **arrays)
            with self.assertRaisesRegex(ValueError, "shared original"):
                load(root, self.anchor, self.manifest)

    def test_explicit_road_level_rim_uses_the_same_supplied_profile(self):
        road = np.array([[0, 1, 16], [1, 1, 16.2], [5, 1, 17]], float)
        np.testing.assert_allclose(
            road_rim(road, [[0, 0], [2, 0], [5, 0]]), [16, 16.4, 17], rtol=0, atol=1e-14
        )
        with self.assertRaises(ValueError):
            road_rim(road[[0, 0, 2]], [[0, 0]])


class AffineContactWitness(unittest.TestCase):
    def test_signed_extrema_keep_actual_points_and_original_face_indices(self):
        faces = np.array([[0, 1, 2]])
        source = np.array([[0, 0, 10], [1, 0, 10], [0, 1, 10]], float)
        terrain = np.array([[0, 0, 10], [1, 0, 13], [0, 1, 9]], float)
        model = ContactSurface(SurfaceSampler(Mesh(source, faces)))
        old = contact_error(model, terrain, faces)
        self.assertNotIn("signed_max_witness", old)
        result = contact_error(model, terrain, faces, include_witness=True)
        self.assertEqual(result["signed_max_witness"]["xy_m"], [1, 0])
        self.assertEqual(result["signed_max_witness"]["signed_error_m"], 3)
        self.assertEqual(result["signed_min_witness"]["xy_m"], [0, 1])
        self.assertEqual(result["signed_min_witness"]["signed_error_m"], -1)
        self.assertEqual(result["signed_max_witness"]["terrain_face"], 0)


if __name__ == "__main__":
    unittest.main()
