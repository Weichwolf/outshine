"""Positive projected overlap and mesh preconditions never silently admit a contact."""

import tempfile
import unittest
from pathlib import Path

import numpy as np
import shapely
from stair_local_assets import road_asset
from stair_local_checks import asset
from stair_overlap_probe import cap_properties, inspect_pairs


def reference(*, ground=True, slope=0.0):
    old = asset(0)
    vertices = old.data["body_vertices"].copy()
    vertices[:4, 2] = 1 + vertices[:4, 0] * slope
    vertices[4:, 2] = vertices[:4, 2] - 0.3
    data = dict(old.data, body_vertices=vertices, ground=np.full(2, ground))
    return road_asset(old.identity, old.source_ids, old.rows, data)


class OverlapCandidates(unittest.TestCase):
    def test_positive_mask_without_original_node_is_only_a_candidate(self):
        old = reference()
        before = old.data["body_vertices"].tobytes()
        sources = {
            "new": {"mvt_xy": [[0, 1], [2, 1]], "properties": {"subclass": "footway"}},
            "0:0": {"mvt_xy": [[0, 0], [2, 0]], "properties": {"class": "minor"}},
        }
        with tempfile.TemporaryDirectory(
            prefix="outshine-transport-overlap-", dir="/tmp"
        ) as tmp:
            rows = [{"source_id": "new", "gradient": 0.1}]
            result, work = inspect_pairs(
                rows, [shapely.box(0.5, 0.5, 1.5, 1.5)], [old], sources, Path(tmp)
            )
            self.assertEqual(len(result), 1)
            self.assertEqual(result[0]["shared_original_nodes"], [])
            self.assertFalse(result[0]["admitted_contact"])
            self.assertEqual(result[0]["area_m2"], 1)
            self.assertTrue(result[0]["preconditions"]["old_ground_faces"])
            self.assertTrue(result[0]["preconditions"]["old_depth_exactly_0_3m"])
            self.assertEqual(work["asset_queries"], 1)
            self.assertEqual(old.data["body_vertices"].tobytes(), before)
            result, _work = inspect_pairs(
                rows, [shapely.box(10, 10, 11, 11)], [old], sources, Path(tmp)
            )
            self.assertEqual(result, [])

    def test_projected_bridge_overlap_retains_non_ground_precondition(self):
        props, _top, _bottom = cap_properties(
            reference(ground=False), np.array([0, 1]), 0.1
        )
        self.assertFalse(props["old_ground_faces"])

    def test_whole_old_facet_gradient_is_checked_in_both_precisions(self):
        props, _top, _bottom = cap_properties(
            reference(slope=0.2), np.array([0, 1]), 0.1
        )
        self.assertEqual(len(props["old_cap_gradient_checks"]), 2)
        self.assertTrue(
            all(not c["isotropic_bound_met"] for c in props["old_cap_gradient_checks"])
        )


if __name__ == "__main__":
    unittest.main()
