"""Exact scalar controls, full material-state separation and source-local reuse."""

import copy
import unittest
from unittest.mock import patch

import numpy as np
import shapely
from building_mass_appearance import (
    clipped_wall_areas,
    clipped_wall_parts,
    patch_measure,
    patch_normal_areas,
    source_measure,
)
from building_mass_materials import material, policy
from source_material_patches import shape_material_patches
from test_building_mass_materials import geometry, surface


def scalar_measure(source, frame, at):
    result = np.zeros((2, 4 + len(at["palette"])))
    patches = calls = 0
    for part, shape in enumerate(source["shapes"]):
        for p in shape_material_patches(
            source, shape, 1, part, frame["basis"], frame["anchor"]
        ):
            value, count = patch_measure(p, at)
            result[int(p.side == 1)] += value
            patches += 1
            calls += count
    return result, patches, calls


class BuildingSourceMaterialWork(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.at = policy()

    def test_wall_areas_preserve_sloping_split_and_empty_sides(self):
        domains = [
            shapely.Polygon([(0, 0), (2, 0), (2, 4), (0, 2)]),
            shapely.box(3, 1, 5, 2),
        ]
        for base, expected in (
            (0, [[0, 6], [0, 2]]),
            (3, [[5.5, 0.5], [2, 0]]),
            (4, [[6, 0], [2, 0]]),
        ):
            actual = clipped_wall_areas(domains, base)
            self.assertEqual(actual.tobytes(), np.asarray(expected, float).tobytes())
            scalar = np.zeros((2, 2))
            for row, domain in enumerate(domains):
                for above, area in clipped_wall_parts(domain, base):
                    scalar[row, int(above)] = area
            self.assertEqual(scalar.tobytes(), actual.tobytes())
        self.assertEqual(clipped_wall_areas([], 3).shape, (0, 2))

    def test_whole_wall_areas_need_no_intersection_and_partial_walls_still_clip(self):
        domains = [
            shapely.Polygon([(0, 0), (2, 0), (2, 4), (0, 2)]),
            shapely.box(3, 1, 5, 2),
        ]
        with patch(
            "building_mass_appearance.shapely.intersection",
            side_effect=AssertionError("whole domains require no clipping"),
        ):
            for base, expected in ((0, [[0, 6], [0, 2]]), (4, [[6, 0], [2, 0]])):
                actual = clipped_wall_areas(domains, base)
                np.testing.assert_array_equal(actual, expected)
        with patch(
            "building_mass_appearance.shapely.intersection", wraps=shapely.intersection
        ) as intersect:
            actual = clipped_wall_areas(domains, 3)
            np.testing.assert_array_equal(actual, [[5.5, 0.5], [2, 0]])
            self.assertEqual(len(intersect.call_args.args[0]), 2)

    def test_local_measures_keep_sloped_areas_without_world_geometry(self):
        source = surface()
        source["shapes"][0].update(
            roof_kind=1, rise_m=1, centre=[1, 1.5], axis_u=[1, 0], half_v_m=1.5
        )
        with patch(
            "source_material_patches.SurfacePatch",
            side_effect=AssertionError("world patch not requested"),
        ):
            value, _, _ = source_measure(1, source, geometry(), self.at)
        np.testing.assert_allclose(
            value[:, -1], [53, 4 * np.sqrt(1.5**2 + 1)], atol=1e-12, rtol=0
        )
        with self.assertRaisesRegex(ValueError, "positive finite source"):
            source_measure(1, source | {"shapes": []}, geometry(), self.at)

    def test_identical_walls_reuse_state_but_source_lifetime_ends(self):
        source, frame = surface(), geometry()
        scalar, count, calls = scalar_measure(source, frame, self.at)
        with patch("building_mass_appearance.material", wraps=material) as evaluate:
            value, patches, actual = source_measure(1, source, frame, self.at)
            self.assertEqual(actual, evaluate.call_count)
            self.assertLess(actual, calls)
            self.assertEqual(patches, count)
            self.assertEqual(value.tobytes(), scalar.tobytes())
            source_measure(2, source, frame, self.at)
            self.assertEqual(evaluate.call_count, 2 * actual)

    def test_different_openings_derivatives_roof_categories_and_paint_stay_distinct(
        self,
    ):
        source = surface([0.2, 0.3, 0.4])
        a = source["shapes"][0]
        a.update(roof_kind=1, rise_m=1, centre=[1, 1.5], axis_u=[1, 0], half_v_m=1.5)
        b = copy.deepcopy(a)
        b.update(
            ident=3,
            wall_variant=3,
            opening_style=1,
            front_edge=2,
            party_wall_edges=[1, 0, 1, 0],
            floor_m=4,
            rise_m=2,
            seat_m=1,
            foot_m=0.4,
        )
        source["shapes"].append(b)
        angle = 0.13
        c, s = np.cos(angle), np.sin(angle)
        source.update(east=[c, 0, s], up=[-s, 0, c])
        frame = geometry() | {
            "basis": np.array([[0, 1, 0], [-1, 0, 0], [0, 0, 1]]),
            "anchor": np.array([10, 20, 30]),
        }
        for paint in ([0.2, 0.3, 0.4], [0.7, 0.4, 0.2]):
            source["wall_colour"] = paint
            expected, count, calls = scalar_measure(source, frame, self.at)
            actual, patches, evaluated = source_measure(1, source, frame, self.at)
            self.assertEqual(actual.dtype, expected.dtype)
            self.assertEqual(actual.shape, expected.shape)
            self.assertEqual(actual.tobytes(), expected.tobytes())
            self.assertEqual(count, patches)
            self.assertLess(evaluated, calls)

    def test_batched_patch_values_keep_scalar_words_and_empty_shape(self):
        source, frame = surface(), geometry()
        patches = shape_material_patches(
            source, source["shapes"][0], 1, 0, frame["basis"], frame["anchor"]
        )
        normals, areas = patch_normal_areas(patches)
        for p, n, a in zip(patches, normals, areas, strict=True):
            expected, calls = patch_measure(p, self.at)
            actual, uses = patch_measure(p, self.at, normal_area=(n, a))
            self.assertEqual(actual.tobytes(), expected.tobytes())
            self.assertEqual(uses, calls)
        n, a = patch_normal_areas([])
        self.assertEqual(n.shape, (0, 3))
        self.assertEqual(a.shape, (0,))


if __name__ == "__main__":
    unittest.main()
