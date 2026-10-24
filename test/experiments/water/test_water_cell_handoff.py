"""Acquisition-time handoff, canonical source IDs and incomplete-arrival controls."""

import dataclasses
import unittest

import shapely
from test_owned_water_cells import cell
from water_cell_handoff import route_cells, source_token

A, B, C = (4, 5, 5), (4, 6, 5), (4, 10, 10)


def route(snapshots, recipients=(A, B), source_tiles=(A, B), **kwargs):
    return route_cells(
        snapshots,
        source_tiles=source_tiles,
        recipients=recipients,
        scope="explicit-delivered-batch-r1",
        **kwargs,
    )


def geometry(products):
    return shapely.union_all(
        [p.polygon for c in products.values() for p in c.snapshot.pieces]
    )


class WaterCellHandoff(unittest.TestCase):
    def test_overhang_enters_valid_empty_neighbor_with_original_provenance(self):
        raw = cell(A, shapely.box(5.2, 5.2, 6.8, 5.8))
        products, work = route({A: raw, B: cell(B)})
        self.assertTrue(geometry(products).equals(raw.pieces[0].polygon))
        self.assertEqual(len(products[B].snapshot.pieces), 1)
        self.assertEqual(
            set(products[B].parents.values()), {A + ("water_polygons", 9, 0)}
        )
        self.assertEqual(
            products[A].contacts[0]["edge"], products[B].contacts[0]["edge"]
        )
        self.assertEqual(
            products[A].contacts[0]["interval"], products[B].contacts[0]["interval"]
        )
        self.assertEqual(work["manufactured_receipts"], 0)

    def test_received_geometry_does_not_certify_missing_or_failed_neighbor(self):
        raw = cell(A, shapely.box(5.2, 5.2, 6.8, 5.8))
        for missing in (None, cell(B, complete=False)):
            products, _ = route({A: raw, B: missing})
            for p in products.values():
                self.assertEqual(p.state, "pending_sources")
                self.assertIsNone(p.snapshot)
                self.assertEqual(p.required, (B,))
        products, _ = route({A: raw, B: cell(B)})
        self.assertTrue(all(p.state == "ready" for p in products.values()))

    def test_overlapping_recipient_windows_and_source_order_are_stable(self):
        snapshots = {
            A: cell(A, shapely.box(5.2, 5.2, 6.8, 5.8)),
            B: cell(B, shapely.box(5.3, 5.3, 6.7, 5.7)),
        }
        together, _ = route(snapshots)
        for recipient in (A, B):
            alone, _ = route(
                dict(reversed(list(snapshots.items()))), (recipient,), (B, A)
            )
            self.assertEqual(together[recipient].snapshot, alone[recipient].snapshot)
            self.assertEqual(together[recipient].parents, alone[recipient].parents)

    def test_other_source_arrival_does_not_renumber_contributors(self):
        first = {
            A: cell(A, shapely.box(5.2, 5.2, 6.8, 5.8)),
            B: cell(B),
            C: cell(C),
        }
        second = dict(first)
        second[C] = cell(C, shapely.box(10.1, 10.1, 10.9, 10.9))
        second[C] = dataclasses.replace(second[C], digest="new-C-digest")
        before, _ = route(first, source_tiles=(A, B, C))
        after, _ = route(second, source_tiles=(A, B, C))
        self.assertEqual(before[A].snapshot, after[A].snapshot)
        self.assertEqual(before[B].parents, after[B].parents)
        missing = dict(first)
        missing.pop(C)
        pending, _ = route(missing, source_tiles=(A, B, C))
        self.assertEqual(pending[A].required, (C,))

    def test_source_holes_and_outside_batch_material_remain_in_originals(self):
        raw = shapely.Polygon(
            [(4.5, 5.1), (7.5, 5.1), (7.5, 5.9), (4.5, 5.9)],
            [[(5.8, 5.3), (6.2, 5.3), (6.2, 5.7), (5.8, 5.7)]],
        )
        original = cell(A, raw)
        unchanged = original.pieces[0].polygon.wkb
        products, _ = route({A: original, B: cell(B)})
        output = geometry(products)
        expected = raw.intersection(shapely.box(5, 5, 7, 6))
        self.assertTrue(output.equals(expected))
        self.assertEqual(len(output.interiors), 1)
        self.assertEqual(original.pieces[0].polygon.wkb, unchanged)
        self.assertGreater(raw.difference(output).area, 0)
        self.assertEqual(set(products), {A, B})

    def test_colliding_feature_numbers_from_other_source_cells_remain_distinct(self):
        snapshots = {
            A: cell(A, shapely.box(5.2, 5.2, 6.8, 5.8)),
            B: cell(B, shapely.box(5.3, 5.3, 6.7, 5.7)),
        }
        products, _ = route(snapshots)
        self.assertEqual(len(products[A].snapshot.pieces), 2)
        self.assertEqual(len(set(products[A].parents.values())), 2)
        tokens = {
            source_token(x, y, f) for x in range(8) for y in range(8) for f in range(8)
        }
        self.assertEqual(len(tokens), 8**3)

    def test_caps_scope_and_provider_mismatch_refuse(self):
        raw = cell(A, shapely.box(5.2, 5.2, 6.8, 5.8))
        for kwargs in ({"coordinate_limit": 4}, {"source_limit": 0}):
            with self.assertRaises(ValueError):
                route({A: raw, B: cell(B)}, **kwargs)
        with self.assertRaisesRegex(ValueError, "recipient subset"):
            route({A: raw}, recipients=(B,), source_tiles=(A,))
        other = dataclasses.replace(cell(B), source=dict(raw.source, revision="other"))
        with self.assertRaisesRegex(ValueError, "provider/revision"):
            route({A: raw, B: other})


if __name__ == "__main__":
    unittest.main()
