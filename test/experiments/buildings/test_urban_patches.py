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
from urban_patch_bake import bake, hash_cells
from urban_patch_mesh import build, height_atlas
from urban_patch_projection import projected_work
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
    def test_ready_reader_rejects_signed_fractional_and_out_of_range_tiers(self):
        original = example().arrays()
        variants = [
            original["tiers"].astype(np.int8),
            original["tiers"].astype(np.float64),
            original["tiers"].astype(np.uint16),
            original["tiers"].astype(bool),
            original["tiers"].copy(),
        ]
        variants[0][0, 0, 0] = -1
        variants[1][0, 0, 0] = 0.5
        variants[-1][0, 0, 0] = len(original["heights"])
        with tempfile.TemporaryDirectory(prefix="outshine-urban-reader-") as folder:
            path = Path(folder) / "invalid.npz"
            for tier in variants:
                with self.subTest(dtype=tier.dtype):
                    np.savez_compressed(path, **(original | {"tiers": tier}))
                    with self.assertRaises(ValueError):
                        load(path)

    def test_ready_reader_checks_schema_shapes_before_indexing(self):
        original = example().arrays()
        replacements = [
            {"chunks": np.array(0, np.int32)},
            {"tiers": np.array(1, np.uint8)},
            {"heights": np.empty(0, np.float64)},
            {"frame": np.array(1, np.float64)},
            {"bounds": np.zeros((1, 3, 2), np.float32)},
            {"source_offsets": np.array(0, np.uint32)},
            {"source_indices": np.array([[0]], np.uint32)},
            {"chunks": original["chunks"].astype(np.float64)},
            {"source_indices": original["source_indices"].astype(np.int32)},
            {"extra": np.zeros(1, np.uint8)},
        ]
        with tempfile.TemporaryDirectory(prefix="outshine-urban-reader-") as folder:
            path = Path(folder) / "invalid.npz"
            for replacement in replacements:
                with self.subTest(fields=list(replacement)):
                    np.savez_compressed(path, **(original | replacement))
                    with self.assertRaises(ValueError):
                        load(path)
            np.savez_compressed(
                path, **{k: v for k, v in original.items() if k != "tiers"}
            )
            with self.assertRaises(ValueError):
                load(path)

    def test_ready_reader_checks_finite_bounds_and_monotonic_source_offsets(self):
        original = example().arrays()
        nonfinite = original["bounds"].copy()
        nonfinite[0, 1, 0] = np.inf
        reversed_bounds = original["bounds"].copy()
        reversed_bounds[0, 0, 0] = 49
        two = {k: v.copy() for k, v in original.items()}
        two["chunks"] = np.array([[0, 0], [1, 0]], np.int32)
        two["tiers"] = np.repeat(two["tiers"], 2, axis=0)
        two["bounds"] = np.repeat(two["bounds"], 2, axis=0)
        two["source_offsets"] = np.array([0, 2, 1], np.uint32)
        variants = [
            original | {"bounds": nonfinite},
            original | {"bounds": reversed_bounds},
            original | {"source_offsets": np.array([1, 1], np.uint32)},
            original | {"source_offsets": np.array([0, 2], np.uint32)},
            two,
        ]
        with tempfile.TemporaryDirectory(prefix="outshine-urban-reader-") as folder:
            path = Path(folder) / "invalid.npz"
            for index, arrays in enumerate(variants):
                with self.subTest(index=index):
                    np.savez_compressed(path, **arrays)
                    with self.assertRaises(ValueError):
                        load(path)

    def test_chunk_acceptance_matches_individual_geos_footprint_predicates(self):
        document = source_document()
        product, metadata = bake(document)
        data = domains(document)
        left, bottom, right, top = data["tile_bounds"]
        pitch = 32.0
        lower = np.ceil(np.array([left, bottom]) / pitch - 0.5).astype(np.int64)
        upper = np.ceil(np.array([right, top]) / pitch - 0.5).astype(np.int64)
        expected = set()
        for y in range(lower[1], upper[1]):
            for x in range(lower[0], upper[0]):
                if hash_cells(np.array([x]), np.array([y]), 1105)[0] % 100 < 15:
                    continue
                centre = (np.array([x, y]) + 0.5) * pitch
                if shapely.covers(
                    data["allowed"], shapely.box(*(centre - 12), *(centre + 12))
                ):
                    expected.add((x, y))
        actual = {
            (int(key[0] * product.side + x), int(key[1] * product.side + y))
            for row, key in enumerate(product.chunks)
            for y, x in np.argwhere(product.tiers[row] > 0)
        }
        self.assertEqual(actual, expected)
        self.assertGreater(metadata["lots_accepted_without_footprint_geometry"], 0)

    def test_projected_work_counts_front_geometry_and_clips_viewport(self):
        mesh = {
            "positions": np.array(
                [[-0.5, -0.5, 1], [-0.5, 0.5, 1], [0.5, 0.5, 1], [0.5, -0.5, 1]]
            ),
            "quads": np.array([[0, 1, 2, 3]]),
        }
        camera = {
            "eye": [0, 0, 0],
            "camera_basis": np.eye(3),
            "width": 200,
            "height": 200,
            "focal_px": 100,
        }
        report = projected_work(mesh, camera)
        self.assertAlmostEqual(report["projected_geometric_pixel_area"], 10000)
        mesh["positions"][:, :2] *= 4
        self.assertAlmostEqual(
            projected_work(mesh, camera)["projected_geometric_pixel_area"], 40000
        )
        mesh["quads"] = mesh["quads"][:, ::-1]
        self.assertEqual(
            projected_work(mesh, camera)["projected_geometric_pixel_area"], 0
        )

    def test_parent_urban_evidence_fills_unknown_without_removing_child_exclusions(
        self,
    ):
        child = source_document()
        before, _ = bake(child)
        parent = copy.deepcopy(child)
        parent["tile"] = [13, 4096, 4096]
        parent["source"]["sha256"] = "synthetic-parent"
        parent["layers"] = {
            "landuse": parent["layers"]["landuse"],
            "transportation": parent["layers"]["transportation"],
        }
        child["layers"]["landuse"]["features"][0]["properties"]["class"] = (
            "unclassified"
        )
        child["parents"] = [parent]
        after, metadata = bake(child)
        self.assertGreater((after.tiers > 0).sum(), 0)
        self.assertEqual(
            metadata["urban_area_projected_m2"], metadata["owner_area_projected_m2"]
        )
        self.assertTrue(
            any(
                row["role"] == "superseded_by_finer_transport_evidence"
                for row in metadata["source_records"]
            )
        )
        rows = {tuple(key): i for i, key in enumerate(after.chunks)}
        for i, key in enumerate(before.chunks):
            occupied = before.tiers[i] > 0
            np.testing.assert_array_equal(
                before.tiers[i][occupied], after.tiers[rows[tuple(key)]][occupied]
            )
        data = domains(child)
        left, _bottom, right, top = data["tile_bounds"]
        step = (right - left) / 4096
        self.assertFalse(
            shapely.covers(
                data["allowed"], shapely.Point(left + 1500 * step, top - 1500 * step)
            )
        )
        self.assertLess(
            data["allowed_area_projected_m2"], data["owner_area_projected_m2"]
        )

    def test_one_atlas_preserves_every_selected_patch_without_filtering_tiers(self):
        product = example()
        product.chunks = np.array([[0, 0], [1, 0]], np.int32)
        product.tiers = np.concatenate((product.tiers, np.flip(product.tiers, axis=1)))
        product.bounds = np.repeat(product.bounds, 2, axis=0)
        product.bounds[1, :, 0] += 48
        product.source_offsets = np.array([0, 1, 2], np.uint32)
        product.source_indices = np.array([0, 0], np.uint32)
        selected = [1, 0, 1]
        image, columns, rows = height_atlas(product, selected)
        self.assertEqual((columns, rows), (2, 2))
        for i in range(3):
            wanted = product.heights[product.tiers[selected[i]]].astype(np.float32)
            x, y = i % columns * product.side, i // columns * product.side
            np.testing.assert_array_equal(
                image[y : y + product.side, x : x + product.side, 0], wanted
            )
        self.assertTrue((image[product.side :, product.side :, :3] == 0).all())

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
        with tempfile.TemporaryDirectory(
            prefix="outshine-urban-reader-empty-"
        ) as folder:
            path = Path(folder) / "empty.npz"
            np.savez_compressed(path, **product.arrays())
            self.assertEqual(load(path).tiers.shape, (0, product.side, product.side))

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
