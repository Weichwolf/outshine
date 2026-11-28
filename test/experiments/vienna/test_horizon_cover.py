import unittest

import numpy as np
import shapely
from horizon_cover import COVER_CODES, COVER_MATERIALS, cover_material, rasterize_cover


def material_pixel(name):
    colour, roughness = COVER_MATERIALS[name]
    return np.rint(np.array((*colour, roughness)) * 255).astype(int)


class HorizonCoverTest(unittest.TestCase):
    def test_coverage_uses_world_axis_order_and_keeps_a_clearance(self):
        woodland = shapely.box(-5, 2, 16.37, 15).difference(shapely.box(8, 7, 11, 12))
        features = [woodland, shapely.box(17, 11, 23, 19), shapely.box(9, 8, 11, 10)]
        cover = rasterize_cover(
            [(COVER_CODES["wood"], shape) for shape in features],
            (0, 0, 20, 20),
            size=128,
        )

        np.testing.assert_allclose(cover.transform, (0, 0, 0.05, 0.05))
        self.assertEqual(cover.pixels.dtype, np.uint8)
        forest, plain = material_pixel("wood"), material_pixel("distant terrain")
        for y, x, material in (
            (25, 64, forest),
            (68, 57, plain),
            (60, 60, forest),
            (108, 120, forest),
            (120, 64, plain),
        ):
            np.testing.assert_array_equal(cover.pixels[y, x], material)
        pixels = cover.pixels[..., 2]
        self.assertTrue(np.any((pixels > forest[2]) & (pixels < plain[2])))
        calculated_area = (plain[2] - pixels.mean()) * 400 / (plain[2] - forest[2])
        domain = shapely.box(0, 0, 20, 20)
        expected = shapely.union_all(features).intersection(domain)
        supersample_diagonal = np.sqrt(2) * 20 / (2 * len(cover.pixels))
        boundary_band = expected.boundary.buffer(supersample_diagonal)
        self.assertAlmostEqual(
            calculated_area,
            expected.area,
            delta=boundary_band.intersection(domain).area,
        )

    def test_openings_reveal_underlying_land_use_and_edges_filter_properties(self):
        field = shapely.box(0, 0, 18.5, 18)
        rock = shapely.box(4, 4, 14, 14).difference(shapely.box(7, 8, 10, 11))
        cover = rasterize_cover(
            [(COVER_CODES["bare rock"], rock), (COVER_CODES["dry crop"], field)],
            (0, 0, 20, 20),
            size=80,
        )
        np.testing.assert_array_equal(cover.pixels[23, 23], material_pixel("bare rock"))
        np.testing.assert_array_equal(cover.pixels[35, 35], material_pixel("dry crop"))
        np.testing.assert_array_equal(
            cover.pixels[0, 35], material_pixel("distant terrain")
        )
        edges = cover.pixels[37, 70:77]
        field_pixel = material_pixel("dry crop")
        plain_pixel = material_pixel("distant terrain")
        midpoint = (plain_pixel + field_pixel) / 2
        self.assertTrue(np.any(np.max(abs(edges - midpoint), axis=1) <= 1))

    def test_mapped_crops_keep_repeated_feature_ids_in_the_same_palette(self):
        self.assertEqual(
            cover_material(
                "landcover", {"class": "farmland", "subclass": "vineyard"}, 2
            ),
            "vineyard",
        )
        self.assertEqual(
            cover_material("landcover", {"class": "rock", "subclass": "scree"}), "scree"
        )
        keys = [
            cover_material("landcover", {"class": "farmland"}, feature)
            for feature in range(1, 22)
        ]
        self.assertEqual(set(keys), {"green crop", "dry crop", "field soil"})
        self.assertEqual(
            keys[17], cover_material("landcover", {"class": "farmland"}, 18)
        )
