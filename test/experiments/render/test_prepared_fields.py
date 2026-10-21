"""Known coordinate cases, bounded wire failures and complete actual source binding."""

import copy
import struct
import tarfile
import unittest
from pathlib import Path

import numpy as np
from compare_prepared_fields import codes, evaluate, statistics
from full_view_reference import load
from prepared_field_association import associate, with_geometry_plans
from prepared_material_fields import at_local, at_world, facade_catalog
from prepared_surface_recipes import Reader, bind, decode, load_recipes, read_hip


def rectangle():
    return {
        "ring": [[0, 0], [8, 0], [8, 4], [0, 4]],
        "holes": [],
        "party_wall_edges": [0, 0, 0, 0],
        "front_edge": 0,
        "roof_kind": 0,
        "ident": 22,
        "opening_style": 1,
        "wall_variant": 6,
        "seat_m": 10,
        "foot_m": 2,
        "floor_m": 4,
        "bay_m": 3.2,
    }


class CoordinateTest(unittest.TestCase):
    def test_front_wall_round_half_away_and_material_metres(self):
        # 8/3.2=2.5 bays rounds to THREE, unlike Python's ties-to-even round.
        uv, metres = at_local(rectangle(), 2, [[4, 0, 16]], gable=False)
        np.testing.assert_array_equal(uv, [[256 * 221 + 1.5, 2]])
        np.testing.assert_array_equal(metres, [[4, 4]])

    def test_explicit_hit_context_changes_same_corner(self):
        p = [[4, 0, 16]]
        a, _ = at_local(rectangle(), 2, p, gable=False)
        b, _ = at_local(rectangle(), 2, p, gable=True)
        self.assertNotEqual(a[0, 0], b[0, 0])
        self.assertEqual(b[0, 0], 256 * 199)
        with self.assertRaisesRegex(ValueError, "explicit gable"):
            at_local(rectangle(), 2, p)

    def test_party_hole_roof_bottom_and_glazing(self):
        shape = rectangle()
        shape["party_wall_edges"][0] = 1
        uv, _ = at_local(shape, 2, [[4, 0, 16]], gable=False)
        self.assertEqual(uv[0, 0], 256 * 221)
        shape["holes"] = [[[1, 1], [3, 1], [3, 3], [1, 3]]]
        uv, _ = at_local(shape, 6, [[2, 1, 16]], gable=False)
        self.assertEqual(uv[0, 0], 256 * 199 + 0.5)
        uv, metres = at_local(shape, 0, [[0, 0, 16]])
        np.testing.assert_array_equal(uv, [[-361, 16]])
        np.testing.assert_array_equal(metres, [[0, 0]])
        uv, _ = at_local(shape, 1, [[0, 0, 16]])
        self.assertEqual(uv[0, 0], -355)
        shape["roof_kind"] = 1
        self.assertEqual(at_local(shape, 1, [[0, 0, 16]])[0][0, 0], -354)
        shape["opening_style"] = facade_catalog()["Glazing"]
        np.testing.assert_array_equal(
            at_local(shape, 2, [[4, 0, 16]], gable=False)[1], [[0, 0]]
        )

    def test_world_frame_and_invalid_points(self):
        surface = {
            "origin": [10, 20, 30],
            "east": [0, 1, 0],
            "north": [-1, 0, 0],
            "up": [0, 0, 1],
            "shapes": [rectangle()],
        }
        a = at_world(surface, 0, 2, [[10, 24, 46]], gable=False)
        b = at_local(rectangle(), 2, [[4, 0, 16]], gable=False)
        for x, y in zip(a, b, strict=True):
            np.testing.assert_array_equal(x, y)
        with self.assertRaises(ValueError):
            at_local(rectangle(), 2, [[0, np.nan, 0]], gable=False)
        with self.assertRaises(ValueError):
            at_local(rectangle(), 99, [[0, 0, 0]], gable=False)

    def test_submillimetre_shift_can_change_material_code(self):
        shape = rectangle() | {"opening_style": 1, "wall_variant": 0, "front_edge": -1}
        uv, _ = at_local(shape, 2, [[0, 0, 16], [-0.0002, 0, 16]], gable=False)
        self.assertLess(abs(float(uv[1, 0]) - float(uv[0, 0])), 0.001)
        np.testing.assert_array_equal(codes(uv), [1, 0])
        report = statistics(
            uv[:1], np.zeros((1, 2)), uv[1:], np.zeros((1, 2)), np.array([True])
        )
        self.assertEqual(report["categorical_code_mismatches"], 1)


class WireTest(unittest.TestCase):
    def test_empty_legacy_truncated_count_trailing_and_nonfinite(self):
        empty = struct.pack("<IQ", 0x33435342, 0)
        self.assertEqual(decode(empty)[0], [])
        for raw in (
            b"",
            empty[:6],
            empty + b"x",
            struct.pack("<IQ", 0x31435342, 0),
            struct.pack("<IQ", 0x33435342, 2**64 - 1),
        ):
            with self.assertRaises(ValueError):
                decode(raw)
        with self.assertRaises(ValueError):
            Reader(struct.pack("<d", float("nan"))).number("d")
        with self.assertRaises(ValueError):
            Reader(b"\x02").flag()
        with self.assertRaises(ValueError):
            read_hip(Reader(b"\x03"))
        with self.assertRaises(ValueError):
            read_hip(Reader(struct.pack("<Bi", 2, 5)))


class OriginalRecipesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = Path("test/outshine/data/render/building-full-view/cell.json.gz")
        cls.source, cls.document, _, _ = load(cls.path)
        cls.recipes, cls.cost = load_recipes(cls.path, cls.document)

    def test_complete_original_fields_and_provenance(self):
        self.assertEqual(len(self.recipes), 576)
        self.assertEqual(sum(len(s["shapes"]) for s in self.recipes.values()), 618)
        self.assertEqual(self.cost["wire_bytes"], 346222)
        self.assertFalse(self.cost["package_crc_checked"])
        shape = self.recipes[2256]["shapes"][0]
        self.assertEqual(
            (
                shape["seed"],
                shape["ident"],
                shape["wall_variant"],
                shape["front_edge"],
                shape["opening_style"],
            ),
            (2581382284, 22, 6, 2, 1),
        )
        self.assertEqual((shape["floor_m"], shape["bay_m"]), (2.75, 3.2))
        with tarfile.open(self.path.parent / "original-inputs.tar.gz") as archive:
            raw = archive.extractfile(
                self.document["parent"] + "/" + self.cost["block"]
            ).read()
        for shortened in (raw[:13], raw[:-1]):
            with self.assertRaises(ValueError):
                decode(shortened)

    def test_source_permutation_and_geometry_mismatch_rejected(self):
        document = copy.deepcopy(self.document)
        document["sources"][0], document["sources"][1] = (
            document["sources"][1],
            document["sources"][0],
        )
        with self.assertRaises(ValueError):
            bind(list(self.recipes.values()), document)
        document = copy.deepcopy(self.document)
        document["sources"][0]["surface"]["origin"][0] += 1
        with self.assertRaises(ValueError):
            bind(list(self.recipes.values()), document)

    def test_actual_association_keeps_unknown_bindings_and_boundary_failure(self):
        binding = self.source["binding"].copy()
        labels, reasons, _ = associate(
            self.source, with_geometry_plans(self.recipes, self.document)
        )
        np.testing.assert_array_equal(binding, self.source["binding"])
        self.assertTrue((binding[:, 2:4] == -1).all())
        self.assertTrue((labels[reasons > 0] == -1).all())
        ids = np.repeat(np.arange(len(labels)), 3)
        triangles = self.source["indices"]
        uv, metres, known = evaluate(
            self.source,
            self.recipes,
            labels,
            ids,
            self.source["positions"][triangles].reshape(-1, 3),
            facade_catalog(),
            triangle_context=True,
        )
        self.assertTrue(known.any())
        self.assertTrue((~known).any())
        self.assertTrue(np.isnan(uv[~known]).all())
        native = self.source["uv"][triangles].reshape(-1, 2)
        bad = codes(uv[known]) != codes(native[known])
        self.assertTrue(
            bad.any()
        )  # Preserve the actual failure; small UV error is not code parity.
        self.assertLess(np.max(np.abs(uv[known, 0] - native[known, 0])), 0.01)
        self.assertTrue(np.isfinite(metres[known]).all())


if __name__ == "__main__":
    unittest.main()
