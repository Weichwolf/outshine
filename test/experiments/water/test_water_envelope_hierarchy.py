"""Page absence, fixed snapshot membership and unchanged envelope results."""

import dataclasses
import unittest

import numpy as np
from compare_canonical_water_domains import DEFAULT
from compare_water_course_envelope import query_point
from compare_water_envelope_hierarchy import compare
from water_course_envelope import course_block, summarize
from water_envelope_fixture import acquire, prepare_parent
from water_envelope_hierarchy import HierarchicalEnvelope
from water_envelope_pages import metadata_pages
from water_envelope_query import EnvelopeIndex


def fixture():
    blocks = {}
    for key, x, height in (
        ((3, 0, 0), 0.0, 100.0),
        ((3, 0, 1), 20.0, 101.0),
        ((3, 7, 7), 10000.0, 200.0),
        ((3, 7, 6), 11000.0, 201.0),
    ):
        blocks[key] = course_block(
            key,
            [[[x, 0, 0], [x + 1, 0, 0]]],
            [[height, height]],
            [str(key)],
            source_version="source-v1",
        )
    summaries = {k: summarize(v) for k, v in blocks.items()}
    root, pages = metadata_pages(tuple(blocks), summaries)
    return blocks, summaries, root, pages


class LazyMetadata(unittest.TestCase):
    def test_uncertain_subtree_after_a_candidate_still_requires_metadata(self):
        blocks, _, _, _ = fixture()
        height = float(np.nextafter(np.nextafter(100.0, np.inf), np.inf))
        for key, x, h in (
            ((3, 7, 7), -10000.0, height),
            ((3, 7, 6), 10000.0, height + 1),
        ):
            blocks[key] = course_block(
                key,
                [[[x, 0, 0], [x + 1, 0, 0]]],
                [[h, h]],
                [str(key)],
                source_version="overhang",
            )
        root, pages = metadata_pages(
            tuple(blocks), {k: summarize(v) for k, v in blocks.items()}
        )
        query = HierarchicalEnvelope(root, pages.get, scope="fixed").query(
            np.zeros(3), blocks.get, metadata_limit=2
        )
        self.assertEqual(query.state, "metadata_budget_exhausted")
        self.assertEqual(len(query.opened), 1)
        self.assertIsNone(query.height_m)
        missing = dict(pages)
        missing.pop(query.reason["failure"])
        query = HierarchicalEnvelope(root, missing.get, scope="fixed").query(
            np.zeros(3), blocks.get
        )
        self.assertEqual(query.state, "pending_metadata")
        self.assertEqual(len(query.opened), 1)
        self.assertIsNone(query.height_m)

    def test_repeat_query_has_no_additional_logical_page_or_payload_demand(self):
        blocks, summaries, root, pages = fixture()
        flat = EnvelopeIndex(tuple(blocks), summaries, scope="fixed")
        rows = compare(
            flat, blocks, [("first", np.zeros(3)), ("repeat", np.zeros(3))], root, pages
        )
        self.assertTrue(rows[0]["added_metadata_pages"])
        self.assertTrue(rows[0]["added_payload_keys"])
        self.assertEqual(rows[1]["added_metadata_pages"], [])
        self.assertEqual(rows[1]["added_payload_keys"], [])

    def test_pruned_metadata_and_payloads_can_both_be_absent(self):
        blocks, summaries, root, pages = fixture()
        full = HierarchicalEnvelope(root, pages.get, scope="fixed").query(
            np.zeros(3), blocks.get
        )
        read = full.reason["metadata_pages"]
        self.assertLess(len(read), len(pages))
        sparse_pages = {k: pages[k] for k in read}
        sparse_payloads = {k: blocks[k] for k in full.opened}
        index = HierarchicalEnvelope(root, sparse_pages.get, scope="fixed")
        result = index.query(np.zeros(3), sparse_payloads.get)
        flat = EnvelopeIndex(tuple(blocks), summaries, scope="fixed").query(
            np.zeros(3), blocks.get
        )
        self.assertEqual(
            (result.height_m, result.winner, result.parameter),
            (flat.height_m, flat.winner, flat.parameter),
        )
        self.assertEqual(result, full)

    def test_missing_or_stale_needed_page_never_returns_best_so_far(self):
        blocks, _, root, pages = fixture()
        missing = HierarchicalEnvelope(root, lambda key: None, scope="fixed").query(
            np.zeros(3), blocks.get
        )
        self.assertEqual(missing.state, "pending_metadata")
        self.assertIsNone(missing.height_m)
        page = list(pages[root.bound.key])
        page[0] = dataclasses.replace(page[0], source_cells=page[0].source_cells + 1)
        stale = HierarchicalEnvelope(
            root, lambda key: tuple(page), scope="fixed"
        ).query(np.zeros(3), blocks.get)
        self.assertEqual(stale.state, "stale_metadata")
        self.assertIsNone(stale.height_m)

    def test_root_or_semantic_incompleteness_stops_before_callbacks(self):
        blocks, summaries, _, _ = fixture()
        summaries.pop((3, 7, 7))
        root, _ = metadata_pages(tuple(blocks), summaries)

        def forbidden(key):
            self.fail("data accessed for an unknown snapshot")

        self.assertEqual(
            HierarchicalEnvelope(root, forbidden, scope="fixed")
            .query(np.zeros(3), forbidden)
            .state,
            "pending_metadata",
        )
        self.assertEqual(
            HierarchicalEnvelope(None, forbidden, scope="fixed")
            .query(np.zeros(3), forbidden)
            .state,
            "pending_metadata",
        )

    def test_confirmed_empty_root_needs_no_pages_or_payloads(self):
        blocks = {
            key: course_block(key, [], [], [], source_version="empty")
            for key in ((3, 0, 0), (3, 7, 7))
        }
        root, _ = metadata_pages(
            tuple(blocks), {k: summarize(v) for k, v in blocks.items()}
        )

        def forbidden(key):
            self.fail("empty guide requested data")

        result = HierarchicalEnvelope(root, forbidden, scope="empty").query(
            np.zeros(3), forbidden
        )
        self.assertEqual(result.state, "empty_guide")
        self.assertEqual(result.reason["empty_source_cells"], 2)

    def test_metadata_budget_and_needed_payload_refuse_instead_of_falling_back(self):
        blocks, _, root, pages = fixture()
        index = HierarchicalEnvelope(root, pages.get, scope="fixed")
        result = index.query(np.zeros(3), blocks.get, metadata_limit=1)
        self.assertEqual(result.state, "metadata_budget_exhausted")
        self.assertIsNone(result.height_m)
        result = index.query(np.zeros(3), lambda key: None)
        self.assertEqual(result.state, "pending_payload")
        self.assertIsNone(result.height_m)

    def test_producer_order_and_source_membership_are_explicit(self):
        blocks, summaries, root, pages = fixture()
        other, other_pages = metadata_pages(
            tuple(reversed(tuple(blocks))), dict(reversed(tuple(summaries.items())))
        )
        self.assertEqual(root, other)
        self.assertEqual(pages, other_pages)
        smaller, _ = metadata_pages(tuple(blocks)[:-1], summaries)
        a = HierarchicalEnvelope(root, pages.get, scope="fixed")
        b = HierarchicalEnvelope(smaller, pages.get, scope="fixed")
        self.assertNotEqual(a.version, b.version)


class OriginalPages(unittest.TestCase):
    def test_real_source_parent_matches_flat_queries_with_bounded_frontier(self):
        data, snapshots, _, receipts, _ = acquire(DEFAULT)
        parent, _ = prepare_parent(data, snapshots, receipts)
        root, pages = metadata_pages(parent.index.expected, parent.index.summaries)
        index = HierarchicalEnvelope(root, pages.get, scope="fixed")
        for key in (
            (14, 17257, 11493),
            (14, 17257, 11494),
            (14, 17258, 11494),
            (14, 17258, 11495),
        ):
            q = query_point(key)
            flat = parent.index.query(q, parent.blocks.get)
            result = index.query(q, parent.blocks.get)
            self.assertEqual(
                (result.height_m, result.winner, result.parameter),
                (flat.height_m, flat.winner, flat.parameter),
            )
            self.assertLessEqual(
                result.reason["frontier_peak"],
                1 + 3 * len(result.reason["metadata_pages"]),
            )


if __name__ == "__main__":
    unittest.main()
