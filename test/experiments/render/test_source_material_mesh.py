"""Material seams, native cast/weld order and labelled corner correspondence."""

import unittest
from collections import defaultdict
from pathlib import Path

import numpy as np
import shapely
from build_source_material_mesh import source_patches
from compare_source_material_fields import coordinate_colour
from coupled_vertex_clusters import cluster_mesh
from parent_surface_inputs import open_parent
from prepared_material_fields import at_local
from source_material_mesh import carry_fields, material_mesh
from source_material_patches import (
    corner_fields,
    permute_corner_fields,
    round_away,
    shape_material_patches,
    weld_local,
)
from source_surface_patches import original_patches, shape_patches
from test_prepared_fields import rectangle


def prepared_shape():
    return rectangle() | {
        "roof_kind": 1,
        "eaves_m": 8,
        "rise_m": 3,
        "sole_m": 10,
        "centre": [4, 2],
        "axis_u": [1, 0],
        "half_u_m": 4,
        "half_v_m": 2,
    }


def surface():
    return {
        "origin": [0, 0, 0],
        "east": [1, 0, 0],
        "north": [0, 1, 0],
        "up": [0, 0, 1],
        "minimum_height_m": 10,
    }


def patches(shape=None):
    return shape_material_patches(
        surface(), shape or prepared_shape(), 42, 0, np.eye(3), np.zeros(3)
    )


class SourceMaterialTest(unittest.TestCase):
    def test_rounding_distinguishes_both_neighbors_of_half(self):
        x = np.array([np.nextafter(0.5, 0), 0.5, np.nextafter(0.5, 1)])
        np.testing.assert_array_equal(round_away(x), [0, 1, 1])
        np.testing.assert_array_equal(round_away(-x), [0, -1, -1])

    def test_deck_has_distinct_wall_and_gable_coordinates_at_same_point(self):
        pieces = [p for p in patches() if p.side == 3]
        wall = next(p for p in pieces if not p.gable)
        gable = next(p for p in pieces if p.gable)
        a = corner_fields(wall, [[1, 20]])
        b = corner_fields(gable, [[1, 20]])
        np.testing.assert_array_equal(a[1], b[1])
        self.assertNotEqual(a[0][0, 0], b[0][0, 0])
        self.assertEqual(float(b[0][0, 0] % 256), 0)
        self.assertEqual(a[0][0, 1], b[0][0, 1])

    def test_source_domain_union_and_courtyard_survive_semantic_split(self):
        shape = prepared_shape() | {"holes": [[[3, 1], [3, 3], [5, 3], [5, 1]]]}
        fields = patches(shape)
        original = shape_patches(surface(), shape, 42, np.eye(3), np.zeros(3))
        self.assertGreater(len(fields), len(original))
        for patch in original:
            candidates = [
                p.geometry.domain
                for p in fields
                if np.array_equal(p.geometry.origin, patch.origin)
                and np.array_equal(p.geometry.axes, patch.axes)
                and np.array_equal(p.geometry.normal, patch.normal)
            ]
            # Roof planes can have distinct domains in a common frame.
            covered = shapely.union_all(candidates)
            self.assertAlmostEqual(shapely.difference(patch.domain, covered).area, 0)
        roof = shapely.union_all([p.geometry.domain for p in fields if p.side == 1])
        self.assertFalse(shapely.covers(roof, shapely.Point(4, 2)))
        self.assertTrue(any(p.side >= 6 for p in fields))

    def test_flat_rise_belongs_to_wall_and_roof_uses_negative_code(self):
        pp = patches(prepared_shape() | {"roof_kind": 0})
        self.assertFalse(any(p.gable for p in pp))
        self.assertEqual(
            max(p.geometry.domain.bounds[3] for p in pp if p.side >= 2), 23
        )
        roof = next(p for p in pp if p.side == 1)
        uv, metres, code = corner_fields(roof, [[0, 0]])
        self.assertEqual(code, -355)
        np.testing.assert_array_equal(uv, [[-355, 23]])
        np.testing.assert_array_equal(metres, [[0, 0]])

    def test_weld_ties_away_from_zero_and_uv_precedes_weld(self):
        np.testing.assert_array_equal(
            weld_local([[-0.0005, 0.0005, 1.2345]]), [[-0.001, 0.001, 1.235]]
        )
        wall = next(p for p in patches() if p.side == 2 and not p.gable)
        uv, metres, _ = corner_fields(wall, [[0.0005, 16.0006]])
        self.assertEqual(uv[0, 0], np.float32(256 * 221) + np.float32(3 * 0.0005 / 8))
        self.assertEqual(uv[0, 1], np.float32(1) + np.float32(4.0006 / 4))
        np.testing.assert_array_equal(metres, np.array([[0.001, 4.001]], np.float32))

    def test_native_category_does_not_underflow_at_a_snapped_source_edge(self):
        shape = prepared_shape() | {
            "ring": [[0.0002, 0], [8.0002, 0], [8.0002, 4], [0.0002, 4]],
            "front_edge": -1,
            "wall_variant": 0,
        }
        wall = next(p for p in patches(shape) if p.side == 2 and not p.gable)
        uv, _, code = corner_fields(wall, [[0, 16]])
        naive = at_local(shape, 2, [[0, 0, 16]], gable=False)[0]
        self.assertEqual(code, 1)
        self.assertEqual(uv[0, 0], 256)
        self.assertEqual(np.floor(naive[0, 0] / 256), 0)

    def test_party_front_and_glazing_keep_explicit_context(self):
        shape = prepared_shape() | {"party_wall_edges": [1, 0, 0, 0]}
        wall = next(p for p in patches(shape) if p.side == 2 and not p.gable)
        self.assertEqual(corner_fields(wall, [[4, 16]])[0][0, 0], 256 * 221)
        glazed = next(p for p in patches(shape | {"opening_style": 7}) if p.side == 2)
        np.testing.assert_array_equal(corner_fields(glazed, [[4, 16]])[1], [[0, 0]])

    def test_winding_swaps_transport_corner_bits_and_barycentric_values(self):
        field = np.array(
            [[[1, -0.0], [2, 5], [3, 9]], [[8, 4], [7, 3], [6, 2]]], np.float32
        )
        swap = np.array([True, False])
        carried = permute_corner_fields(field, swap)
        np.testing.assert_array_equal(
            permute_corner_fields(carried, swap).view(np.uint32), field.view(np.uint32)
        )
        weight = np.array([[0.2, 0.3, 0.5], [0.1, 0.7, 0.2]])
        reordered = weight.copy()
        reordered[swap] = reordered[swap][:, [0, 2, 1]]
        np.testing.assert_allclose(
            np.einsum("ij,ijk->ik", weight, field),
            np.einsum("ij,ijk->ik", reordered, carried),
        )

    def test_material_mesh_and_cluster_keep_all_triangles_and_field_bits(self):
        control, fields, work = material_mesh(patches())
        centre, _ = cluster_mesh(
            control["positions"], control["indices"], method="centre"
        )
        shifted = carry_fields(fields, centre["corner_swap"])
        self.assertEqual(len(control["indices"]), len(centre["indices"]))
        self.assertEqual(work["categorical_code_crossing_triangles"], 0)
        for key in ("corner_uv", "corner_metres"):
            restored = permute_corner_fields(shifted[key], centre["corner_swap"])
            np.testing.assert_array_equal(
                restored.view(np.uint32), fields[key].view(np.uint32)
            )
        self.assertGreater(
            work["unique_position_normal_uv_metres_rows"], len(control["positions"])
        )

    def test_invalid_parameters_and_swap_contract_reject(self):
        wall = next(p for p in patches() if p.side == 2)
        for point in ([[np.nan, 16]], [[-0.1, 16]], [[8.1, 16]]):
            with self.assertRaises(ValueError):
                corner_fields(wall, point)
        with self.assertRaises(ValueError):
            permute_corner_fields(np.zeros((2, 3, 2)), np.array([0, 1]))

    def test_diagnostic_category_decode_has_the_shader_input_cast(self):
        uv = np.array([[256 - 1e-10, 2], [256, 2], [-354 - 1e-10, 9], [-354, 9]])
        colour = coordinate_colour(uv, np.zeros_like(uv), "program")
        np.testing.assert_array_equal(colour[0], colour[1])
        np.testing.assert_array_equal(colour[2], colour[3])


class OriginalMaterialSupportTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(
            "test/outshine/data/render/building-full-view/original-inputs.tar.gz"
        )
        with open_parent(path) as parent:
            cls.patches, basis, anchor, ids, needs = source_patches(parent, {33})
            cls.original, ob, oa, oi, on = original_patches(parent, {33})
        np.testing.assert_array_equal(basis, ob)
        np.testing.assert_array_equal(anchor, oa)
        np.testing.assert_array_equal(ids, oi)
        assert needs == on
        cls.sources, cls.needs = ids, needs

    def test_all_original_plane_frames_masks_and_unknown_sources_remain(self):
        def key(patch):
            return (patch.source, *patch.origin, *patch.axes.flat, *patch.normal)

        old, new = defaultdict(list), defaultdict(list)
        for patch in self.original:
            old[key(patch)].append(patch.domain)
        for patch in self.patches:
            new[key(patch.geometry)].append(patch.geometry.domain)
        self.assertEqual(old.keys(), new.keys())
        for identity in old:
            original = shapely.union_all(old[identity])
            rebuilt = shapely.union_all(new[identity])
            # Same projected mask contract as source_plane_mesh.triangulate.
            self.assertLessEqual(
                shapely.symmetric_difference(original, rebuilt).area,
                1e-9 * max(1, original.area),
            )
        self.assertEqual(len(self.sources) + len(self.needs), 576)
        self.assertEqual([n["source"] for n in self.needs], [2893])

    def test_actual_population_has_homogeneous_codes_and_exact_carried_fields(self):
        control, fields, _ = material_mesh(self.patches)
        centre, _ = cluster_mesh(
            control["positions"], control["indices"], method="centre"
        )
        restored = carry_fields(
            carry_fields(fields, centre["corner_swap"]), centre["corner_swap"]
        )
        for key in ("corner_uv", "corner_metres"):
            np.testing.assert_array_equal(
                restored[key].view(np.uint32), fields[key].view(np.uint32)
            )
        np.testing.assert_array_equal(
            np.unique(fields["patch_sources"]), np.sort(self.sources)
        )


if __name__ == "__main__":
    unittest.main()
