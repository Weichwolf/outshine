"""Bounded source-batch normalization: spatial ownership differs from provenance."""

import hashlib
import json
from dataclasses import dataclass

import shapely
from canonical_water_domains import Piece, Snapshot
from outshine_ref.places.sources import tile_tuple
from owned_water_cells import normalize_cell


@dataclass(frozen=True)
class CellHandoff:
    state: str
    snapshot: Snapshot | None
    parents: dict
    contacts: tuple
    required: tuple
    work: dict


def source_token(x, y, feature):
    """Injective Python-only lowering; the original structured ID remains primary.

    Cantor pairs are independent of other source arrivals or enumeration order.
    These arbitrary-width integers are not a proposed native storage format.
    """
    tile = (x + y) * (x + y + 1) // 2 + y
    return (tile + feature) * (tile + feature + 1) // 2 + feature


def route_cells(
    snapshots,
    *,
    source_tiles,
    recipients,
    scope,
    layer="water_polygons",
    source_limit=1024,
    coordinate_limit=131072,
):
    """Normalize an explicitly declared finite delivered source batch.

    All declared source receipts must arrive before publishing a complete cell.
    This is acquisition-time batch work, not a world-wide source search. A raw
    overhang may contribute to any requested cell in this batch. Unrequested or
    outside-batch geometry remains in the caller-owned originals; no receipt is
    manufactured. The batch must be stable before comparing overlapping windows.
    Native/provider completeness beyond this declared batch is never asserted.
    """
    tiles = tuple(sorted({tile_tuple(t) for t in source_tiles}))
    requested = tuple(sorted({tile_tuple(t) for t in recipients}))
    if (
        not tiles
        or len(tiles) > 49
        or not set(requested) <= set(tiles)
        or len({t[0] for t in tiles}) != 1
        or not isinstance(scope, str)
        or not scope
    ):
        raise ValueError("bounded declared source batch and recipient subset required")
    if (
        type(source_limit) is not int
        or not 1 <= source_limit <= 1024
        or type(coordinate_limit) is not int
        or not 1 <= coordinate_limit <= 131072
    ):
        raise ValueError("bounded source normalization work required")
    raw_count = sum(
        len(snapshots[t].pieces) for t in tiles if snapshots.get(t) is not None
    )
    coordinates = sum(
        shapely.get_num_coordinates(p.polygon)
        for t in tiles
        if snapshots.get(t) is not None
        for p in snapshots[t].pieces
    )
    if raw_count > source_limit or coordinates > coordinate_limit:
        raise ValueError("declared source batch budget exhausted")
    missing = tuple(
        t for t in tiles if snapshots.get(t) is None or not snapshots[t].complete
    )
    work = {
        "declared_source_cells": len(tiles),
        "raw_parts": raw_count,
        "raw_coordinates": int(coordinates),
        "source_candidate_pairs": 0,
        "positive_source_cell_pairs": 0,
        "manufactured_receipts": 0,
    }
    if missing:
        return {
            t: CellHandoff("pending_sources", None, {}, (), missing, dict(work))
            for t in requested
        }, work
    records = []
    epoch = None
    for tile in tiles:
        original = snapshots[tile]
        if original.tile != tile:
            raise ValueError("source receipt identity mismatch")
        # Reuse the same source validation/clipping policy, keeping the originals.
        normalize_cell(original, scope=scope, layer=layer)
        identity = tuple(
            original.source.get(k) for k in ("provider", "revision", "origin", "kind")
        )
        if epoch is None:
            epoch = identity
        if epoch != identity:
            raise ValueError(
                "one explicit provider/revision/source-kind batch required"
            )
        for p in original.pieces:
            parent = tile + (layer, p.feature, p.part)
            records.append((parent, p, original.digest))
    records.sort(key=lambda r: r[0])
    tree = shapely.STRtree([r[1].polygon for r in records])
    batch_scope = hashlib.sha256(
        json.dumps(
            ["delivered-water-batch-v1", scope, layer, tiles, epoch], sort_keys=True
        ).encode()
    ).hexdigest()
    outputs = {}
    for tile in requested:
        _, x, y = tile
        box = shapely.box(x, y, x + 1, y + 1)
        candidates = sorted(tree.query(box).tolist())
        work["source_candidate_pairs"] += len(candidates)
        pieces, origin_map, receipt = [], {}, []
        for i in candidates:
            parent, raw, digest = records[i]
            if raw.polygon.intersection(box).area <= 0:
                continue
            work["positive_source_cell_pairs"] += 1
            token = source_token(parent[1], parent[2], raw.feature)
            pieces.append(Piece(token, raw.part, raw.polygon, raw.info))
            origin_map[(token, raw.part)] = parent
            receipt.append((parent, digest, raw.polygon.wkb_hex))
        version = hashlib.sha256(
            json.dumps(
                [batch_scope, tile, snapshots[tile].digest, receipt],
                sort_keys=True,
                separators=(",", ":"),
            ).encode()
        ).hexdigest()
        virtual = Snapshot(tile, snapshots[tile].source, version, True, tuple(pieces))
        owned = normalize_cell(virtual, scope=batch_scope, layer=layer)
        parents = {
            key: origin_map[(value[-2], value[-1])]
            for key, value in owned.parents.items()
        }
        contacts = tuple(dict(c, original=parents[c["source"]]) for c in owned.contacts)
        outputs[tile] = CellHandoff(
            "ready",
            owned.snapshot,
            parents,
            contacts,
            (),
            dict(owned.work, contributing_originals=len(pieces)),
        )
    return outputs, work
