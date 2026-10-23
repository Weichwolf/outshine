"""Source-informed rejection of hidden volumes and gaps before field admission."""

import unittest

import numpy as np
from reconstructed_source_field import SOLID, UNKNOWN, FieldBuilder
from test_source_support_function import compile_surfaces, shape, surface


class ReconstructedFieldTest(unittest.TestCase):
    def test_zero_corners_require_source_containment_before_a_solid_cube(self):
        support = compile_surfaces([surface([shape()])])[0]
        field, _ = FieldBuilder(support, max_nodes=1).run()
        self.assertEqual(int(field["tags"][0]), SOLID)
        self.assertEqual(field["values"].shape, (0, 8))

    def test_contained_building_keeps_a_source_need_when_corners_miss(self):
        small = shape(
            ring=[[0.01, 0.01], [0.02, 0.01], [0.02, 0.02], [0.01, 0.02]],
            sole_m=0.25,
            eaves_m=0.75,
        )
        support = compile_surfaces([surface([small])])[0]
        field, _ = FieldBuilder(support, max_nodes=1).run()
        self.assertTrue(int(field["tags"][0]) & UNKNOWN)
        np.testing.assert_array_equal(field["unknown_sources"], [0])

    def test_off_centre_two_centimetre_gap_is_not_filled(self):
        a = shape(ring=[[0, 0], [1.23, 0], [1.23, 4], [0, 4]], eaves_m=4)
        b = shape(ring=[[1.25, 0], [4, 0], [4, 4], [1.25, 4]], eaves_m=4)
        support = compile_surfaces([surface([a]), surface([b])])[0]
        field, _ = FieldBuilder(support, max_nodes=1).run()
        self.assertTrue(int(field["tags"][0]) & UNKNOWN)
        np.testing.assert_array_equal(field["unknown_sources"], [0, 1])

    def test_tiny_courtyard_assembled_from_parts_stays_a_need(self):
        rings = [
            [[0, 0], [1.23, 0], [1.23, 4], [0, 4]],
            [[1.25, 0], [4, 0], [4, 4], [1.25, 4]],
            [[1.23, 0], [1.25, 0], [1.25, 1.23], [1.23, 1.23]],
            [[1.23, 1.25], [1.25, 1.25], [1.25, 4], [1.23, 4]],
        ]
        support = compile_surfaces(
            [surface([shape(ring=r, eaves_m=4) for r in rings])]
        )[0]
        field, _ = FieldBuilder(support, max_nodes=1).run()
        self.assertTrue(int(field["tags"][0]) & UNKNOWN)
        np.testing.assert_array_equal(field["unknown_sources"], [0])
