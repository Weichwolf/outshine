"""Independent closed-box rays, roof controls, holes and source membership."""

import json
import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from column_field_bake import merge_columns, patch_columns, store_native_fallback
from column_field_query import load_product, trace, walk_cells
from column_roof_patches import project_patch, roof_patches
from native_capture_meshes import independent_depth
from parent_surface_inputs import open_parent
from prepared_surface_recipes import Reader


def product_for(rows, output):
    arrays = merge_columns(rows, 4)
    arrays["basis"] = np.eye(3)
    output.mkdir()
    np.savez_compressed(output / "field.npz", **arrays)
    (output / "header.json").write_text(
        json.dumps(
            {
                "schema": "shared-column-geometry-1",
                "pitch_m": 1.0,
                "arrays": {k: a.nbytes for k, a in arrays.items()},
                "stored_array_bytes": sum(a.nbytes for a in arrays.values()),
            }
        )
    )
    return load_product(output)


def explicit_boxes(product):
    triangles = []
    faces = np.array(
        [
            [0, 2, 3],
            [0, 3, 1],
            [4, 5, 7],
            [4, 7, 6],
            [0, 1, 5],
            [0, 5, 4],
            [2, 6, 7],
            [2, 7, 3],
            [0, 4, 6],
            [0, 6, 2],
            [1, 3, 7],
            [1, 7, 5],
        ]
    )
    for i, (x, y) in enumerate(product["columns"]):
        for z0, z1 in product["spans"][
            product["offsets"][i] : product["offsets"][i + 1]
        ]:
            corners = np.array(
                [[a, b, c] for a in (x, x + 1) for b in (y, y + 1) for c in (z0, z1)]
            )
            triangles.extend(corners[faces[:, [0, 2, 1]]])
    return np.array(triangles)


class ColumnTests(unittest.TestCase):
    def test_parallel_inset_gap_skips_grid_without_losing_boundaries(self):
        product = {
            "low": np.array([0, 0]),
            "shape": np.array([4096, 4096]),
            "height_bounds": np.array([0.0, 48.0]),
            "header": {"pitch_m": 16.0, "footprint_fraction": 0.75},
        }
        for y in [
            1.0,
            15.0,
            2.0,
            14.0,
            np.nextafter(2.0, 0.0),
            np.nextafter(14.0, 16.0),
        ]:
            work = {"steps": 0, "cell_ray_visits": 0}
            result = next(
                walk_cells(
                    product,
                    np.array([[-8.0, y, 2.0]]),
                    np.array([[1.0, 0.0, 0.0]]),
                    work,
                ),
                None,
            )
            if y in [1.0, 15.0]:
                self.assertIsNone(result)
                self.assertEqual(work["cell_ray_visits"], 0)
            else:
                # Exact/adjacent-ULP surface boundaries remain conservative.
                self.assertIsNotNone(result)

    def test_ready_native_fallback_is_queried_before_field(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "field"
            product = product_for({(0, 0): [(0, 1, 0, 0)]}, path)
            source = {
                "positions": np.array(
                    [[-1, -1, 4], [3, -1, 4], [-1, 3, 4]], np.float32
                ),
                "indices": np.array([[0, 1, 2]]),
                "vertex_normal": np.array([[0, 0, 1]] * 3),
                "binding": np.array([[0, 0, 0, 0, 0, 0]]),
            }
            header = product["header"] | {"local_original_needs": [{"source": 0}]}
            store_native_fallback(path, source, header)
            ready = load_product(path)
            depth, normal, owner, _ = trace(ready, [[0.5, 0.5, 5]], [[0, 0, -1]])
            self.assertEqual(depth[0], 1)
            self.assertEqual(owner[0], -2)
            np.testing.assert_array_equal(normal[0], [0, 0, 1])
            empty = ready | {
                "shape": np.zeros(2, np.int64),
                "lookup": np.empty((0, 0), np.int32),
            }
            depth, normal, owner, work = trace(empty, [[0.5, 0.5, 5]], [[0, 0, -1]])
            self.assertEqual(work["cell_ray_visits"], 0)
            self.assertEqual(depth[0], 1)
            self.assertEqual(owner[0], -2)
            np.testing.assert_array_equal(normal[0], [0, 0, 1])

    def test_overlap_union_retains_gap_and_anonymous_sources(self):
        a = merge_columns({(0, 0): [(0, 2, 0, 0), (1, 3, 1, 0), (5, 6, 2, 1)]}, 3)
        np.testing.assert_allclose(a["spans"], [[0, 3], [5, 6]], atol=1e-6)
        np.testing.assert_array_equal(a["lineage_offsets"], [0, 2, 3])
        np.testing.assert_array_equal(a["lineage"], [[0, 0], [1, 0], [2, 1]])

    def test_hole_and_piecewise_roof_maximum(self):
        polygon = shapely.Polygon(
            [(0, 0), (3, 0), (3, 3), (0, 3)], [[(1, 1), (2, 1), (2, 2), (1, 2)]]
        )
        xy, spans, tests = patch_columns(polygon, np.array([2, 3, 4]), -1, 1)
        self.assertEqual(tests, 9)
        self.assertFalse(np.any(np.all(xy == [1, 1], axis=1)))
        for cell, span in zip(xy, spans, strict=True):
            self.assertAlmostEqual(span[1], 2 * (cell[0] + 1) + 3 * (cell[1] + 1) + 4)

    def test_dda_against_explicit_triangle_boxes(self):
        rows = {
            (0, 0): [(0, 1, 0, 0), (2, 3, 1, 0)],
            (1, 0): [(0, 2, 2, 0)],
            (2, 2): [(1, 2, 3, 0)],
        }
        with tempfile.TemporaryDirectory() as tmp:
            product = product_for(rows, Path(tmp) / "field")
            rng = np.random.default_rng(99)
            origins = rng.uniform(-4, 7, (5000, 3))
            # Origins outside all columns; retain rotations, negative and parallel rays.
            origins[:, 2] = rng.choice([-2, 5], len(origins))
            targets = rng.uniform(-1, 4, origins.shape)
            directions = targets - origins
            directions[:100, 0] = 0
            directions[100:200, 1] = 0
            expected = independent_depth(explicit_boxes(product), origins, directions)
            actual, normal, _, _ = trace(product, origins, directions)
            np.testing.assert_array_equal(np.isfinite(actual), np.isfinite(expected))
            hit = np.isfinite(actual)
            np.testing.assert_allclose(
                actual[hit], expected[hit], atol=1e-10, rtol=1e-10
            )
            np.testing.assert_allclose(np.linalg.norm(normal[hit], axis=1), 1)

    def test_vertical_gap_empty_grid_and_inside_need(self):
        with tempfile.TemporaryDirectory() as tmp:
            product = product_for(
                {(0, 0): [(0, 1, 0, 0), (2, 3, 1, 0)], (2, 0): [(0, 1, 2, 0)]},
                Path(tmp) / "field",
            )
            origins = np.array([[0.5, -1, 1.5], [0.5, 0.5, 4], [1.5, 0.5, 4]])
            directions = np.array([[0, 1, 0], [0, 0, -1], [0, 0, -1]])
            depth, *_ = trace(product, origins, directions)
            self.assertFalse(np.isfinite(depth[0]))
            self.assertAlmostEqual(depth[1], 1, places=6)
            self.assertFalse(np.isfinite(depth[2]))
            with self.assertRaisesRegex(ValueError, "inside occupied"):
                trace(product, [[0.5, 0.5, 0.5]], [[1, 0, 0]])

    def test_affine_source_projection(self):
        angle = 0.001
        surface = {
            "origin": [10, 20, 30],
            "east": [np.cos(angle), 0, -np.sin(angle)],
            "north": [0, 1, 0],
            "up": [np.sin(angle), 0, np.cos(angle)],
        }
        polygon = shapely.box(-2, -1, 2, 1)
        plane = np.array([0.2, 0.3, 8])
        footprint, roof, bottom, shift = project_patch(
            surface, polygon, plane, np.eye(3), 2
        )
        xy = shapely.get_coordinates(polygon)
        xyz = (
            np.column_stack((xy, xy @ plane[:2] + plane[2]))
            @ np.array([surface[k] for k in ("east", "north", "up")])
            + surface["origin"]
        )
        np.testing.assert_allclose(
            xyz[:, 2], xyz[:, :2] @ roof[:2] + roof[2], atol=1e-12
        )
        np.testing.assert_allclose(
            shapely.get_coordinates(footprint), xyz[:, :2], atol=1e-12
        )
        self.assertLess(bottom, 33)
        self.assertGreater(shift, 0)

    def test_native_roof_controls(self):
        shape = {
            "ring": [[-3, -2], [3, -2], [3, 2], [-3, 2]],
            "holes": [],
            "rise_m": 2,
            "seat_m": 1,
            "foot_m": 0,
            "eaves_m": 5,
            "centre": [0, 0],
            "axis_u": [1, 0],
            "half_v_m": 2,
            "break_fraction_v": 0.5,
            "break_rise_m": 1,
            "half_u_m": 3,
            "period_m": 2,
        }
        for kind, point, expected in [
            (0, [0, 0], 8),
            (1, [0, 0], 8),
            (1, [0, 1], 7),
            (3, [0, -1], 6.5),
            (4, [0, 1.5], 6.5),
            (5, [-1.3, 0], 8),
        ]:
            patches = roof_patches(shape | {"roof_kind": kind})
            heights = [
                np.r_[point, 1] @ plane
                for p, plane in patches
                if shapely.intersects(p, shapely.Point(point))
            ]
            self.assertTrue(heights)
            np.testing.assert_allclose(heights, expected, atol=1e-12)
        with self.assertRaisesRegex(ValueError, "dome"):
            roof_patches(shape | {"roof_kind": 6})

    def test_reader_budget_and_truncation(self):
        for budget in (0, 16777217, True):
            with self.assertRaises(ValueError):
                Reader(b"abcd", number_limit=budget)
        reader = Reader(b"abcd", number_limit=1)
        reader.number("I")
        with self.assertRaises(ValueError):
            reader.number("B")
        with self.assertRaises(ValueError):
            Reader(b"abcd").skip(5)

    def test_actual_parent_partition_and_recipe_association(self):
        path = (
            Path(__file__).resolve().parents[2]
            / "outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        with open_parent(path) as parent:
            self.assertEqual(len(parent.cells), 13218)
            self.assertEqual(len(parent.forms), 64)
            self.assertTrue(
                all(r["source_id"] == (0, 0) for r in parent.plans["records"])
            )
            cell, ordinals, surfaces = next(parent.iter_forms([33]))
            self.assertEqual(cell, 33)
            self.assertEqual(len(ordinals), 576)
            np.testing.assert_array_equal(ordinals, np.flatnonzero(parent.cells == 33))
            altered = dict(surfaces[0], origin=[0, 0, 0])
            with self.assertRaisesRegex(ValueError, "association differs"):
                parent.check_surface(int(ordinals[0]), altered)


if __name__ == "__main__":
    unittest.main()
