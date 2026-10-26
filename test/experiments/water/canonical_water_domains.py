"""Bounded source closure; conditional on the adapter's declared support/completeness.

Coordinates are global, unwrapped Web-Mercator tile units at one source zoom.
A complete snapshot covers all normalized polygons of the declared semantic epoch,
not merely everything admitted in a camera window. Digests identify that snapshot.
This first experiment defers projection seams and open/global bodies to contacts.
"""

import hashlib
import json
import math
from dataclasses import dataclass

import shapely
from water_body_domains import assemble, merge_key


@dataclass(frozen=True)
class Piece:
    feature: int
    part: int
    polygon: object
    info: dict


@dataclass(frozen=True)
class Snapshot:
    tile: tuple
    source: dict
    digest: str
    complete: bool
    pieces: tuple


@dataclass(frozen=True)
class Closure:
    state: str
    reason: str
    required: tuple
    sources: tuple
    owner: tuple | None
    version: str | None
    geometry: object | None
    work: dict


def source_cells(bounds, zoom, support, limit):
    """Closed boxes intersect iff each axis intersects, including exact tile edges.

    A piece from cell i lies in [i-support, i+1+support]. Therefore it can
    touch [a,b] only if ceil(a-1-support) <= i <= floor(b+support).
    Count before allocation. No modulo wrapping/projection-cap claim here.
    """
    a, b, c, d = bounds
    low = (math.ceil(a - 1 - support), math.ceil(b - 1 - support))
    high = (math.floor(c + support), math.floor(d + support))
    side = 1 << zoom
    if min(low) < 0 or max(high) >= side:
        return (), "projection_seam_needs_contacts"
    if (high[0] - low[0] + 1) * (high[1] - low[1] + 1) > limit:
        return (), "source_tile_budget_needs_contacts"
    return tuple(
        (zoom, x, y)
        for x in range(low[0], high[0] + 1)
        for y in range(low[1], high[1] + 1)
    ), ""


def validate_snapshot(snapshot, tile, epoch, support):
    _z, x, y = tile
    if (
        snapshot.tile != tile
        or not isinstance(snapshot.digest, str)
        or not snapshot.digest
    ):
        return "invalid_snapshot_receipt"
    if (snapshot.source.get("provider"), snapshot.source.get("revision")) != epoch:
        return "source_epoch_mismatch"
    if type(snapshot.complete) is not bool:
        return "invalid_completeness"
    identifiers = set()
    for piece in snapshot.pieces:
        key = (piece.feature, piece.part)
        if any(type(v) is not int or v < 0 for v in key) or key in identifiers:
            return "invalid_source_identity"
        identifiers.add(key)
        polygon = piece.polygon
        if (
            polygon.geom_type not in ("Polygon", "MultiPolygon")
            or polygon.is_empty
            or not polygon.is_valid
            or not math.isfinite(polygon.area)
        ):
            return "invalid_source_geometry"
        a, b, c, d = polygon.bounds
        if (
            a < x - support
            or b < y - support
            or c > x + 1 + support
            or d > y + 1 + support
        ):
            return "source_support_violation"
    return ""


def resolve(
    seed,
    snapshots,
    *,
    epoch,
    support,
    model_revision,
    assume_ground=False,
    tile_limit=49,
    source_limit=1024,
    coordinate_limit=131072,
):
    """Read only needed cells; no IO, source acquisition, fitting or partial mesh.

    seed=(z,x,y,feature,part). Call again after missing snapshots arrive. A shared
    owner/version is an input to an existing canonical fit, never a hydraulic ID.
    Model revision must bind datum, priors, hard contacts and recipe semantics.
    Caller-owned inputs must remain unchanged throughout a call.
    """
    if (
        len(seed) != 5
        or any(type(v) is not int or v < 0 for v in seed)
        or not 0 <= seed[0] <= 22
        or max(seed[1:3]) >= 1 << seed[0]
        or len(epoch) != 2
        or not all(isinstance(v, str) and v for v in epoch)
        or not isinstance(model_revision, str)
        or not model_revision
        or type(support) not in (int, float)
        or not math.isfinite(support)
        or not 0 <= support <= 1
        or type(assume_ground) is not bool
    ):
        raise ValueError(
            "explicit seed, epoch, support, model revision and policy required"
        )
    for value, cap in (
        (tile_limit, 49),
        (source_limit, 1024),
        (coordinate_limit, 131072),
    ):
        if type(value) is not int or not 1 <= value <= cap:
            raise ValueError("bounded positive work limits required")
    seed = tuple(seed)
    work = {
        "tile_reads": 0,
        "snapshot_lookups": 0,
        "source_records_examined": 0,
        "coordinates_examined": 0,
        "source_records": 0,
        "coordinates": 0,
        "passes": 0,
        "relation_tests": 0,
        "candidate_pairs_seen": 0,
        "geometry_wkb_bytes": 0,
        "snapshot_payload_bytes": None,
        "copied_bytes": None,
        "scope": "Python source closure, not native loading or hydraulic admission",
    }
    needed, read, records, members = {tuple(seed[:3])}, {}, {}, ()

    def result(state, reason, required=(), geometry=None, version=None):
        return Closure(
            state,
            reason,
            tuple(sorted(required)),
            members,
            min(members) if geometry is not None else None,
            version,
            geometry,
            dict(work),
        )

    while True:
        pending = []
        for tile in sorted(needed - read.keys()):
            work["snapshot_lookups"] += 1
            snapshot = snapshots.get(tile)
            if snapshot is None:
                pending.append(tile)
                continue
            if len(snapshot.pieces) + len(records) > source_limit:
                return result("needs_contacts", "source_record_budget")
            coordinates = int(
                sum(shapely.get_num_coordinates(p.polygon) for p in snapshot.pieces)
            )
            if work["coordinates"] + coordinates > coordinate_limit:
                return result("needs_contacts", "coordinate_budget")
            work["source_records_examined"] += len(snapshot.pieces)
            work["coordinates_examined"] += coordinates
            error = validate_snapshot(snapshot, tile, epoch, support)
            if error:
                return result("invalid_source", error, (tile,))
            if not snapshot.complete:
                pending.append(tile)
                continue
            read[tile] = snapshot
            work["tile_reads"] += 1
            work["coordinates"] += coordinates
            for piece in snapshot.pieces:
                records[tile + (piece.feature, piece.part)] = (piece, snapshot.source)
            work["source_records"] = len(records)
        if pending:
            return result("pending_sources", "missing_or_incomplete_snapshot", pending)
        if seed not in records:
            return result("invalid_source", "seed_absent_from_complete_snapshot")
        piece, source = records[seed]
        key, policy = merge_key(piece.info, source, assume_ground)
        if key is None:
            return result("needs_semantics", policy)
        keys = sorted(records)
        bodies, rejected, cost = assemble(
            [records[k][0].polygon for k in keys],
            [records[k][0].info for k in keys],
            [records[k][1] for k in keys],
            assume_ground=assume_ground,
            source_limit=source_limit,
            coordinate_limit=coordinate_limit,
            broad_phase="tree",
        )
        work["passes"] += 1
        for name in ("relation_tests", "candidate_pairs_seen"):
            work[name] += cost[name]
        index = keys.index(seed)
        failure = next((r["reason"] for r in rejected if r["source"] == index), None)
        if failure:
            return result("needs_contacts", failure)
        body = next(b for b in bodies if index in b.sources)
        members = tuple(keys[i] for i in body.sources)
        cells, error = source_cells(body.geometry.bounds, seed[0], support, tile_limit)
        if error:
            return result("needs_contacts", error)
        needed.update(cells)
        if len(needed) > tile_limit:
            return result("needs_contacts", "source_tile_budget_needs_contacts")
        if needed - read.keys():
            continue
        geometry = shapely.normalize(body.geometry)
        encoded = geometry.wkb
        work["geometry_wkb_bytes"] = len(encoded)
        receipt = json.dumps(
            [
                "canonical-source-closure-v1",
                epoch,
                support,
                model_revision,
                assume_ground,
                policy,
                members,
                [(tile, read[tile].digest) for tile in sorted(needed)],
            ],
            separators=(",", ":"),
            sort_keys=True,
        ).encode()
        version = hashlib.sha256(receipt + encoded).hexdigest()
        return result(
            "closed",
            "conditional_on_declared_source_contract",
            geometry=geometry,
            version=version,
        )
