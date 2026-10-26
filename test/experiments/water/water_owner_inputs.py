"""Finite source-owned input stencils, independent of caller residence windows.

Completeness refers to declared delivered cell/layer snapshots, not Original OSM
or all Earth water. Distinct stencils still need a shared contact/height owner.
This is a Python experiment, not a database, scheduler or native asset format.
"""

import hashlib
import json
from dataclasses import dataclass

from outshine_ref.places.sources import tile_tuple

LAYERS = ("water_polygons", "water_lines")


@dataclass(frozen=True)
class LayerReceipt:
    tile: tuple
    layer: str
    epoch: tuple
    digest: str
    complete: bool


@dataclass(frozen=True)
class OwnerInputs:
    state: str
    centre: tuple
    tiles: tuple
    required: tuple
    missing: tuple
    version: str | None
    work: dict


def fixed_owner_inputs(centre, receipts, *, epoch, model_revision, radius=1):
    """Bind one fixed finite stencil's two mandatory source layers.

    Callers acquire only missing keys, then repeat with actual receipts. No empty
    snapshot or partial version is manufactured. A source change in this stencil
    invalidates its version; outside arrivals cannot change it. The radius is a
    generation recipe parameter, never inferred from a camera/window/LOD.
    """
    centre = tile_tuple(centre)
    if (
        type(radius) is not int
        or not 1 <= radius <= 3
        or not isinstance(epoch, tuple)
        or len(epoch) != 2
        or any(not isinstance(v, str) or not v for v in epoch)
        or not isinstance(model_revision, str)
        or not model_revision
    ):
        raise ValueError(
            "explicit source epoch, recipe and bounded fixed radius required"
        )
    z, x, y = centre
    side = 1 << z
    work = {
        "receipt_lookups": 0,
        "source_cells": 0,
        "source_layers": len(LAYERS),
        "radius_cells": radius,
        "scope": "finite delivered layer snapshots; no global raw completeness or adjacent-stencil height agreement",
    }
    if min(x, y) < radius or max(x, y) + radius >= side:
        return OwnerInputs("needs_projection_contacts", centre, (), (), (), None, work)
    tiles = tuple(
        (z, a, b)
        for a in range(x - radius, x + radius + 1)
        for b in range(y - radius, y + radius + 1)
    )
    keys = tuple((tile, layer) for tile in tiles for layer in LAYERS)
    missing, contents = [], []
    for key in keys:
        receipt = receipts.get(key)
        work["receipt_lookups"] += 1
        if receipt is None:
            missing.append(key)
            continue
        if (
            not isinstance(receipt, LayerReceipt)
            or (receipt.tile, receipt.layer) != key
            or receipt.epoch != epoch
            or not isinstance(receipt.digest, str)
            or not receipt.digest
            or type(receipt.complete) is not bool
        ):
            raise ValueError("source layer receipt identity/epoch/digest mismatch")
        if not receipt.complete:
            missing.append(key)
        contents.append((key, receipt.digest))
    work["source_cells"] = len(tiles)
    if missing:
        return OwnerInputs(
            "pending_sources", centre, tiles, keys, tuple(missing), None, work
        )
    digest = hashlib.sha256(
        json.dumps(
            [
                "fixed-water-owner-inputs-v1",
                centre,
                radius,
                epoch,
                model_revision,
                contents,
            ],
            sort_keys=True,
            separators=(",", ":"),
        ).encode()
    ).hexdigest()
    return OwnerInputs("ready", centre, tiles, keys, (), digest, work)
