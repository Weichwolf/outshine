"""Domain storage roundtrips, including undersides, holes and separate sources."""

import unittest
from dataclasses import replace
from unittest.mock import patch

import numpy as np
import shapely
from complete_source_patches import complete_shape_patches
from source_domain_buffer import DomainBudget, pack, restore_source
from test_source_material_mesh import prepared_shape, surface
from welded_source_patch import source_corners


class SourceDomainBuffer(unittest.TestCase):
    def setUp(self):
        shape = prepared_shape() | {"form": 0, "round_footprint": False}
        shape["holes"] = [[[2, 1], [2, 3], [6, 3], [6, 1]]]
        self.surface = surface() | {"wall_colour": None, "shapes": [shape]}
        self.patches = complete_shape_patches(
            self.surface, shape, 42, 0, np.eye(3), np.zeros(3)
        )

    def test_direct_and_dictionary_preserve_every_local_control_and_hole(self):
        for dictionary in (False, True, "scalar"):
            arrays = pack(
                [(42, self.surface, self.patches)], np.eye(3), dictionary=dictionary
            )
            with patch(
                "source_material_domains.shape_material_domains",
                side_effect=AssertionError,
            ):
                restored = restore_source(arrays, 42, self.surface)
            self.assertEqual(len(restored), len(self.patches))
            for before, after in zip(self.patches, restored, strict=True):
                self.assertEqual(before.geometry.domain.wkb, after.geometry.domain.wkb)
                np.testing.assert_array_equal(
                    before.local_axes.view("u8"), after.local_axes.view("u8")
                )
                np.testing.assert_array_equal(
                    np.asarray(before.local_origin).view("u8"),
                    after.local_origin.view("u8"),
                )
                self.assertEqual(
                    (before.part, before.side, before.gable, before.orientation),
                    (after.part, after.side, after.gable, after.orientation),
                )
            self.assertEqual(
                next(p for p in restored if p.side == 0).geometry.domain.area, 24
            )
            old, _ = source_corners(self.patches, np.eye(3))
            new, _ = source_corners(restored, np.eye(3))
            for key in old:
                np.testing.assert_array_equal(old[key].view("u1"), new[key].view("u1"))

    def test_source_selection_restores_only_requested_domains(self):
        second = [
            replace(p, geometry=replace(p.geometry, source=43)) for p in self.patches
        ]
        arrays = pack(
            [(42, self.surface, self.patches), (43, self.surface, second)], np.eye(3)
        )
        import source_domain_buffer

        with patch(
            "source_domain_buffer.domain_at", wraps=source_domain_buffer.domain_at
        ) as read:
            self.assertEqual(len(restore_source(arrays, 43, self.surface)), len(second))
        self.assertEqual(read.call_count, len(second))
        self.assertTrue(
            all(call.args[1] >= len(self.patches) for call in read.call_args_list)
        )
        with self.assertRaises(ValueError):
            restore_source(arrays, 99, self.surface)

    def test_budget_counts_all_rings_before_any_polygon_construction(self):
        for dictionary in (False, True, "scalar"):
            arrays = pack(
                [(42, self.surface, self.patches)], np.eye(3), dictionary=dictionary
            )
            count = int(arrays["ring_vertex_offsets"][-1])
            with (
                patch("source_domain_buffer.domain_at", side_effect=AssertionError),
                self.assertRaises(DomainBudget) as failure,
            ):
                restore_source(arrays, 42, self.surface, max_controls=count - 1)
            self.assertEqual(failure.exception.controls, count)
            self.assertEqual(failure.exception.identity, 42)
            restored = restore_source(arrays, 42, self.surface, max_controls=count)
            self.assertEqual(
                [p.geometry.domain.wkb for p in restored],
                [p.geometry.domain.wkb for p in self.patches],
            )
            for invalid in (-1, True, 3.5, 1048577):
                with self.assertRaises(ValueError):
                    restore_source(arrays, 42, self.surface, max_controls=invalid)

    def test_budget_is_local_to_source_and_counts_dictionary_occurrences(self):
        floor = next(p for p in self.patches if p.side == 0)
        small = replace(floor, geometry=replace(floor.geometry, source=43))
        for dictionary in (False, True, "scalar"):
            arrays = pack(
                [(42, self.surface, self.patches), (43, self.surface, [small])],
                np.eye(3),
                dictionary=dictionary,
            )
            count = sum(
                len(r.coords) - 1
                for polygon in shapely.get_parts(small.geometry.domain)
                for r in (polygon.exterior, *polygon.interiors)
            )
            self.assertEqual(
                len(restore_source(arrays, 43, self.surface, max_controls=count)), 1
            )
            with self.assertRaises(DomainBudget):
                restore_source(arrays, 42, self.surface, max_controls=count)

    def test_disjoint_components_do_not_fill_gap(self):
        floor = next(p for p in self.patches if p.side == 0)
        geometry = replace(
            floor.geometry,
            domain=shapely.MultiPolygon(
                [
                    shapely.box(0, 0, 1, 1),
                    shapely.box(2, 0, 3, 1),
                ]
            ),
        )
        arrays = pack(
            [(42, self.surface, [replace(floor, geometry=geometry)])], np.eye(3)
        )
        restored = restore_source(arrays, 42, self.surface)[0]
        self.assertEqual(restored.geometry.domain.area, 2)
        self.assertFalse(restored.geometry.domain.covers(shapely.Point(1.5, 0.5)))

    def test_scalar_dictionary_preserves_signed_zero_control_words(self):
        floor = next(p for p in self.patches if p.side == 0)
        origin = floor.local_origin.copy()
        origin[0] = -0.0
        modified = replace(floor, local_origin=origin)
        arrays = pack([(42, self.surface, [modified])], np.eye(3), dictionary="scalar")
        self.assertEqual(arrays["local_frames"].dtype, np.dtype("<u2"))
        self.assertIn(0, arrays["float64_words"])
        self.assertIn(1 << 63, arrays["float64_words"])
        restored = restore_source(arrays, 42, self.surface)[0]
        np.testing.assert_array_equal(
            origin.view("u8"), restored.local_origin.view("u8")
        )

    def test_scalar_dictionary_does_not_wrap_more_than_65536_values(self):
        x = np.arange(33000, dtype=np.float64)
        domain = shapely.Polygon(np.c_[x, x * x + 0.125])
        floor = next(p for p in self.patches if p.side == 0)
        modified = replace(floor, geometry=replace(floor.geometry, domain=domain))
        arrays = pack([(42, self.surface, [modified])], np.eye(3), dictionary="scalar")
        self.assertGreater(len(arrays["float64_words"]), 65536)
        self.assertEqual(arrays["coordinates"].dtype, np.dtype("<u4"))
        restored = restore_source(arrays, 42, self.surface)[0]
        self.assertEqual(domain.wkb, restored.geometry.domain.wkb)


if __name__ == "__main__":
    unittest.main()
