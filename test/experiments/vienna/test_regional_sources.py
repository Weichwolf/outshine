import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import regional
from shapely.geometry import LineString, box
from source_scene import LocalScene


class RegionalSourcesTest(unittest.TestCase):
    def test_local_sources_use_the_square_terrain_extent(self):
        scene = LocalScene.__new__(LocalScene)
        scene.features = []
        woodland = box(7.5, 7.5, 9.5, 9.5)
        building = box(-9.5, -9.5, -8, -8)
        path = LineString([(-13, 9), (-8, 9)])
        outside = box(10.5, 7.5, 13, 9.5)
        with patch.object(
            scene,
            "metric_features",
            return_value=[
                ("landcover", woodland, {"class": "wood"}),
                ("building", building, {}),
                ("transportation", path, {"class": "path"}),
                ("landcover", outside, {"class": "wood"}),
            ],
        ):
            scene.add_features({}, box(-10, -10, 10, 10))
        self.assertEqual(
            [name for name, _, _ in scene.features],
            ["landcover", "building", "transportation"],
        )

    def test_skyline_uses_distinct_tiles_and_requires_their_pins(self):
        existing, skyline, behind = (14, 20, 20), (14, 21, 20), (14, 22, 20)
        source = SimpleNamespace(lon=0, lat=0, metres=np.ones(2))
        place = {"heading_deg": 90, "skyline_range_m": 16000}
        url = "https://tiles.openfreemap.org/planet/sample/14/21/20.pbf"
        bounds = {
            existing: np.array([[7000.0, 800.0], [8000.0, -800.0]]),
            skyline: np.array([[9000.0, 800.0], [10000.0, -800.0]]),
            behind: np.array([[-9000.0, 800.0], [-8000.0, -800.0]]),
        }
        region = [{"kind": "vector", "tile": list(existing), "url": "regional"}]
        with (
            patch.object(
                regional, "covering_tiles", return_value=[existing, skyline, behind]
            ),
            patch.object(regional, "tile_rect", side_effect=lambda _, t: bounds[t]),
            patch.object(regional, "acquire") as load,
        ):
            specs = regional.skyline_source_specs(source, place, "sample", 700, region)
            self.assertEqual([u for kind, _, u in specs if kind == "vector"], [url])
            with self.assertRaisesRegex(ValueError, "Unpinned Vienna skyline source"):
                regional.skyline_sources(source, place, "sample", None, 700, region, {})
            load.assert_not_called()

    def test_bridgehead_added_at_dem_border_loads_the_neighbour(self):
        tile = (14, 8, 11)
        vector = "https://tiles.openfreemap.org/planet/sample/14/8/11.pbf"
        existing = "https://tiles.mapterhorn.com/12/2/2.webp"
        adjacent = "https://tiles.mapterhorn.com/12/2/3.webp"
        pins = {url: str(i) for i, url in enumerate((vector, existing, adjacent))}
        source = SimpleNamespace(lon=0, lat=0, metres=np.ones(2))
        span = SimpleNamespace(origin=np.zeros(2), length=20, direction=np.ones(2))

        def receipt(request, _cache, *, fetch):
            kind, index, url = request
            return {"kind": kind, "tile": index, "url": url, "sha256": pins[url]}

        with (
            patch.object(regional, "covering_tiles", return_value=[tile]),
            patch.object(regional, "acquire", side_effect=receipt) as load,
        ):
            extra = regional.bridgehead_sources(
                source, [(None, span)], [{"url": existing}], "sample", None, pins
            )
        self.assertEqual(load.call_count, 2)
        self.assertEqual({r["url"] for r in extra}, {adjacent, vector})
