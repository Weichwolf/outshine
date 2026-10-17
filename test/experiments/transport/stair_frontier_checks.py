"""Discrete frontier controls, actual cached roots, T pads and whole headroom."""

import copy
import json
import unittest
from pathlib import Path

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_frontier_dispositions import source_dispositions
from stair_landing_pads import complete_landings
from stair_module_audit import traffic
from stair_module_clearance import source_clearances
from stair_module_inputs import FIXTURE, inputs
from stair_module_node_heights import plan_heights
from stair_module_roots import bind_root_pad, root_profile
from stair_southwest_checks import cached_reference


class ActualInteriorRoot(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _r, _rows, _context, cls.sources, _sampler, _layer = inputs(FIXTURE)
        cls.state = cached_reference()
        cls.key = next(iter(cls.state["cached_module_rows"]))

    def test_actual_original_interior_cap_is_sampled_without_outer_port(self):
        recipe = {
            "port": {"source_id": self.key, "vertex": 6},
            "contact_mode": "profile",
        }
        row, line = root_profile(self.state, recipe, self.sources)
        self.assertTrue(row["root_interior"])
        np.testing.assert_array_equal(line[1, :2], self.sources[self.key]["xy_m"][6])
        self.assertTrue(np.isfinite(line).all())

    def test_interior_root_requires_original_axis_and_explicit_family(self):
        recipe = {"port": {"source_id": self.key, "vertex": 6}, "contact_mode": "flat"}
        with self.assertRaises(ValueError):
            root_profile(self.state, recipe, self.sources)
        bad = copy.deepcopy(self.state)
        bad["cached_module_rows"][self.key]["xy"][6][0] += 0.001
        with self.assertRaises(ValueError):
            root_profile(bad, dict(recipe, contact_mode="profile"), self.sources)


class PhysicalAttemptDispositions(unittest.TestCase):
    def test_real_fixed_face_fails_whole_gradient_despite_passing_every_edge(self):
        path = Path(__file__).parent / "fixtures/stair-prefix-fixed-face-vienna.json"
        counter = json.loads(path.read_text())
        xyz = np.array(counter["fixed_vertices_enu_m"])
        frame = np.array(counter["grade_frame"])
        for a, b in ((0, 1), (1, 2), (2, 0)):
            maximum = np.linalg.norm(np.linalg.solve(frame.T, xyz[b, :2] - xyz[a, :2]))
            self.assertLessEqual(abs(xyz[b, 2] - xyz[a, 2]), maximum)
        normal = np.cross(xyz[1] - xyz[0], xyz[2] - xyz[0])
        whole = np.linalg.norm(frame @ (-normal[:2] / normal[2]))
        self.assertAlmostEqual(whole, 3.0342389995478083, places=7)
        self.assertGreater(whole, 1)

    def test_joint_failure_is_not_an_individual_impossibility_and_keeps_others(self):
        plan = {"proposals": [{"source_ids": ["a", "b"]}, {"source_ids": ["c"]}]}
        attempts = [
            {"index": 0, "source_ids": ["a", "b"], "reason": "joint_contact_conflict"},
            {"index": 1, "source_ids": ["a"], "reason": ""},
        ]
        result = source_dispositions(plan, attempts, 1)
        self.assertTrue(result["a"]["candidate_geometry"])
        self.assertEqual(result["a"]["attempt_indices"], [0, 1])
        self.assertFalse(result["b"]["candidate_geometry"])
        self.assertIn("retained_for_later", result["b"]["reason"])
        self.assertEqual(result["b"]["attempt_indices"], [0])
        self.assertEqual(result["c"]["reason"], "outside_bounded_geometry_attempts")
        self.assertFalse(
            any(
                r["candidate_geometry"]
                for r in source_dispositions(plan, [], None).values()
            )
        )


class SourceJunctionsAndClearance(unittest.TestCase):
    def test_root_pad_supports_frozen_through_and_branch_profiles_together(self):
        rows = [
            {"xy": np.array([[-20, 0], [0, 0], [20, 0]], float), "width_m": 2.0},
            {"xy": np.array([[0, 0], [0, 20]], float), "width_m": 1.8},
        ]
        node = {"incident": [0, 1], "xy": [0, 0]}
        line = np.c_[np.array([[-1, 1], [0, 0], [1, -1]]) / np.sqrt(2), np.zeros(3)]
        bind_root_pad(rows, node, line, include_interiors=True)
        required = [
            shapely.LineString(line[:, :2]),
            shapely.LineString([[0, -1], [0, 1]]),
            shapely.LineString([[-0.9, 0], [0.9, 0]]),
        ]
        for row in rows:
            self.assertEqual(len(row["junction_pads_xy_m"]), 1)
            pad = shapely.Polygon(row["junction_pads_xy_m"][0])
            self.assertTrue(all(pad.covers(p) for p in required))
        self.assertEqual(rows[0]["junction_pad_vertices"], [1])

    def test_t_junction_uses_actual_internal_profile_and_old_default_stays(self):
        rows = [
            {
                "source_id": str(i),
                "xy": np.array(xy, float),
                "prior": np.zeros(len(xy)),
                "width_m": 2.0,
                "original": {
                    "bridge": False,
                    "source": {
                        "feature": {
                            "properties": {"class": "path", "subclass": "footway"}
                        }
                    },
                },
            }
            for i, xy in enumerate(([[-20, 0], [0, 0], [20, 0]], [[0, 0], [0, 20]]))
        ]
        nodes = [{"incident": [0, 1], "mvt_xy": [0, 0], "xy": [0, 0]}]
        self.assertFalse(complete_landings(copy.deepcopy(rows), nodes, [])[0])
        pads, _ledger = complete_landings(rows, nodes, [], include_interiors=True)
        self.assertEqual(len(pads), 1)
        self.assertEqual(rows[0]["junction_pad_vertices"], [1])
        self.assertEqual(len(plan_heights(rows, nodes, [], include_interiors=True)), 1)

    def test_actual_headroom_is_checked_for_each_declared_profile(self):
        low = np.array([[0, 0, 0], [4, 0, 0], [4, 4, 0], [0, 4, 0]], float)
        faces = np.array([[0, 1, 2], [0, 2, 3]])
        data = {
            "surface_vertices": low,
            "surface_faces": faces,
            "owners": np.ones((2, 1), bool),
        }
        upper = Mesh(low + [0, 0, 3], faces)
        row = {
            "source_id": "foot",
            "original": {
                "source": {"feature": {"properties": {"subclass": "footway"}}}
            },
        }
        footprint = shapely.box(0, 0, 4, 4)
        proof = source_clearances(data, [row], [footprint], [], upper)
        self.assertTrue(all(p["sources"][0]["minimum_clearance_m"] == 3 for p in proof))
        with self.assertRaisesRegex(ValueError, "headroom below declared profile"):
            source_clearances(
                data,
                [dict(row, traffic_profile={"clearance_m": 4.5})],
                [footprint],
                [],
                upper,
            )
        with self.assertRaisesRegex(ValueError, "intersects an existing bridge"):
            traffic(data, upper, [])


if __name__ == "__main__":
    unittest.main()
