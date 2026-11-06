import math
import struct
import unittest
from itertools import product

import numpy as np

from field_group_selection import FieldGroups, enu_rows
from field_panoramic_geometry import panoramic_bound
from field_view_projection import CapturePlane
from source_product_cut import SourceProducts
from test_field_pose_geometry import perspective
from test_source_product_cut import index


class FieldGroupTests(unittest.TestCase):
    def groups(self, tree=None, eye=(15, 1, 1000), axes=None):
        tree = tree or SourceProducts(*index(), revision="fixture")
        return FieldGroups(tree, (0, 0, 0), eye, np.eye(3) if axes is None else axes,
                           720, (1280, 720), 1)

    def test_panorama_refines_a_centred_candidate_without_losing_sources(self):
        model = self.groups()
        old, _ = model.tree.cut(model.eye, 720, 1)
        new, _ = model.cut()
        self.assertEqual(len(old), 1)
        self.assertIsNone(model.candidate(0))
        self.assertGreater(len(new), 1)
        self.assertTrue(all(p.kind == "field" for p in new))
        self.assertEqual(model.costs(old, [1] * 4)["eligible_sources"], 0)
        self.assertEqual(model.costs(new, [1] * 4, verify=True)["eligible_sources"], 4)

    def test_one_near_source_blocks_far_sources_only_with_cell_admission(self):
        nodes = [(0, 0, 0, 2, 2, 1992, 0, 2, 1, 2),
                 (0, 0, 0, 2, 2, 2, 0, 1, 0, 0),
                 (0, 0, 1990, 2, 2, 1992, 1, 1, 0, 0)]
        data = struct.pack("<QII", 0x3158444945474150, 3, 2)
        data += b"".join(struct.pack("<6d4I", *node) for node in nodes)
        data += struct.pack("<2I", 0, 1)
        tree = SourceProducts(data, [1, 1], bytes([1, 1]), revision="mixed")
        model = self.groups(tree=tree, eye=(1, 1, 2000))
        cut, _ = model.cut()
        counts = model.costs(cut, [1, 1], verify=True)
        self.assertEqual([p.kind for p in cut], ["field", "geometry"])
        self.assertEqual(counts["cell_blocked_eligible_sources"], 1)
        self.assertEqual(counts["residual_sources"], 1)
        self.assertEqual(counts["cell_admitted_sources"], 0)

    def test_unknown_sources_and_zero_error_remain_explicit(self):
        data, cells, _ = index()
        tree = SourceProducts(data, cells, bytes([1, 0, 1, 1]), revision="unknown")
        model = self.groups(tree=tree, eye=(15, 1, 100000))
        cut, _ = model.cut()
        self.assertEqual(sum(p.kind == "unresolved" for p in cut), 1)
        self.assertEqual(model.costs(cut, cells, verify=True)["residual_sources"], 1)
        model.allowed = 0
        self.assertTrue(all(p.kind != "field" for p in model.cut()[0]))

    def test_approaching_a_town_refines_without_dropping_buildings(self):
        counts = []
        for distance in (100000, 3000, 1000, 10):
            model = self.groups(eye=(15, 1, distance))
            cut, _ = model.cut()
            model.costs(cut, [1] * 4, verify=True)
            counts.append(len(cut))
            if distance == 10:
                self.assertTrue(all(p.kind == "geometry" for p in cut))
        self.assertEqual(counts[0], 1)
        self.assertEqual(counts[-1], 4)
        self.assertEqual(counts, sorted(counts))

    def test_transformed_leaf_bounds_union_matches_the_source_view(self):
        axes = enu_rows((4000000, 3000000, 4000000))
        model = self.groups(axes=axes)
        corners = np.array([corner for node in model.tree.nodes if node[7] == 1
                            for corner in product(*zip(node[:3], node[3:6]))]) @ axes.T
        np.testing.assert_allclose(model.local[0], np.r_[corners.min(0), corners.max(0)])

    def test_float_model_agrees_with_existing_interval_reference(self):
        model = self.groups(eye=(15, 1, 3000))
        for at in range(len(model.tree.nodes)):
            captured = model.capture(at)
            self.assertIsNotNone(captured)
            _, half, direction, rays, support = captured
            centre = support.mean(axis=0)
            lower, upper = rays.min(0), rays.max(0)
            plane = CapturePlane(lower, upper, centre, direction)
            hull = tuple(tuple((float(v), float(v)) for v in point) for point in rays)
            bound = panoramic_bound(plane, (0, 0, 0), perspective(focal=720),
                                    (1280, 720), source_hull=hull)
            candidate = model.candidate(at)
            self.assertIsNotNone(candidate)
            self.assertGreaterEqual(bound["screen_l1_upper_px"], candidate.error_px)
            self.assertTrue(math.isclose(bound["screen_l1_upper_px"],
                                         candidate.error_px, rel_tol=1e-7))

    def test_invalid_frames_and_budgets_do_not_create_candidates(self):
        for axes in (np.zeros((3, 3)), np.diag((-1, 1, 1))):
            with self.assertRaises(ValueError):
                self.groups(axes=axes)
        with self.assertRaises(ValueError):
            enu_rows((0, 0, 0))


if __name__ == "__main__":
    unittest.main()
