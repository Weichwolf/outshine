"""Known normalized classes recover width recipes without changing source evidence."""

import json
import unittest
from copy import deepcopy
from pathlib import Path

import numpy as np
import shapely
from transport_component_surfaces import assemble
from transport_flat_plan import plan
from transport_network import source_roads
from transport_travel import compile_travel

WIDTHS = {
    "residential": 5.5,
    "subway": 3.8,
    "tram": 3.0,
    "footway": 1.8,
    "cycleway": 2.0,
}
RECIPES = json.loads(
    Path(__file__).with_name("transport_network_recipes.json").read_text()
)


def road(index=0, width=None, **properties):
    return {
        "line": shapely.LineString([[index * 20, 0], [index * 20 + 10, 0]]),
        "width": width,
        "properties": {"class": "minor", **properties},
        "source_id": f"width-control:{index}",
    }


class WidthRecipes(unittest.TestCase):
    def test_normalized_known_classes_use_their_existing_widths_and_keep_sources(self):
        sources = [
            road(0),
            road(1, **{"class": "transit", "subclass": "subway"}),
            road(2, **{"class": "transit", "subclass": "tram"}),
            road(3, **{"class": "path", "subclass": "footway"}),
        ]
        original = deepcopy(sources)
        accepted, report = source_roads(sources, RECIPES, WIDTHS)
        self.assertEqual([r["width"] for r in accepted], [5.5, 3.8, 3.0, 1.8])
        self.assertEqual(
            report["inferred_widths"],
            {k: 1 for k in ("residential", "subway", "tram", "footway")},
        )
        self.assertEqual(sources, original)
        for source, normalized in zip(sources, accepted):
            self.assertEqual(source["source_id"], normalized["source_id"])
            self.assertEqual(source["properties"], normalized["properties"])
            self.assertEqual(source["line"].wkb, normalized["line"].wkb)

    def test_explicit_source_and_axis_recipe_widths_keep_their_precedence(self):
        recipes = deepcopy(RECIPES)
        recipes["axisRecipes"].update(
            residential={"widthM": 7.0},
            tram={"widthFrom": "cycleway"},
            minor_construction={"widthM": 7.75},
        )
        sources = [
            road(0, width=4.25),
            road(1),
            road(2, **{"class": "transit", "subclass": "tram"}),
            road(3, **{"class": "minor_construction"}),
        ]
        accepted, report = source_roads(sources, recipes, WIDTHS)
        self.assertEqual([r["width"] for r in accepted], [4.25, 7.0, 2.0, 7.75])
        self.assertEqual(
            report["inferred_widths"],
            {"residential": 1, "tram": 1, "minor_construction": 1},
        )

    def test_routes_unknown_classes_and_construction_aliases_stay_distinct(self):
        sources = [
            road(0, **{"class": "ferry"}),
            road(1, **{"class": "unknown-width"}),
            road(2, **{"class": "minor_construction"}),
        ]
        accepted, report = source_roads(sources, RECIPES, WIDTHS)
        self.assertEqual([r["source_id"] for r in accepted], ["width-control:2"])
        self.assertEqual(accepted[0]["width"], 5.5)
        self.assertEqual(report["routes"], {"ferry": 1})
        self.assertEqual(report["unclassified"], {"unknown-width": 1})

    def test_level_indoor_unknown_and_source_ids_survive_the_vector_plan(self):
        sources = [
            road(0),
            road(1, layer=1, brunnel="bridge"),
            road(2, **{"class": "path", "subclass": "footway", "indoor": 1}),
            road(3, **{"class": "unknown-width"}),
        ]
        accepted, graphs, report = plan(sources, RECIPES, WIDTHS)
        self.assertEqual([r["source_id"] for r in accepted], ["width-control:0"])
        self.assertEqual(sum(len(g["lines"]) for g in graphs.values()), 1)
        self.assertEqual(
            [r["id"] for r in report["usage"]], [s["source_id"] for s in sources]
        )
        self.assertEqual(
            [r["reason"] for r in report["usage"]],
            ["", "deferred_level", "deferred_indoor_recipe", "route_or_missing_recipe"],
        )
        self.assertEqual(
            report["normalization"]["inferred_widths"], {"residential": 2, "footway": 1}
        )

    def test_inferred_and_explicit_recipes_have_identical_meshes_and_permissions(self):
        sources = [
            road(0, access="private", oneway="yes"),
            road(1, **{"class": "path", "subclass": "footway"}),
            road(2, **{"class": "transit", "subclass": "tram"}),
        ]
        explicit = [dict(r, width=w) for r, w in zip(sources, (5.5, 1.8, 3.0))]
        outputs = []
        for raw in (sources, explicit):
            accepted, graphs, report = plan(raw, RECIPES, WIDTHS)
            compile_travel(accepted, graphs, RECIPES["travelRecipes"])
            widths = np.array([r["width"] for r in accepted])
            for graph in graphs.values():
                graph["half_widths"] = 0.5 * np.maximum.reduceat(
                    widths[graph["owner_sources"]], graph["owner_offsets"][:-1]
                )
            products, audit = assemble(list(graphs.values()), corner_radius=0)
            self.assertTrue(audit["complete"], audit)
            outputs.append((graphs, report["usage"], products))
        a, b = outputs
        self.assertEqual(a[1], b[1])
        self.assertEqual(a[0].keys(), b[0].keys())
        for kind in a[0]:
            for key in (
                "vertices",
                "edges",
                "owner_sources",
                "owner_directions",
                "half_widths",
                "travel_public",
                "travel_restricted",
            ):
                np.testing.assert_array_equal(a[0][kind][key], b[0][kind][key])
        self.assertEqual(len(a[2]), len(b[2]))
        for inferred, explicit in zip(a[2], b[2]):
            self.assertEqual(inferred.keys(), explicit.keys())
            for key in inferred:
                np.testing.assert_array_equal(inferred[key], explicit[key])


if __name__ == "__main__":
    unittest.main()
