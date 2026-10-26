"""Independent exhaustive closure controls and source-arrival counterexamples."""

import dataclasses
import unittest
from unittest.mock import patch

import shapely
from canonical_water_domains import Piece, Snapshot, resolve, source_cells
from test_water_domains import INPUT, SOURCE

EPOCH = (SOURCE["provider"], SOURCE["revision"])


def snapshot(tile, polygons=()):
    pieces = tuple(Piece(i, 0, p, dict(INPUT)) for i, p in enumerate(polygons))
    return Snapshot(tile, dict(SOURCE), "snapshot:" + str(tile), True, pieces)


def case():
    tiles = {(4, x, y): snapshot((4, x, y)) for x in range(4, 9) for y in range(4, 8)}
    shapes = {
        (4, 5, 5): [shapely.box(5.1, 5.1, 5.3, 6)],
        (4, 5, 6): [shapely.box(5.1, 6, 6, 6.2)],
        (4, 6, 6): [shapely.box(6, 6, 6.9, 6.2)],
        (4, 6, 5): [shapely.box(6.7, 5.1, 6.9, 6)],
    }
    tiles.update({t: snapshot(t, p) for t, p in shapes.items()})
    return tiles


def run(seed, tiles, **kwargs):
    return resolve(
        seed,
        tiles,
        epoch=EPOCH,
        support=0,
        model_revision="model-r1",
        assume_ground=True,
        **kwargs,
    )


class CanonicalDomains(unittest.TestCase):
    def test_closed_box_cover_matches_exhaustive_tile_intersections(self):
        for support in (0, 0.125, 1):
            for a, b in ((5, 6), (5.125, 6.875), (5, 5), (5.9, 6.1)):
                bounds = (a, 5.25, b, 6)
                actual, error = source_cells(bounds, 4, support, 49)
                expected = tuple(
                    (4, x, y)
                    for x in range(16)
                    for y in range(16)
                    if x - support <= b
                    and x + 1 + support >= a
                    and y - support <= 6
                    and y + 1 + support >= 5.25
                )
                self.assertEqual(error, "")
                self.assertEqual(actual, expected)

    def test_hidden_join_seeds_and_order_have_one_canonical_product(self):
        tiles = case()
        results = [run(t + (0, 0), tiles) for t, s in tiles.items() if s.pieces]
        self.assertTrue(all(r.state == "closed" for r in results))
        self.assertEqual(len({r.owner for r in results}), 1)
        self.assertEqual(len({r.version for r in results}), 1)
        self.assertEqual(len({r.geometry.wkb for r in results}), 1)
        self.assertTrue(all(len(r.sources) == 4 for r in results))
        reverse = run((4, 6, 5, 0, 0), dict(reversed(list(tiles.items()))))
        self.assertEqual(reverse.version, results[0].version)
        full = shapely.union_all([p.polygon for s in tiles.values() for p in s.pieces])
        self.assertTrue(reverse.geometry.equals(full))
        # Both worlds have identical local observations but different global connectivity.
        window = shapely.box(5, 5, 7, 5.5)
        disconnected = dict(tiles)
        for t in ((4, 5, 6), (4, 6, 6)):
            disconnected[t] = snapshot(t)
        other = shapely.union_all(
            [p.polygon for s in disconnected.values() for p in s.pieces]
        )
        self.assertTrue(full.intersection(window).equals(other.intersection(window)))
        left = run((4, 5, 5, 0, 0), disconnected)
        right = run((4, 6, 5, 0, 0), disconnected)
        self.assertNotEqual(left.owner, right.owner)
        self.assertEqual(len(left.sources), 1)

    def test_missing_and_incomplete_arrival_never_expose_partial_geometry(self):
        tiles = case()
        seed = (4, 5, 5, 0, 0)
        partial = {seed[:3]: tiles[seed[:3]]}
        first = run(seed, partial)
        self.assertEqual(first.state, "pending_sources")
        self.assertIsNone(first.geometry)
        self.assertIn((4, 5, 6), first.required)
        for tile in first.required:
            partial[tile] = tiles[tile]
        partial[(4, 5, 6)] = dataclasses.replace(tiles[(4, 5, 6)], complete=False)
        pending = run(seed, partial)
        self.assertEqual(pending.state, "pending_sources")
        self.assertEqual(pending.required, ((4, 5, 6),))
        # Demand is a set difference; existing snapshots are kept by the caller.
        while True:
            next_result = run(seed, partial)
            if next_result.state != "pending_sources":
                break
            partial.update({t: tiles[t] for t in next_result.required})
        self.assertEqual(next_result.version, run(seed, tiles).version)

    def test_loaded_extras_rotation_and_irrelevant_arrival_do_no_work(self):
        tiles = case()
        seed = (4, 5, 5, 0, 0)
        expected = run(seed, tiles)
        # A huge unrelated tile is already loaded but must never be inspected.
        tiles[(4, 12, 12)] = None
        for _ in range(4):
            result = run(seed, tiles)
            self.assertEqual(result.version, expected.version)
            self.assertEqual(result.work, expected.work)
        self.assertEqual(result.work["tile_reads"], 4)
        self.assertLessEqual(result.work["passes"], result.work["tile_reads"])

    def test_local_budgets_preserve_independent_pond(self):
        tiles = case()
        seed = (4, 5, 5, 0, 0)
        for kwargs in ({"tile_limit": 1}, {"source_limit": 1}, {"coordinate_limit": 5}):
            result = run(seed, tiles, **kwargs)
            self.assertEqual(result.state, "needs_contacts")
            self.assertIsNone(result.geometry)
        pond = (4, 8, 7)
        tiles[pond] = snapshot(pond, [shapely.box(8.2, 7.2, 8.8, 7.8)])
        result = run(pond + (0, 0), tiles, tile_limit=1)
        self.assertEqual(result.state, "closed")
        self.assertEqual(result.work["tile_reads"], 1)

    def test_model_epoch_and_support_are_explicit(self):
        tiles = case()
        seed = (4, 5, 5, 0, 0)
        good = run(seed, tiles)
        changed = resolve(
            seed,
            tiles,
            epoch=EPOCH,
            support=0,
            model_revision="model-r2",
            assume_ground=True,
        )
        self.assertEqual(changed.owner, good.owner)
        self.assertNotEqual(changed.version, good.version)
        tile = seed[:3]
        tiles[tile] = dataclasses.replace(
            tiles[tile], source=dict(SOURCE, revision="other")
        )
        self.assertEqual(run(seed, tiles).reason, "source_epoch_mismatch")
        tiles = case()
        tiles[tile] = snapshot(tile, [shapely.box(5.1, 5.1, 6.01, 5.2)])
        self.assertEqual(run(seed, tiles).reason, "source_support_violation")

    def test_unresolved_crossings_and_exact_point_do_not_fake_connections(self):
        tile = (4, 5, 5)
        s = snapshot(
            tile, [shapely.box(5.1, 5.1, 5.4, 5.4), shapely.box(5.4, 5.4, 5.7, 5.7)]
        )
        seed = tile + (0, 0)
        result = run(seed, {tile: s})
        self.assertEqual(result.sources, (seed,))
        s = dataclasses.replace(
            s, pieces=(dataclasses.replace(s.pieces[0], info=dict(INPUT, bridge=None)),)
        )
        self.assertEqual(run(seed, {tile: s}).state, "needs_semantics")
        with patch("canonical_water_domains.assemble", side_effect=AssertionError):
            result = resolve(
                seed, {tile: s}, epoch=EPOCH, support=0, model_revision="m"
            )
        self.assertEqual(result.state, "needs_semantics")

    def test_source_revision_changes_version_but_not_owner(self):
        tiles = case()
        seed = (4, 5, 5, 0, 0)
        first = run(seed, tiles)
        tiles[seed[:3]] = dataclasses.replace(
            tiles[seed[:3]], digest="different-normalized-snapshot"
        )
        second = run(seed, tiles)
        self.assertEqual(first.owner, second.owner)
        self.assertNotEqual(first.version, second.version)

    def test_lake_and_coast_use_existing_semantics_not_provider_id_alone(self):
        tile = (4, 5, 5)
        for kind in ("lake", "ocean"):
            pieces = tuple(
                Piece(i, 0, p, dict(INPUT, kind=kind, provider_feature_id="same"))
                for i, p in enumerate(
                    (shapely.box(5.1, 5.1, 5.5, 5.8), shapely.box(5.5, 5.1, 5.9, 5.8))
                )
            )
            s = snapshot(tile)
            s = dataclasses.replace(s, pieces=pieces)
            result = run(tile + (0, 0), {tile: s})
            self.assertEqual(len(result.sources), 2)
            s = dataclasses.replace(
                s,
                pieces=(
                    pieces[0],
                    dataclasses.replace(
                        pieces[1],
                        info=dict(pieces[1].info, provider_feature_id="other"),
                    ),
                ),
            )
            self.assertEqual(len(run(tile + (0, 0), {tile: s}).sources), 1)

    def test_malformed_identity_and_overhang_fail_before_fit(self):
        tile = (4, 5, 5)
        piece = Piece(0, 0, shapely.box(5.1, 5.1, 5.8, 5.8), dict(INPUT))
        s = dataclasses.replace(snapshot(tile), pieces=(piece, piece))
        self.assertEqual(
            run(tile + (0, 0), {tile: s}).reason, "invalid_source_identity"
        )
        with self.assertRaises(ValueError):
            run(tile + (0, 0), {tile: s}, tile_limit=50)
        with self.assertRaises(ValueError):
            resolve(
                tile + (0, 0),
                {tile: s},
                epoch=EPOCH,
                support=float("inf"),
                model_revision="m",
            )

    def test_projection_seams_remain_explicit(self):
        for tile, polygon in (
            ((4, 0, 5), shapely.box(0, 5.1, 0.5, 5.8)),
            ((4, 5, 0), shapely.box(5.1, 0, 5.8, 0.5)),
        ):
            result = run(tile + (0, 0), {tile: snapshot(tile, [polygon])})
            self.assertEqual(result.state, "needs_contacts")
            self.assertEqual(result.reason, "projection_seam_needs_contacts")


if __name__ == "__main__":
    unittest.main()
