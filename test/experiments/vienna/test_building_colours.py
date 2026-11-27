import unittest
from types import SimpleNamespace

import numpy as np
from building_colours import facade_colours, mapped_colour
from materials import surface_properties
from regional_detail import building_shells
from scene_mesh import assemble
from shapely.geometry import box, mapping

PARENT = {"colour": "#c68d94", "hide_3d": True, "render_height": 17}
PART = {"render_height": 12}
PAINTED_PART = PART | {"colour": "DCD2C8"}


class BuildingColourTest(unittest.TestCase):
    def test_colour_is_opaque_and_converted_to_linear_reflectance_once(self):
        self.assertEqual(mapped_colour({"colour": "#ABC"}), "#aabbcc")
        self.assertIsNone(mapped_colour({"colour": "#1122337f"}))
        self.assertIsNone(mapped_colour({"colour": "red; blue"}))
        material, rgb, roughness = surface_properties("wall #808080")
        self.assertEqual(material, "wall")
        self.assertAlmostEqual(rgb[0], 0.21586, places=5)
        self.assertEqual(roughness, surface_properties("wall")[2])

    def test_part_finish_precedes_parent_and_ambiguous_colours_are_skipped(self):
        parent = box(0, 0, 12, 10)
        geometry = [parent, box(1, 1, 3, 3), box(4, 4, 6, 6), box(11, 1, 13, 3)]
        colours = facade_colours(geometry, [PARENT, PART, PAINTED_PART, PART])
        self.assertEqual(colours, ["#c68d94", "#c68d94", "#dcd2c8", None])
        colours = facade_colours(
            [parent, parent, geometry[1]],
            [PARENT, PARENT | {"colour": "#332211"}, PART],
        )
        self.assertIsNone(colours[-1])

    def test_local_and_regional_shells_keep_the_same_colours(self):
        rows = [
            (box(12, 12, 29, 29), PARENT),
            (box(13, 13, 18, 19), PART),
            (box(19, 13, 28, 19), PAINTED_PART),
        ]
        near = assemble(
            SimpleNamespace(
                features=[("building", g, p) for g, p in rows],
                height=lambda xy: np.zeros(len(xy)),
            ),
            40,
        )
        self.assertIn("wall #c68d94", near)
        self.assertIn("wall #dcd2c8", near)
        self.assertNotIn("wall", near)
        tile = {
            "building": {
                "extent": 64,
                "features": [
                    {"geometry": mapping(g), "properties": p} for g, p in rows
                ],
            }
        }
        source = SimpleNamespace(metres=np.ones(2), lon=0, lat=0, datum=0)
        dem = SimpleNamespace(
            sample_with_zoom=lambda lat, lon, **kw: (np.zeros_like(lat), None)
        )
        regional = building_shells(
            tile, np.array([-32, 32]), np.array([32, -32]), source, dem, 1, 700
        )
        materials = {name for name, _, _ in regional}
        self.assertIn("regional walls #c68d94", materials)
        self.assertIn("regional walls #dcd2c8", materials)
        self.assertNotIn("regional walls", materials)
