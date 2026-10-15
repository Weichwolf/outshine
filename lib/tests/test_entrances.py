"""Shared entrance selection retains procedural and explicit frontage semantics."""

import unittest
from unittest.mock import patch

import shapely
from outshine_ref.buildings.interiors.entrances import entrance


class Entrances(unittest.TestCase):
    def setUp(self):
        self.polygon = shapely.box(0, 0, 12, 18)
        self.rooms = [self.polygon.buffer(-0.24, join_style=2)]

    def test_default_selection_and_context_frontage(self):
        a = entrance(self.polygon, self.rooms, {})
        self.assertEqual(a["centre"], [12.0, 4.5])
        self.assertEqual(a["tangent"], [0.0, 1.0])
        self.assertEqual(a["origin"], "procedural longest usable facade")
        context = {"evidence": [{"usable_frontage": True, "position_xy": [-3.0, 9.0]}]}
        b = entrance(self.polygon, self.rooms, context)
        self.assertEqual(b["centre"], [0.0, 9.0])
        self.assertEqual(b["origin"], "context-frontage")

    def test_existing_cells_and_local_filter(self):
        entry = entrance(
            self.polygon,
            self.rooms,
            {},
            fractions_by_edge={0: (0.125, 0.375, 0.625, 0.875)},
            accept=lambda d: d["centre"][1] > 9,
        )
        self.assertEqual(entry["centre"], [12.0, 11.25])
        self.assertIsNone(
            entrance(self.polygon, self.rooms, {}, accept=lambda _: False)
        )
        self.assertIsNone(entrance(self.polygon, [], {}))

    def test_capacity_and_invalid_candidates_before_geometry(self):
        with patch(
            "outshine_ref.buildings.interiors.entrances.rectangle",
            side_effect=AssertionError("expanded"),
        ), self.assertRaisesRegex(ValueError, "capacity"):
            entrance(self.polygon, self.rooms, {}, max_candidates=1)
        for positions in (
            {0: [0.5]},
            {0: (0.5, 0.5)},
            {4: (0.5,)},
            {0: (1.0,)},
            {False: (0.5,)},
        ):
            with self.assertRaises(ValueError):
                entrance(self.polygon, self.rooms, {}, fractions_by_edge=positions)


if __name__ == "__main__":
    unittest.main()
