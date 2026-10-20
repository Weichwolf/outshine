"""Independent arithmetic, selection-before-payload and original physical checks."""

import dataclasses
import decimal
import unittest

import numpy as np
from compare_canonical_water_domains import DEFAULT
from water_colored_reference import ReferenceUpdates
from water_course_envelope import (
    block_lower_bound,
    block_minimum,
    candidate_upper,
    course_block,
    summarize,
)
from water_envelope_fixture import acquire, prepare_parent
from water_envelope_query import EnvelopeIndex
from water_nodal_height import audit_face


def block(key, x, height, identity):
    return course_block(
        key,
        [[[x, 0, 0], [x + 10, 0, 0]]],
        [[height, height]],
        [identity],
        source_version="source-v1",
    )


class EnvelopeArithmetic(unittest.TestCase):
    def test_interior_minimum_steep_endpoint_and_degenerate_control(self):
        flat = block((1, 0, 0), 0, 100.0, "flat")
        value, identity, parameter = block_minimum(flat, np.array([5.0, 3.0, 0.0]), 0.1)
        self.assertEqual(identity, "flat")
        self.assertAlmostEqual(value, 100.3, places=12)
        self.assertAlmostEqual(parameter, 0.5, places=12)
        tilted = course_block(
            (1, 0, 0), flat.segments, [[100.0, 100.5]], ["tilted"], source_version="s"
        )
        self.assertAlmostEqual(
            block_minimum(tilted, np.array([5.0, 3.0, 0.0]), 0.1)[0],
            100.25 + 3 * np.sqrt(0.1**2 - 0.05**2),
            places=11,
        )
        steep = course_block(
            (1, 0, 0), flat.segments, [[100.0, 120.0]], ["steep"], source_version="s"
        )
        self.assertEqual(block_minimum(steep, np.array([5.0, 3.0, 0.0]), 0.1)[2], 0.0)
        point = course_block(
            (1, 0, 0),
            [[[0, 0, 0], [0, 0, 0]]],
            [[100.0, 99.0]],
            ["point"],
            source_version="s",
        )
        self.assertAlmostEqual(
            block_minimum(point, np.zeros(3), 0.1)[0], 99.0, places=12
        )

    def test_outward_bounds_against_high_precision_stored_coordinate_model(self):
        rng = np.random.default_rng(23)
        with decimal.localcontext() as context:
            context.prec = 70
            D = decimal.Decimal.from_float
            for _ in range(64):
                a = rng.uniform(-6.4e6, 6.4e6, 3)
                b = a + rng.normal(size=3) * 100
                q = a + rng.normal(size=3) * 200
                levels = rng.uniform(100.0, 700.0, 2)
                t = float(rng.uniform())
                value = course_block(
                    (1, 0, 0), [[a, b]], [levels], ["s"], source_version="s"
                )
                true_point = [
                    D(float(a[i])) + D(t) * (D(float(b[i])) - D(float(a[i])))
                    for i in range(3)
                ]
                distance = sum(
                    (D(float(q[i])) - true_point[i]) ** 2 for i in range(3)
                ).sqrt()
                exact = (
                    D(float(levels[0]))
                    + D(t) * (D(float(levels[1])) - D(float(levels[0])))
                    + D(0.005) * distance
                )
                upper = candidate_upper(value, q, 0.005, np.array([t]))[0]
                self.assertGreaterEqual(D(float(upper)), exact)
                summary = summarize(value)
                low, high = summary.bounds
                delta = [
                    max(
                        decimal.Decimal(0),
                        D(float(low[i])) - D(float(q[i])),
                        D(float(q[i])) - D(float(high[i])),
                    )
                    for i in range(3)
                ]
                bound = (
                    D(float(levels.min())) + D(0.005) * sum(d * d for d in delta).sqrt()
                )
                self.assertLessEqual(D(block_lower_bound(summary, q, 0.005)), bound)


class MetadataSelection(unittest.TestCase):
    def setUp(self):
        self.blocks = {
            (1, 0, 0): block((1, 0, 0), 0, 100.0, "near"),
            (1, 1, 0): block((1, 1, 0), 10000, 200.0, "far"),
        }
        self.metadata = {key: summarize(value) for key, value in self.blocks.items()}
        self.index = EnvelopeIndex(
            tuple(self.blocks), self.metadata, scope="fixed-source-snapshot"
        )

    def test_excluded_payload_can_be_absent_and_residence_order_does_not_change_value(
        self,
    ):
        calls = []

        def load(key):
            calls.append(key)
            return self.blocks.get(key) if key == (1, 0, 0) else None

        result = self.index.query(np.zeros(3), load)
        self.assertEqual(result.state, "ready")
        self.assertEqual(calls, [(1, 0, 0)])
        self.assertEqual(result.excluded[0][0], (1, 1, 0))
        reversed_index = EnvelopeIndex(
            tuple(reversed(tuple(self.blocks))),
            dict(reversed(tuple(self.metadata.items()))),
            scope="fixed-source-snapshot",
        )
        self.assertEqual(self.index.version, reversed_index.version)
        self.assertEqual(result, reversed_index.query(np.zeros(3), load))

    def test_missing_metadata_failed_empty_and_needed_payload_are_not_empty_guides(
        self,
    ):
        missing = dict(self.metadata)
        missing.pop((1, 1, 0))
        index = EnvelopeIndex(
            tuple(self.blocks), missing, scope="fixed-source-snapshot"
        )

        def forbidden(key):
            self.fail("payload work before complete metadata")

        self.assertEqual(index.query(np.zeros(3), forbidden).state, "pending_metadata")
        missing[(1, 1, 0)] = dataclasses.replace(
            self.metadata[(1, 1, 0)],
            count=0,
            bounds=None,
            minimum_level_m=None,
            complete=False,
        )
        self.assertEqual(
            EnvelopeIndex(tuple(self.blocks), missing, scope="s")
            .query(np.zeros(3), forbidden)
            .state,
            "pending_metadata",
        )
        self.assertEqual(
            self.index.query(np.zeros(3), lambda key: None).state, "pending_payload"
        )

    def test_stale_payload_and_exhausted_budget_never_return_an_alternative_height(
        self,
    ):
        stale = block((1, 0, 0), 0, 90.0, "near")
        self.assertEqual(
            self.index.query(np.zeros(3), lambda key: stale).state, "stale_payload"
        )
        # Equal controls have equal lower bounds; canonical tie ownership needs both.
        second = block((1, 1, 0), 0, 100.0, "alias")
        sources = {(1, 0, 0): self.blocks[(1, 0, 0)], (1, 1, 0): second}
        index = EnvelopeIndex(
            tuple(sources), {k: summarize(v) for k, v in sources.items()}, scope="ties"
        )
        result = index.query(np.zeros(3), sources.get, payload_limit=1)
        self.assertEqual(result.state, "budget_exhausted")
        self.assertIsNone(result.height_m)

    def test_confirmed_empty_source_layer_is_distinct_from_unknown(self):
        empty = course_block((1, 0, 0), [], [], [], source_version="empty-v1")
        index = EnvelopeIndex(
            (empty.key,), {empty.key: summarize(empty)}, scope="declared-empty"
        )
        result = index.query(
            np.zeros(3), lambda key: self.fail("empty payload requested")
        )
        self.assertEqual(result.state, "empty_guide")
        self.assertIsNone(result.height_m)


class OriginalSourceParent(unittest.TestCase):
    def test_source_parent_replaces_the_manual_level_and_preserves_both_physical_repairs(
        self,
    ):
        data, snapshots, epoch, receipts, order = acquire(DEFAULT)
        parent, _ = prepare_parent(data, snapshots, receipts)
        recipe = ReferenceUpdates(
            receipts,
            epoch,
            snapshots,
            data,
            order,
            parent=parent,
            parent_version=parent.version,
        )
        self.assertIsNone(recipe.parent_level_m)
        faces = (
            ((14, 17257, 11493), (14, 17257, 11494), (14, 17258, 11494)),
            ((14, 17257, 11494), (14, 17258, 11495), (14, 17258, 11494)),
        )
        for face in faces:
            original = {p: recipe.target(p) for p in face}
            nodes = {
                p: dataclasses.replace(original[p], world=parent(p).world) for p in face
            }
            self.assertEqual(audit_face(face, nodes, data)[0]["state"], "admitted")
            self.assertTrue(all(recipe.resolve(p).state == "ready" for p in face))
        first = parent(faces[0][0])
        # Unknown coarse inputs remain unknown; no laboratory or sea-level fallback.
        empty_parent = type(parent)(
            EnvelopeIndex(parent.index.expected, {}, scope="unknown"), {}
        )
        self.assertEqual(empty_parent(faces[0][0]).state, "pending_metadata")
        self.assertIsNone(empty_parent(faces[0][0]).world)
        pending_recipe = ReferenceUpdates(
            receipts,
            epoch,
            snapshots,
            data,
            order,
            parent=empty_parent,
            parent_version=empty_parent.version,
        )
        self.assertEqual(pending_recipe.resolve(faces[0][2]).state, "pending_metadata")
        self.assertFalse(pending_recipe.targets)
        self.assertIsNotNone(first.world)


if __name__ == "__main__":
    unittest.main()
