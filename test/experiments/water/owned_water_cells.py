"""Finite delivered cell/layer model; retain raw buffers without granting neighbor receipts."""

import hashlib
import json
from dataclasses import dataclass

import numpy as np
import shapely
from canonical_water_domains import Piece, Snapshot
from outshine_ref.places.sources import tile_tuple


@dataclass(frozen=True)
class OwnedCell:
    snapshot: Snapshot
    original: Snapshot
    parents: dict
    outside: dict
    ledger: tuple
    contacts: tuple
    work: dict


def normalize_cell(
    snapshot,
    *,
    scope,
    layer="water_polygons",
    piece_limit=1024,
    coordinate_limit=131072,
):
    """Complete means the declared delivered snapshot, never all original OSM.

    Outside geometry is retained with original provenance for neighbor handling;
    this function never creates a neighbor or promotes an incomplete receipt.
    Routing that remainder into its recipient's product is a separate required
    step; closure of these owned pieces does not certify that handoff.
    Normalized part=(original_part<<32)|component, invertible within this layer.
    Contacts identify shared geometric edges; heights/semantics still need ownership.
    """
    if snapshot is None:
        return None
    z, x, y = tile_tuple(snapshot.tile)
    if (
        not isinstance(scope, str)
        or not scope
        or not isinstance(layer, str)
        or not layer
    ):
        raise ValueError("declared delivered snapshot scope/layer required")
    if type(snapshot.complete) is not bool or not snapshot.digest:
        raise ValueError("explicit receipt and completeness required")
    if (
        type(piece_limit) is not int
        or not 1 <= piece_limit <= 1024
        or type(coordinate_limit) is not int
        or not 1 <= coordinate_limit <= 131072
    ):
        raise ValueError("bounded normalization work required")
    if (
        len(snapshot.pieces) > piece_limit
        or sum(shapely.get_num_coordinates(p.polygon) for p in snapshot.pieces)
        > coordinate_limit
    ):
        raise ValueError("raw cell normalization budget exhausted")
    box = shapely.box(x, y, x + 1, y + 1)
    edges = [
        ((z, 0, x, y), shapely.LineString([(x, y), (x, y + 1)]), 1),
        ((z, 0, x + 1, y), shapely.LineString([(x + 1, y), (x + 1, y + 1)]), 1),
        ((z, 1, x, y), shapely.LineString([(x, y), (x + 1, y)]), 0),
        ((z, 1, x, y + 1), shapely.LineString([(x, y + 1), (x + 1, y + 1)]), 0),
    ]
    pieces, parents, outside, ledger, contacts = [], {}, {}, [], []
    seen = set()
    output_coordinates = 0
    digest = hashlib.sha256(
        json.dumps(
            [
                "owned-water-cell-v1",
                scope,
                layer,
                snapshot.tile,
                snapshot.source,
                snapshot.digest,
                snapshot.complete,
            ],
            separators=(",", ":"),
        ).encode()
    )
    for raw in sorted(snapshot.pieces, key=lambda p: (p.feature, p.part)):
        if (
            type(raw.feature) is not int
            or type(raw.part) is not int
            or min(raw.feature, raw.part) < 0
        ):
            raise ValueError("original feature/part identity required")
        if not isinstance(raw.info, dict):
            raise TypeError("original source metadata required")
        raw_layer = raw.info.get("layer")
        key = snapshot.tile + (raw_layer, raw.feature, raw.part)
        if key in seen:
            raise ValueError("duplicate original source identity")
        seen.add(key)
        if raw_layer != layer:
            raise ValueError(
                "receipt must explicitly describe one delivered water layer"
            )
        polygon = raw.polygon
        if (
            polygon.is_empty
            or not polygon.is_valid
            or polygon.geom_type not in ("Polygon", "MultiPolygon")
            or not np.isfinite(shapely.get_coordinates(polygon)).all()
        ):
            raise ValueError("valid original source polygons required")
        inside = polygon.intersection(box)
        remainder = polygon.difference(box)
        if not remainder.is_empty:
            outside[key] = remainder
        parts = sorted(
            (
                shapely.normalize(p)
                for p in shapely.get_parts(inside)
                if p.geom_type == "Polygon" and p.area > 0
            ),
            key=lambda p: p.wkb,
        )
        if len(parts) >= 1 << 32 or len(pieces) + len(parts) > piece_limit:
            raise ValueError("normalized part budget exhausted")
        normalized = []
        for ordinal, part in enumerate(parts):
            output_coordinates += int(shapely.get_num_coordinates(part))
            if output_coordinates > coordinate_limit:
                raise ValueError("normalized coordinate budget exhausted")
            token = (raw.part << 32) | ordinal
            part_key = snapshot.tile + (raw.feature, token)
            pieces.append(Piece(raw.feature, token, part, raw.info))
            parents[part_key] = key
            normalized.append(part_key)
            digest.update(
                json.dumps(
                    [key, token, raw.info], sort_keys=True, separators=(",", ":")
                ).encode()
                + part.wkb
            )
            for edge_key, line, axis in edges:
                for contact in shapely.get_parts(part.intersection(line)):
                    if contact.geom_type == "LineString" and contact.length > 0:
                        values = np.asarray(contact.coords)[:, axis]
                        contacts.append(
                            {
                                "edge": edge_key,
                                "interval": [float(values.min()), float(values.max())],
                                "source": part_key,
                                "original": key,
                            }
                        )
        ledger.append(
            {
                "original": key,
                "normalized": normalized,
                "original_area": float(polygon.area),
                "owned_area": float(inside.area),
                "outside_area": float(remainder.area),
                "reason": "owned_fragment" if parts else "outside_owned_cell",
                "area_balance_error": float(
                    polygon.area - inside.area - remainder.area
                ),
            }
        )
    source = dict(
        snapshot.source,
        normalized_scope=scope,
        normalized_layer=layer,
        normalization="owned-water-cell-v1",
    )
    result = Snapshot(
        snapshot.tile, source, digest.hexdigest(), snapshot.complete, tuple(pieces)
    )
    return OwnedCell(
        result,
        snapshot,
        parents,
        outside,
        tuple(ledger),
        tuple(contacts),
        {
            "raw_parts": len(snapshot.pieces),
            "normalized_parts": len(pieces),
            "normalized_coordinates": output_coordinates,
            "outside_parts_retained": len(outside),
            "contact_intervals": len(contacts),
            "declared_support_tile_units": 0,
            "manufactured_neighbors": 0,
        },
    )
