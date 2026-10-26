"""Closed delivered cells retain buffered originals without inventing receipts."""

import dataclasses
import unittest

import shapely
from canonical_water_domains import Piece, Snapshot, resolve
from owned_water_cells import normalize_cell
from test_water_domains import INPUT, SOURCE
from water_body_domains import assemble


def cell(tile, *polygons, complete=True):
    return Snapshot(
        tile,
        dict(SOURCE),
        "delivered:" + str(tile),
        complete,
        tuple(Piece(9, i, p, dict(INPUT)) for i, p in enumerate(polygons)),
    )


def normalize(snapshot, **kwargs):
    return normalize_cell(snapshot, scope="delivered-test-layer-v1", **kwargs)


def resolve_cells(seed, cells):
    return resolve(
        seed,
        cells,
        epoch=(SOURCE["provider"], SOURCE["revision"]),
        support=0,
        model_revision="owned-test-v1",
        assume_ground=True,
    )


class OwnedCells(unittest.TestCase):
    def test_large_raw_buffer_is_retained_but_not_neighbor_delivery(self):
        original = cell((4, 5, 5), shapely.box(1, 1, 12, 12))
        raw_wkb = original.pieces[0].polygon.wkb
        result = normalize(original)
        self.assertIs(result.original, original)
        self.assertEqual(result.original.pieces[0].polygon.wkb, raw_wkb)
        self.assertEqual(result.snapshot.pieces[0].polygon.bounds, (5, 5, 6, 6))
        self.assertAlmostEqual(result.ledger[0]["area_balance_error"], 0)
        self.assertAlmostEqual(sum(p.area for p in result.outside.values()), 120)
        self.assertEqual(result.work["manufactured_neighbors"], 0)
        pending = resolve_cells((4, 5, 5, 9, 0), {(4, 5, 5): result.snapshot})
        self.assertEqual(pending.state, "pending_sources")
        self.assertIsNone(pending.geometry)

    def test_shared_contact_and_arrival_keep_identity_and_originals(self):
        polygon = shapely.box(5.2, 5.2, 6.8, 5.8)
        left = normalize(cell((4, 5, 5), polygon))
        right = normalize(cell((4, 6, 5), polygon))
        a, b = (4, 5, 5, 9, 0), (4, 6, 5, 9, 0)
        pending = resolve_cells(a, {a[:3]: left.snapshot})
        self.assertEqual(pending.required, (b[:3],))
        self.assertEqual(left.contacts[0]["edge"], right.contacts[0]["edge"])
        self.assertEqual(left.contacts[0]["interval"], right.contacts[0]["interval"])
        self.assertEqual(left.contacts[0]["edge"], (4, 0, 6, 5))
        cells = {a[:3]: left.snapshot, b[:3]: right.snapshot}
        first = resolve_cells(a, cells)
        second = resolve_cells(b, dict(reversed(list(cells.items()))))
        self.assertEqual(first.state, "closed")
        self.assertEqual(first.version, second.version)
        self.assertTrue(first.geometry.equals(polygon))
        self.assertEqual(left.parents[a], (4, 5, 5, "water_polygons", 9, 0))

    def test_connection_outside_cell_does_not_falsely_join_local_parts(self):
        raw = shapely.union_all(
            [
                shapely.box(5.1, 5.2, 5.3, 6.3),
                shapely.box(5.7, 5.2, 5.9, 6.3),
                shapely.box(5.1, 6.1, 5.9, 6.3),
            ]
        )
        lower = normalize(cell((4, 5, 5), raw))
        upper = normalize(cell((4, 5, 6), raw))
        with self.assertRaisesRegex(ValueError, "normalized part budget"):
            normalize(cell((4, 5, 5), raw), piece_limit=1)
        self.assertEqual(len(lower.snapshot.pieces), 2)
        bodies, rejected, _ = assemble(
            [p.polygon for p in lower.snapshot.pieces],
            [p.info for p in lower.snapshot.pieces],
            [SOURCE] * 2,
            assume_ground=True,
        )
        self.assertEqual(len(bodies), 2)
        self.assertFalse(rejected)
        seed = (4, 5, 5, 9, 0)
        self.assertEqual(
            resolve_cells(seed, {(4, 5, 5): lower.snapshot}).required,
            ((4, 5, 6),),
        )
        result = resolve_cells(
            seed, {(4, 5, 5): lower.snapshot, (4, 5, 6): upper.snapshot}
        )
        self.assertEqual(result.state, "closed")
        self.assertEqual(len(result.sources), 3)
        self.assertTrue(result.geometry.equals(raw))

    def test_unknown_incomplete_and_valid_empty_are_distinct(self):
        self.assertIsNone(normalize(None))
        incomplete = normalize(cell((4, 5, 5), complete=False))
        empty = normalize(cell((4, 5, 5)))
        self.assertFalse(incomplete.snapshot.complete)
        self.assertTrue(empty.snapshot.complete)
        self.assertFalse(empty.snapshot.pieces)
        self.assertNotEqual(incomplete.snapshot.digest, empty.snapshot.digest)
        outside = normalize(cell((4, 5, 5), shapely.box(7, 7, 8, 8)))
        self.assertFalse(outside.snapshot.pieces)
        self.assertEqual(len(outside.outside), 1)
        self.assertEqual(outside.ledger[0]["reason"], "outside_owned_cell")

    def test_order_revision_and_layer_bind_normalized_receipt(self):
        raw = cell(
            (4, 5, 5), shapely.box(5.1, 5.1, 5.2, 5.2), shapely.box(5.7, 5.7, 6.1, 5.9)
        )
        expected = normalize(raw)
        reversed_result = normalize(dataclasses.replace(raw, pieces=raw.pieces[::-1]))
        self.assertEqual(expected.snapshot.digest, reversed_result.snapshot.digest)
        self.assertEqual(expected.parents, reversed_result.parents)
        reordered_source = normalize(
            dataclasses.replace(raw, source=dict(reversed(list(raw.source.items()))))
        )
        self.assertEqual(expected.snapshot.digest, reordered_source.snapshot.digest)
        changed = normalize(
            dataclasses.replace(raw, source=dict(SOURCE, revision="r2"))
        )
        self.assertNotEqual(expected.snapshot.digest, changed.snapshot.digest)
        with self.assertRaisesRegex(ValueError, "one delivered water layer"):
            normalize(
                dataclasses.replace(
                    raw,
                    pieces=(
                        dataclasses.replace(
                            raw.pieces[0], info=dict(INPUT, layer="other")
                        ),
                    ),
                )
            )

    def test_output_and_input_caps_refuse_without_partial_publication(self):
        raw = cell((4, 5, 5), shapely.box(5.1, 5.1, 5.9, 5.9))
        original = raw.pieces[0].polygon.wkb
        for options in ({"coordinate_limit": 4}, {"piece_limit": 0}):
            with self.assertRaises(ValueError):
                normalize(raw, **options)
        self.assertEqual(raw.pieces[0].polygon.wkb, original)


if __name__ == "__main__":
    unittest.main()
