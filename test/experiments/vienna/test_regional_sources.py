import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np
import regional


class RegionalSourcesTest(unittest.TestCase):
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
