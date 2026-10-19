"""Stable categorical inputs, known analytic coordinates and full original fallback."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
from attributed_prisms import (
    compile_product,
    load_product,
    material_fields,
    material_tables,
    native_fields,
    native_inputs,
    render_product,
)
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views
from prepared_surface_recipes import load_recipes
from roof_prisms import Prisms
from triangle_image_reference import image_rays, intersect_source, prepare_source


def example():
    shape = {
        "ring": [[0, 0], [8, 0], [8, 4], [0, 4]],
        "party_wall_edges": [0, 0, 0, 0],
        "front_edge": 0,
        "roof_kind": 0,
        "ident": 22,
        "opening_style": 1,
        "wall_variant": 6,
        "seat_m": 10,
        "foot_m": 2,
        "eaves_m": 4,
        "rise_m": 0,
        "floor_m": 4,
        "bay_m": 3.2,
    }
    surface = {
        "origin": [100, 200, 300],
        "east": [1, 0, 0],
        "north": [0, 1, 0],
        "up": [0, 0, 1],
        "wall_colour": None,
        "shapes": [shape],
    }
    planes = np.array(
        [
            [0, 0, -1, -312],
            [0, -1, 0, -200],
            [1, 0, 0, 108],
            [0, 1, 0, 204],
            [-1, 0, 0, -100],
            [0, 0, 1, 316],
        ],
        float,
    )
    prism = Prisms(
        planes,
        np.array([0, 6], np.uint32),
        np.array([[11, 0]], np.int32),
        np.array([[[100, 200, 312], [108, 204, 316]]]),
        np.array([False] * 5 + [True]),
        np.empty(0, np.uint32),
    )
    return prism, {11: surface}


class AnalyticTest(unittest.TestCase):
    def test_known_frame_bays_metres_roof_and_base(self):
        prism, recipes = example()
        material = material_tables(prism, recipes)
        v = material_fields(
            material,
            prism,
            np.array([0, 0, 0]),
            np.array([1, 5, 0]),
            np.array([[104, 200, 316], [104, 202, 316], [104, 202, 312]], float),
        )
        np.testing.assert_array_equal(v["code"], [221, -355, -361])
        np.testing.assert_array_equal(
            v["coordinates"], [[1.5, 2], [157, 16], [151, 12]]
        )
        np.testing.assert_array_equal(v["metres"], [[4, 4], [0, 0], [0, 0]])
        np.testing.assert_array_equal(v["normal"], [[0, -1, 0], [0, 0, 1], [0, 0, -1]])

    def test_material_identity_survives_negative_bay_boundary(self):
        prism, recipes = example()
        recipes[11]["shapes"][0].update(wall_variant=0, front_edge=-1)
        v = material_fields(
            material_tables(prism, recipes),
            prism,
            np.array([0, 0]),
            np.array([1, 1]),
            np.array([[100, 200, 316], [99.9998, 200, 316]]),
        )
        np.testing.assert_array_equal(v["code"], [1, 1])
        self.assertLess(v["coordinates"][1, 0], 0)
        packed = np.float32(256) * v["code"].astype(np.float32) + v["coordinates"][:, 0]
        self.assertEqual(int(np.floor(packed[1] / 256)), 0)

    def test_nonwhite_rgba_fallback_is_not_whitened_or_truncated(self):
        from packed_normals import unpack

        source = {
            "positions": np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1]], np.float32),
            "indices": np.array([[0, 2, 1]], np.uint32),
            "uv": np.array([[256, 1], [258, 3], [260, 5]], np.float32),
            "normal": np.zeros(3, np.uint32),
            "metres": np.array([[1, 2], [3, 4], [5, 6]], np.float32),
            "colour": np.array(
                [[0.2, 0.3, 0.4, 0.5], [0.6, 0.7, 0.8, 0.9], [1, 0.8, 0.6, 0.4]],
                np.float32,
            ),
            "binding": np.array([[0, 7, -1, -1, 0, 0]], np.int32),
        }
        source["vertex_normal"] = unpack(source["normal"])
        arrays, _ = compile_product(
            {"sources": [{"index": 7, "surface": None}]}, source, {7: None}
        )
        np.testing.assert_array_equal(arrays["fallback_colour"], source["colour"])
        with tempfile.TemporaryDirectory(prefix="outshine-attributed-test-") as tmp:
            path = Path(tmp) / "product.npz"
            np.savez_compressed(path, **arrays)
            origin, direction = np.array([[0.25, 0.25, 0]]), np.array([[0, 0, 1.0]])
            actual = render_product(load_product(path), origin, direction)
            expected = native_fields(
                native_inputs(source), origin, direction, np.array([1.0]), np.array([0])
            )
            self.assertEqual(actual[1]["colour"].shape, (1, 4))
            np.testing.assert_array_equal(actual[1]["colour"], expected["colour"])
            np.testing.assert_allclose(
                actual[1]["colour"], [[0.5, 0.525, 0.55, 0.575]], atol=1e-7
            )

    def test_gable_context_entrance_party_and_glazing(self):
        prism, recipes = example()
        material = material_tables(prism, recipes)
        v = material_fields(
            material,
            prism,
            np.array([0, 0]),
            np.array([1, 1]),
            np.array([[104, 200, 316], [104, 200, 317]]),
        )
        np.testing.assert_array_equal(v["code"], [221, 199])
        np.testing.assert_array_equal(v["coordinates"][:, 0], [1.5, 0])
        recipes[11]["shapes"][0]["party_wall_edges"][0] = 1
        self.assertEqual(material_tables(prism, recipes).bays[1], 0)
        recipes[11]["shapes"][0]["opening_style"] = 7
        self.assertFalse(material_tables(prism, recipes).metres[0])

    def test_invalid_material_hit_is_not_assigned_another_primitive(self):
        prism, recipes = example()
        material = material_tables(prism, recipes)
        with self.assertRaises(ValueError):
            material_fields(
                material,
                prism,
                np.array([0]),
                np.array([6]),
                np.array([[104, 200, 316.0]]),
            )
        with self.assertRaises(ValueError):
            material_fields(
                material,
                prism,
                np.array([0]),
                np.array([1]),
                np.array([[np.nan, 200, 316.0]]),
            )

    def test_native_vertex_category_then_interpolation(self):
        source = {
            "positions": np.array([[0, 0, 1], [1, 0, 1], [0, 1, 1]], np.float32),
            "indices": np.array([[0, 1, 2]]),
            "uv": np.array([[256, 1], [258, 3], [260, 5]], np.float32),
            "vertex_normal": np.tile([0.0, 0.0, 1.0], (3, 1)),
            "metres": np.zeros((3, 2)),
            "colour": np.ones((3, 4)),
        }
        inputs = native_inputs(source)
        result = native_fields(
            inputs,
            np.array([[0.25, 0.25, 0]]),
            np.array([[0, 0, 1.0]]),
            np.array([1.0]),
            np.array([0]),
        )
        np.testing.assert_array_equal(result["code"], [1])
        np.testing.assert_array_equal(result["coordinates"], [[1.5, 2.5]])
        source["uv"][1, 0] = 512
        with self.assertRaises(ValueError):
            native_inputs(source)


class CompleteOriginalTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = Path("test/outshine/data/render/building-full-view/cell.json.gz")
        cls.source, cls.document, cls.context, _ = load(cls.path)
        cls.recipes, _ = load_recipes(cls.path, cls.document)

    def test_complete_partition_roundtrip_and_no_recipe_dependency_at_query(self):
        arrays, reasons = compile_product(self.document, self.source, self.recipes)
        represented = set(map(int, arrays["prism_binding"][:, 0]))
        self.assertFalse(represented & set(reasons))
        self.assertEqual(represented | set(reasons), set(self.recipes))
        np.testing.assert_array_equal(
            arrays["fallback_binding"],
            self.source["binding"][arrays["prism_fallback_faces"]],
        )
        with tempfile.TemporaryDirectory(prefix="outshine-attributed-test-") as tmp:
            path = Path(tmp) / "product.npz"
            np.savez_compressed(path, **arrays)
            product = load_product(path)
            p = product[0]
            owner = np.arange(len(p.binding))
            plane = p.offsets[:-1]
            points = p.bounds.mean(1)
            # Query uses only serialized tables. No JSON/recipe is passed or retained.
            values = material_fields(product[1], p, owner, plane, points)
            self.assertTrue(np.isfinite(values["coordinates"]).all())
            np.testing.assert_array_equal(p.binding, arrays["prism_binding"])

    def test_all_original_fallback_keeps_every_field_exact(self):
        document = self.document | {
            "sources": [item | {"surface": None} for item in self.document["sources"]]
        }
        arrays, reasons = compile_product(
            document, self.source, {k: None for k in self.recipes}
        )
        self.assertEqual(len(reasons), 576)
        self.assertEqual(len(arrays["prism_binding"]), 0)
        with tempfile.TemporaryDirectory(prefix="outshine-attributed-test-") as tmp:
            path = Path(tmp) / "product.npz"
            np.savez_compressed(path, **arrays)
            product = load_product(path)
            view = next(
                iter(
                    select_full_poses(
                        self.source, views(self.document, self.context)
                    ).values()
                )
            )
            box = image_roi(self.source, view)
            w, h = (box[1] - box[0]).astype(int)
            o, d = image_rays(view, box, w, h, 2)
            o, d = o[::31], d[::31]
            depth, faces = intersect_source(
                self.source, o, d, prepared=prepare_source(self.source)
            )
            native = native_fields(native_inputs(self.source), o, d, depth, faces)
            result = render_product(product, o, d)
            np.testing.assert_array_equal(depth, result[0])
            for key in ("code", "coordinates", "metres", "normal", "colour"):
                np.testing.assert_array_equal(native[key], result[1][key])
            hit = faces >= 0
            np.testing.assert_array_equal(result[6][hit], faces[hit])

    def test_coloured_sources_are_preserved_by_original_fallback(self):
        recipes = dict(self.recipes)
        first = next(iter(recipes))
        recipes[first] = recipes[first] | {"wall_colour": [0.1, 0.2, 0.3]}
        arrays, reasons = compile_product(self.document, self.source, recipes)
        self.assertIn(first, reasons)
        self.assertNotIn(first, arrays["prism_binding"][:, 0])
        self.assertIn(first, arrays["fallback_binding"][:, 1])


if __name__ == "__main__":
    unittest.main()
