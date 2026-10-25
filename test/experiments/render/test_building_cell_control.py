"""Independent source accounting rejects omissions, fabricated failures and bad ranges."""

import copy
import unittest

from check_building_cell_control import check


def example():
    empty = {"positions": [], "normal_words": [], "uv": [], "triangles": []}
    geometry = {
        "walls": empty,
        "roofs": empty,
        "wall_colours": [],
        "wall_material_metres": [],
    }
    records = [{"cell": 7, "source_id": (1, 11)}, {"cell": 7, "source_id": (1, 12)}]
    sources = [{"index": i, "cell": 7, "kind": 1, "id": 11 + i} for i in range(2)]
    calls = [
        {
            "source": i,
            "accepted": False,
            "error": "unsupported original footprint",
            "wall_index_range": [0, 0],
            "roof_index_range": [0, 0],
        }
        for i in range(2)
    ]
    result = {
        "cell": 7,
        "accepted": True,
        "projection_calls": [],
        "details": [0, 0],
        "footprints": 2,
        "geometry": geometry,
        "mesh_calls": calls,
        "unsupported": 2,
        "fallback_triangles": 0,
    }
    return {
        "parent": "original",
        "view": {"cell": 7},
        "sources": sources,
        "results": [result],
    }, records


class CellControl(unittest.TestCase):
    def test_geometry_attributes_and_source_ranges_are_checked(self):
        report, records = example()
        result = report["results"][0]
        result["geometry"]["walls"] = {
            "positions": [[0, 0, 0], [1, 0, 0], [0, 1, 0]],
            "uv": [[0, 0], [1, 0], [0, 1]],
            "normal_words": [0, 0, 0],
            "triangles": [[0, 1, 2]],
        }
        result["mesh_calls"][0].update(
            accepted=True, error=None, wall_index_range=[0, 3]
        )
        result["mesh_calls"][1]["wall_index_range"] = [3, 0]
        result.update(unsupported=1, fallback_triangles=1)
        self.assertEqual(check(report, records, "original")["triangles"], 1)
        for stream, field, value in [
            ("walls", "triangles", [[0, 1, 3]]),
            ("walls", "normal_words", [0, 0]),
            ("walls", "positions", [[float("inf"), 0, 0], [1, 0, 0], [0, 1, 0]]),
        ]:
            damaged = copy.deepcopy(report)
            damaged["results"][0]["geometry"][stream][field] = value
            with self.assertRaises(ValueError):
                check(damaged, records, "original")

    def test_explicit_failures_preserve_original_sources(self):
        report, records = example()
        result = check(report, records, "original")
        self.assertEqual((result["sources"], result["triangles"]), (2, 0))
        self.assertEqual([f["source"] for f in result["unsupported"]], [0, 1])

    def test_source_omissions_and_id_changes_fail(self):
        report, records = example()
        for change in (
            lambda r: r["sources"].pop(),
            lambda r: r["sources"][0].update(id=999),
        ):
            damaged = copy.deepcopy(report)
            change(damaged)
            with self.assertRaises(ValueError):
                check(damaged, records, "original")

    def test_missing_reasons_duplicate_calls_and_leaked_geometry_fail(self):
        report, records = example()
        mutations = [
            lambda r: r["mesh_calls"][0].update(error=None),
            lambda r: r["mesh_calls"][1].update(source=0),
            lambda r: r["mesh_calls"].pop(),
            lambda r: r["mesh_calls"][0].update(wall_index_range=[0, 3]),
            lambda r: r.update(unsupported=0),
            lambda r: r.update(projection_calls=[{}]),
            lambda r: r.update(details=[2, 2]),
        ]
        for mutation in mutations:
            damaged = copy.deepcopy(report)
            mutation(damaged["results"][0])
            with self.assertRaises(ValueError):
                check(damaged, records, "original")


if __name__ == "__main__":
    unittest.main()
