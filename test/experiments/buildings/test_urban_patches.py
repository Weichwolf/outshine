"""Urban source uncertainty, no-import cache hits and independent geometry controls."""

import copy
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from coarse_city_domains import domains
from urban_patch_bake import bake
from urban_patch_mesh import build
from urban_patch_trace import trace
from urban_patches import UrbanPatches, load


def example():
    tiers = np.array([[[1, 0, 2], [3, 1, 0], [0, 2, 1]]], np.uint8)
    return UrbanPatches(
        np.array([[0, 0]], np.int32),
        tiers,
        np.array([0, 6, 12, 24], float),
        np.array([0, 0, 1, 16, 0.75]),
        np.array([[[0, 0, 0], [48, 48, 24]]], np.float32),
        np.array([0, 1], np.uint32),
        np.array([0], np.uint32),
    )


def source_document():
    return {
        "tile": [14, 8192, 8192],
        "source": {"sha256": "synthetic", "bytes": 0},
        "building_layer_present": False,
        "layers": {
            "landuse": {
                "extent": 4096,
                "features": [
                    {
                        "id": 1,
                        "properties": {"class": "residential"},
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [[0, 0], [4096, 0], [4096, 4096], [0, 4096], [0, 0]]
                            ],
                        },
                    }
                ],
            },
            "water": {
                "extent": 4096,
                "features": [
                    {
                        "id": 2,
                        "properties": {"class": "lake"},
                        "geometry": {
                            "type": "Polygon",
                            "coordinates": [
                                [
                                    [1000, 1000],
                                    [2000, 1000],
                                    [2000, 2000],
                                    [1000, 2000],
                                    [1000, 1000],
                                ]
                            ],
                        },
                    }
                ],
            },
            "transportation": {
                "extent": 4096,
                "features": [
                    {
                        "id": 3,
                        "properties": {"class": "primary"},
                        "geometry": {
                            "type": "LineString",
                            "coordinates": [[2048, 0], [2048, 4096]],
                        },
                    }
                ],
            },
        },
    }


class UrbanPatchTest(unittest.TestCase):
    def test_exact_mask_depth_identity_and_normals_against_boxes(self):
        product = example()
        rng = np.random.default_rng(1105)
        origins = rng.uniform(-80, 120, (4096, 3))
        directions = rng.uniform(0, 48, (4096, 3)) - origins
        before, after = (
            trace(product, origins, directions, mode=mode)
            for mode in ["boxes", "masks"]
        )
        for a, b in zip(before[:4], after[:4], strict=True):
            np.testing.assert_array_equal(a, b)

    def test_holes_parallel_edges_floor_and_stale_previous_hit(self):
        product = example()
        origins = np.array(
            [
                [8, 8, 40],
                [24, 8, 40],
                [-10, 8, 3],
                [8, 8, -5],
                [-10, 2, 6],
                [-10, 8, 3],
            ],
            float,
        )
        directions = np.array(
            [[0, 0, -1], [0, 0, -1], [1, 0, 0], [0, 0, 1], [1, 0, 0], [-1, 0, 0]], float
        )
        for mode in ["boxes", "masks"]:
            depth, owner, face, _, _ = trace(product, origins, directions, mode=mode)
            np.testing.assert_array_equal(depth, [34, np.inf, 12, 5, 12, np.inf])
            np.testing.assert_array_equal(owner, [0, -1, 0, 0, 0, -1])
            np.testing.assert_array_equal(face, [4, -1, 0, 5, 0, -1])

    def test_opaque_mesh_control_independently_matches_primary_depths(self):
        sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "render"))
        from native_capture_meshes import independent_depth

        product = example()
        mesh = build(product, [0], "boxes")
        triangles = mesh["positions"][mesh["quads"][:, [[0, 1, 2], [0, 2, 3]]]].reshape(
            -1, 3, 3
        )
        rng = np.random.default_rng(308)
        origins = rng.uniform(-70, 90, (128, 3))
        directions = rng.uniform(0, 48, (128, 3)) - origins
        expected = independent_depth(triangles, origins, directions)
        actual = trace(product, origins, directions)[0]
        np.testing.assert_array_equal(np.isfinite(actual), np.isfinite(expected))
        good = np.isfinite(expected)
        np.testing.assert_allclose(actual[good], expected[good], atol=1e-10, rtol=1e-10)

    def test_source_building_absence_is_inferred_not_empty_and_water_stays_free(self):
        document = source_document()
        a, metadata = bake(document)
        b, _ = bake(copy.deepcopy(document))
        for key, array in a.arrays().items():
            np.testing.assert_array_equal(array, b.arrays()[key])
        self.assertGreater(metadata["occupied_lots"], 0)
        self.assertEqual(metadata["status"], "inferred_partial_urban")
        self.assertFalse(metadata["building_layer_present"])
        data = domains(document)
        for row, x_y in enumerate(a.chunks):
            for y, x in np.argwhere(a.tiers[row] > 0):
                centre = (x_y * a.side + [x, y] + 0.5) * a.frame[3]
                half = a.frame[3] * a.frame[4] / 2
                self.assertTrue(
                    shapely.covers(
                        data["allowed"], shapely.box(*(centre - half), *(centre + half))
                    )
                )

    def test_no_urban_evidence_is_unknown_and_avoids_cell_geometry(self):
        document = source_document()
        document["layers"]["landuse"]["features"][0]["properties"]["class"] = (
            "unclassified"
        )
        product, metadata = bake(document)
        self.assertEqual(len(product.chunks), 0)
        self.assertEqual(metadata["status"], "no_urban_evidence_not_world_empty")
        self.assertEqual(metadata["footprint_geometries_tested"], 0)

    def test_fresh_ready_reader_and_query_do_not_import_source_or_geos(self):
        with tempfile.TemporaryDirectory(prefix="outshine-urban-patches-") as temporary:
            path = Path(temporary) / "ready.npz"
            np.savez_compressed(path, **example().arrays())
            source = """import importlib.abc, sys, numpy as np
class Block(importlib.abc.MetaPathFinder):
 def find_spec(self, fullname, path, target=None):
  if fullname.split('.')[0] in {'shapely', 'mapbox_vector_tile', 'coarse_city_domains', 'urban_patch_bake'}:
   raise RuntimeError('forbidden ready dependency '+fullname)
sys.meta_path.insert(0, Block())
from urban_patches import load
from urban_patch_trace import trace
p=load(sys.argv[1]);r=trace(p,[[8,8,40]],[[0,0,-1]])
assert r[0][0]==34 and r[1][0]==0
"""
            result = subprocess.run(
                [sys.executable, "-B", "-W", "error", "-c", source, str(path)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(load(path).tiers.shape, (1, 3, 3))


if __name__ == "__main__":
    unittest.main()
