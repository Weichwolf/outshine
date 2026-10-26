"""World identity and physical validity are independent generation requirements."""

import dataclasses
import unittest

import numpy as np
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_water_owner_inputs import layer_receipts
from outshine_ref.contracts import Mesh
from physical_boundary import to_world
from test_water_owner_inputs import EPOCH, receipts
from water_contact_band import physical_audit
from water_nodal_height import (
    audit_face,
    cell_faces,
    evaluate_node,
    node_plan,
    parameter_triangle,
    sample_face,
)


class NodalIdentity(unittest.TestCase):
    def test_four_cells_use_identical_corner_owner_and_bounded_receipts(self):
        corner = (4, 12, 12)
        for cell in ((4, 5, 5), (4, 6, 5), (4, 5, 6), (4, 6, 6)):
            self.assertTrue(any(corner in keys for keys in cell_faces(cell)))
        first = node_plan(corner, receipts(), EPOCH)
        fixed = {key: receipts()[key] for key in reversed(first.required)}
        self.assertEqual(first, node_plan(corner, fixed, EPOCH))
        self.assertEqual(first.work["receipt_lookups"], 18)
        self.assertEqual(len(cell_faces((4, 5, 5))), 8)

    def test_failed_profile_receipt_never_becomes_a_ready_node(self):
        items = receipts()
        key = (4, 12, 12)
        plan = node_plan(key, items, EPOCH)
        missing = plan.required[-1]
        items[missing] = dataclasses.replace(items[missing], complete=False)
        node = evaluate_node(key, items, EPOCH, {}, {}, {})
        self.assertEqual(node.state, "pending_sources")
        self.assertIsNone(node.world)
        self.assertIsNone(node.level_m)

    def test_affine_world_field_is_not_re_lifted_in_consumer_frames(self):
        keys = cell_faces((4, 5, 5))[0]
        world = to_world(
            np.array([9.6, 47.2]), np.array([[0, 0, 500], [10, 10, 501], [10, 0, 500]])
        )
        xy = parameter_triangle(keys)
        query = np.vstack([xy, xy.mean(axis=0)])
        samples = sample_face(keys, world, query)
        np.testing.assert_array_equal(samples[:3], world)
        np.testing.assert_allclose(samples[3], world.mean(axis=0), rtol=0, atol=1e-9)
        with self.assertRaisesRegex(ValueError, "outside canonical facet"):
            sample_face(keys, world, xy[:1] - 5)

    def test_individually_flat_nodal_priors_can_make_an_invalid_shared_facet(self):
        anchor = np.array([9.6, 47.2])
        faces = np.array([[0, 1, 2]], np.uint32)
        xy = np.array([[0, 0], [100, 0], [0, 100]], dtype=float)
        rows = np.array([[0, 1, 0, 0, -0.01, 0.01], [0, 0, 1, 0, -0.01, 0.01]])
        for level in (500, 502):
            mesh = Mesh(np.c_[xy, np.full(3, level)], faces)
            self.assertTrue(physical_audit(mesh, anchor, rows, (480, 522))["admitted"])
        shared = Mesh(np.c_[xy, [500, 502, 500]], faces)
        result = physical_audit(shared, anchor, rows, (480, 522))
        self.assertFalse(result["admitted"])
        self.assertGreater(result["whole_face_envelope_violation"], 0.009)


class OriginalRiverCounterexample(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data, raw, _ = observed_snapshots(DEFAULT)
        cls.snapshots = {
            t: dataclasses.replace(s, complete=True) for t, s in raw.items()
        }
        cls.epoch = tuple(
            next(iter(raw.values())).source[k] for k in ("provider", "revision")
        )
        cls.receipts, cls.order = layer_receipts(cls.data, cls.snapshots, cls.epoch)

    def test_source_owned_ecef_targets_replay_but_do_not_pass_physical_admission(self):
        keys = ((14, 17257, 11493), (14, 17257, 11494), (14, 17258, 11494))
        nodes = {}
        for key in keys:
            first = evaluate_node(
                key, self.receipts, self.epoch, self.snapshots, self.data, self.order
            )
            fixed = {k: self.receipts[k] for k in reversed(first.plan.required)}
            second = evaluate_node(
                key, fixed, self.epoch, self.snapshots, self.data, self.order
            )
            self.assertEqual(first.state, "ready")
            np.testing.assert_array_equal(first.world, second.world)
            self.assertTrue(first.courses)
            nodes[key] = first
        result, payload = audit_face(keys, nodes, self.data)
        self.assertEqual(result["state"], "physical_rejection")
        self.assertGreater(result["physical"]["whole_face_envelope_violation"], 0.003)
        self.assertEqual(result["physical"]["height_box_violation_m"], 0)
        self.assertGreater(result["sampled"]["physical_grade_violation"], 0.003)
        self.assertEqual(payload["world"].shape, (3, 3))


if __name__ == "__main__":
    unittest.main()
