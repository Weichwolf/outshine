"""Whole finite sidewalk joins and exclusion counterexamples."""

import unittest

import numpy as np
import shapely
from bridge_support_contacts import ContactSurface
from curb_experiment import builder
from outshine_ref.contracts import Mesh
from sidewalk_geometry import audit, joint_error
from sidewalk_plan import select


class Sidewalks(unittest.TestCase):
    def plane(self):
        xy = np.array([[-20, -20], [20, -20], [20, 20], [-20, 20]])
        return Mesh(np.c_[xy, xy[:, 0] * 0.01], [[0, 1, 2], [0, 2, 3]])

    def proposals(self, method="combined"):
        traffic = shapely.box(-10, -2, 10, 2)
        shapes = [shapely.box(-8, 2, -1, 2.18), shapely.box(1, 2, 8, 2.18)]
        excluded = shapely.union_all(
            [shapely.box(-1, -10, 1, 10), shapely.box(-20, -20, -8, 20)]
        )
        world = shapely.box(-20, -20, 20, 20)
        rows, blocked, outside, _ = select(
            shapes,
            [{"sources": ["west"]}, {"sources": ["east"]}],
            traffic,
            world,
            excluded,
            method,
        )
        return rows, blocked, outside, traffic, world, shapes

    def test_whole_profile_leaves_traffic_crossing_curb_and_port_free(self):
        rows, blocked, outside, _traffic, _world, shapes = self.proposals()
        self.assertEqual(len(rows), 2)
        for row in rows:
            self.assertFalse(row["reason"])
            self.assertGreater(row["joint_length_m"], 6)
            self.assertLess(row["shape"].intersection(blocked).area, 1e-10)
            self.assertLess(row["shape"].difference(outside).area, 1e-10)
            self.assertEqual(len(row["curb_products"]), 1)
            i = row["curb_products"][0]
            self.assertEqual(row["sources"], [["west"], ["east"]][i])
            self.assertGreater(
                row["shape"].boundary.intersection(shapes[i].boundary).length, 6
            )

    def test_alternative_masks_preserve_profile_and_excluded_crossing(self):
        left = self.proposals("combined")[0]
        right = self.proposals("individual")[0]
        a = shapely.union_all([r["shape"] for r in left])
        b = shapely.union_all([r["shape"] for r in right])
        self.assertLess(a.symmetric_difference(b).area, 1e-10)
        self.assertEqual([r["sources"] for r in left], [r["sources"] for r in right])

    def test_actual_closed_sidewalk_joins_whole_sloping_curb_caps(self):
        rows, blocked, outside, traffic, world, shapes = self.proposals()
        ground = self.plane()
        source = builder(ground)
        curbs = [source.build(s, minimum_height_m=0.1199999)[0] for s in shapes]
        context = ContactSurface(ground), traffic, world, blocked, outside
        for row in rows:
            product, note = source.build(row["shape"], minimum_height_m=0.1199999)
            self.assertIsNotNone(product, note)
            proof = audit(product, row, curbs, shapes, context)
            self.assertLess(proof["maximum_joint_height_error_m"], 1e-10)
            self.assertGreater(proof["joint_length_m"], 6)

    def test_joint_endpoints_alone_do_not_hide_interior_height_peak(self):
        left = Mesh(
            [[0, -1, 0.12], [4, -1, 0.12], [4, 0, 0.12], [2, 0, 0.2], [0, 0, 0.12]],
            [[0, 1, 3], [1, 2, 3], [0, 3, 4]],
        )
        right = Mesh(
            [[0, 0, 0.12], [4, 0, 0.12], [4, 1, 0.12], [0, 1, 0.12]],
            [[0, 1, 2], [0, 2, 3]],
        )
        with self.assertRaisesRegex(ValueError, "joint height mismatch"):
            joint_error(left, right, shapely.LineString([[0, 0], [4, 0]]))


if __name__ == "__main__":
    unittest.main()
