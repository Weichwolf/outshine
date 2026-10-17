"""Actual Float32 collision, outward pads and authoritative old-grid admission."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_cached_box_grids import GRID_M, bind_grids, outward_box
from stair_module_source_contract import verify_source_contract
from stair_shared_boxes import add_boxes, box_plans, common_pad


def context():
    path = (
        Path(__file__).with_name("fixtures") / "stair-cached-grid-collision-vienna.json"
    )
    actual = json.loads(path.read_text())
    node = np.array(actual["original_node_xy_m"])
    old = np.array([node - [10, 0], node])
    sources = {
        "old": {"xy_m": old.tolist(), "mvt_xy": [[-10, 0], [0, 0]]},
        "new": {
            "xy_m": [node.tolist(), (node + [10, 0]).tolist()],
            "mvt_xy": [[0, 0], [10, 0]],
        },
    }
    state = {
        "cached_module_rows": {"old": {"xy": old.tolist(), "source_grid_m": GRID_M}}
    }
    recipe = {
        "contact_mode": "profile",
        "junction_mode": "shared_box",
        "source_ids": ["new"],
        "port": {"source_id": "old", "end": -1},
        "cached_box_grids_m": {"old": GRID_M},
    }
    rows = [
        {"source_id": "new", "xy": np.array(sources["new"]["xy_m"]), "width_m": 5.5}
    ]
    nodes = [{"xy": node.tolist(), "mvt_xy": [0, 0], "incident": [0]}]
    line = np.c_[node + [[0, -2.75], [0, 0], [0, 2.75]], np.full(3, 164.0)]
    return actual, state, sources, recipe, rows, nodes, line


class CachedSourceGridBoxes(unittest.TestCase):
    def test_actual_ungridded_boundary_collision_is_not_a_zero_distance(self):
        actual, _s, _src, _r, rows, nodes, line = context()
        legacy = box_plans(rows, nodes, line, [], include_interiors=False)[(0, 0)]
        boundary = np.array(actual["old_boundary_xy_m"])
        frozen = boundary.copy()
        cuts = shapely.get_coordinates(
            shapely.LineString(boundary).intersection(legacy.boundary)
        )
        expected = np.array(actual["new_ungridded_boundary_point_xy_m"])
        chosen = cuts[np.argmin(np.linalg.norm(cuts - expected, axis=1))]
        self.assertLess(np.linalg.norm(chosen - expected), 1e-10)
        self.assertGreater(np.linalg.norm(chosen - boundary[1]), 1e-7)
        np.testing.assert_array_equal(chosen.astype("f4"), boundary[1].astype("f4"))
        grid = outward_box(legacy, actual["source_grid_m"])
        cuts = shapely.get_coordinates(
            shapely.LineString(boundary).intersection(grid.boundary)
        )
        self.assertTrue(np.any(np.all(cuts == boundary[1], axis=1)))
        np.testing.assert_array_equal(boundary, frozen)

    def test_outward_box_retains_whole_geometry_and_has_exact_float32_corners(self):
        actual, _s, _src, _r, rows, nodes, line = context()
        legacy = box_plans(rows, nodes, line, [], include_interiors=False)[(0, 0)]
        for shape in (legacy, shapely.box(-12.3456, -9.8765, -3.21, 4.567)):
            grid = outward_box(shape, GRID_M)
            self.assertTrue(grid.covers(shape))
            self.assertLessEqual(grid.hausdorff_distance(shape), np.sqrt(2) * GRID_M)
            xy = shapely.get_coordinates(grid)
            np.testing.assert_array_equal(xy, xy.astype("f4").astype(float))
        self.assertEqual(actual["source_grid_m"], GRID_M)

    def test_false_foreign_missing_and_changed_frame_grid_bindings_are_refused(self):
        _a, state, sources, recipe, *_rest = context()
        self.assertEqual(bind_grids(state, recipe, sources, []), {(0, 0): GRID_M})
        for declared in (
            {"old": 2 * GRID_M},
            {"unknown": GRID_M},
            [GRID_M],
            {"old": None},
        ):
            with self.subTest(declared=declared), self.assertRaises(ValueError):
                bind_grids(
                    state, dict(recipe, cached_box_grids_m=declared), sources, []
                )
        for row in (
            {"xy": sources["old"]["xy_m"]},
            {
                "xy": (np.array(sources["old"]["xy_m"]) + [0.001, 0]).tolist(),
                "source_grid_m": GRID_M,
            },
        ):
            with self.subTest(row=row), self.assertRaises(ValueError):
                bind_grids({"cached_module_rows": {"old": row}}, recipe, sources, [])

    def test_independent_reader_requires_verified_grid_and_complete_saved_box(self):
        _a, state, sources, recipe, rows, nodes, line = context()
        bound = bind_grids(state, recipe, sources, [])
        add_boxes(rows, nodes, line, [], include_interiors=False, cached_grids=bound)
        result = verify_source_contract(
            rows, sources, recipe, [], line, nodes, cached_grids=bound
        )
        self.assertEqual(result["cached_box_grids_m"], recipe["cached_box_grids_m"])
        with self.assertRaisesRegex(ValueError, "independently bound"):
            verify_source_contract(rows, sources, recipe, [], line, nodes)
        rows[0].pop("junction_pads_xy_m")
        add_boxes(rows, nodes, line, [], include_interiors=False)
        with self.assertRaisesRegex(ValueError, "box missing"):
            verify_source_contract(
                rows, sources, recipe, [], line, nodes, cached_grids=bound
            )

    def test_wrong_family_range_and_unrelated_anchor_do_not_quantize_old_inputs(self):
        _a, state, sources, recipe, rows, nodes, line = context()
        original = copy.deepcopy(state)
        for mode in ("free", "level_box"):
            with self.assertRaises(ValueError):
                bind_grids(state, dict(recipe, junction_mode=mode), sources, [])
        with self.assertRaises(ValueError):
            outward_box(shapely.box(8189, 0, 8191, 1), GRID_M)
        with self.assertRaises(ValueError):
            box_plans(
                rows,
                nodes,
                line,
                [],
                include_interiors=False,
                cached_grids={(99, 99): GRID_M},
            )
        self.assertEqual(state, original)

    def test_absent_grid_keeps_old_box_coordinates_and_metadata_exact(self):
        _a, state, sources, recipe, rows, nodes, line = context()
        recipe = {k: v for k, v in recipe.items() if k != "cached_box_grids_m"}
        before = box_plans(rows, nodes, line, [], include_interiors=False)
        after = box_plans(
            rows,
            nodes,
            line,
            [],
            include_interiors=False,
            cached_grids=bind_grids(state, recipe, sources, []),
        )
        self.assertEqual(before.keys(), after.keys())
        for key in before:
            self.assertEqual(before[key].wkb, after[key].wkb)

    def test_common_pad_can_use_only_declared_incident_masked_old_surfaces(self):
        rows = [
            {"source_id": "new", "xy": np.array([[0, 0], [4, 0]], float), "width_m": 2}
        ]
        nodes = [{"mvt_xy": [0, 0], "xy": [0, 0], "incident": [0]}]
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        old = np.array([[-4, -2, 0], [0, -2, 0], [0, 2, 0], [-4, 2, 0]], float)
        cached = old + [4, 0, 0]
        data = {
            "surface_vertices": cached,
            "surface_faces": faces,
            "owners": np.zeros((2, 1), bool),
        }
        line = np.array([[-2, 0], [0, 0], [2, 0]], float)
        args = data, nodes, rows, 0, [Mesh(old, faces)], [line]
        contact = {
            "owner": 0,
            "cap": Mesh(cached, faces),
            "mask": shapely.box(0, -2, 4, 2),
        }
        result = common_pad(*args, include_interiors=False, constructed=[contact])
        self.assertEqual(len(result), 2)
        self.assertTrue(contact["mask"].covers(result[1][2]))
        for invalid in (
            dict(contact, owner=1),
            dict(contact, mask=shapely.box(0, -2, 0.1, 2)),
        ):
            with (
                self.subTest(invalid=invalid["owner"]),
                self.assertRaisesRegex(ValueError, "pad is incomplete"),
            ):
                common_pad(*args, include_interiors=False, constructed=[invalid])


if __name__ == "__main__":
    unittest.main()
