"""Whole historical feature lineage, holes and missing-input controls."""

import unittest

import numpy as np
import shapely
from native_water_observation import NO_INPUT, SURFACE_V3
from shore_road_context import whole_feature


def fixture():
    polygons = [
        shapely.Polygon(
            [(0, 0), (2, 0), (2, 2), (0, 2)],
            [[(0.5, 0.5), (0.5, 1.5), (1.5, 1.5), (1.5, 0.5)]],
        ),
        shapely.box(2, 0, 4, 2),
        shapely.box(0.75, 0.75, 1.25, 1.25),
    ]
    rings, points, surfaces = [], [], []
    for i, polygon in enumerate(polygons):
        contours = [polygon.exterior, *polygon.interiors]
        surfaces.append((len(rings), len(contours), 10 + i, i))
        for contour in contours:
            values = np.array(contour.coords)[:, ::-1]
            rings.append((len(points), len(values)))
            points.extend(values)
    inputs = [
        {
            "source": 0 if i < 2 else 1,
            "provider_feature_id": "100",
            "layer": "water_polygons",
            "kind": "river",
            "bridge": False,
            "tunnel": False,
            "crossing": "",
        }
        for i in range(3)
    ]
    return {
        "surfaces": np.array(surfaces, dtype=SURFACE_V3),
        "rings": np.array(
            rings, dtype=[("first_point", "<u4"), ("point_count", "<u4")]
        ),
        "points_lat_lon": np.array(points),
        "inputs": inputs,
        "sources": [
            {"provider": "source", "revision": "old"},
            {"provider": "source", "revision": "new"},
        ],
    }


class WholeShoreContext(unittest.TestCase):
    def test_all_known_fragments_retained_hole_preserved_and_epochs_separate(self):
        data = fixture()
        body, ids, key = whole_feature(data, 0)
        self.assertEqual(ids, [0, 1])
        self.assertEqual(body.area, 7)
        self.assertEqual(len(body.interiors), 1)
        self.assertFalse(body.covers(shapely.Point(1, 1)))
        self.assertEqual(key[:3], ("source", "old", "100"))
        self.assertEqual(whole_feature(data, 1)[0].wkb, body.wkb)
        self.assertEqual(whole_feature(data, 2)[1], [2])

    def test_unknown_identity_stays_unknown_and_invalid_source_is_refused(self):
        data = fixture()
        data["surfaces"][2]["input"] = NO_INPUT
        self.assertEqual(whole_feature(data, 0)[1], [0, 1])
        for selected in (2, -1, 3, True):
            with self.assertRaises(ValueError):
                whole_feature(data, selected)
        data["inputs"][0]["provider_feature_id"] = None
        with self.assertRaises(ValueError):
            whole_feature(data, 0)


if __name__ == "__main__":
    unittest.main()
