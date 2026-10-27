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


@dataclass(frozen=True)
class InterfaceInputs:
    state: str
    edge: tuple
    cells: tuple
    tiles: tuple
    required: tuple
    missing: tuple
    version: str | None
    work: dict


def validate_recipe(epoch, model_revision, radius):
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


def receipt_contents(tiles, receipts, epoch):
    keys = tuple((tile, layer) for tile in tiles for layer in LAYERS)
    missing, contents = [], []
    for key in keys:
        receipt = receipts.get(key)
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
    return keys, tuple(missing), contents


def fixed_owner_inputs(centre, receipts, *, epoch, model_revision, radius=1):
    """Bind one fixed finite stencil's two mandatory source layers.

    Callers acquire only missing keys, then repeat with actual receipts. No empty
    snapshot or partial version is manufactured. A source change in this stencil
    invalidates its version; outside arrivals cannot change it. The radius is a
    generation recipe parameter, never inferred from a camera/window/LOD.
    """
    centre = tile_tuple(centre)
    validate_recipe(epoch, model_revision, radius)
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
    keys, missing, contents = receipt_contents(tiles, receipts, epoch)
    work["receipt_lookups"] = len(keys)
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


def fixed_interface_inputs(edge, receipts, *, epoch, model_revision, radius=1):
    """The union of both cell stencils is owned by one canonical edge key.

    This binds source input ownership, not yet compatible heights or corners.
    Counts precede allocation; at most49 delivered cells/98 layers are requested.
    """
    validate_recipe(epoch, model_revision, radius)
    edge = tuple(edge)
    if (
        len(edge) != 4
        or any(type(v) is not int for v in edge)
        or not 0 <= edge[0] <= 30
        or edge[1] not in (0, 1)
    ):
        raise ValueError("valid canonical cell edge required")
    z, axis, x, y = edge
    side = 1 << z
    if not (0 <= x <= side - axis and 0 <= y < side + axis):
        raise ValueError("canonical edge lies outside the projection")
    cells = ((z, x - 1, y), (z, x, y)) if axis == 0 else ((z, x, y - 1), (z, x, y))
    low = (min(c[1] for c in cells) - radius, min(c[2] for c in cells) - radius)
    high = (max(c[1] for c in cells) + radius, max(c[2] for c in cells) + radius)
    count = (high[0] - low[0] + 1) * (high[1] - low[1] + 1)
    work = {
        "receipt_lookups": 0,
        "source_cells": count,
        "source_layers": len(LAYERS),
        "radius_cells": radius,
        "source_cell_limit": 49,
        "scope": "finite canonical pair input; no other-edge/corner/global ownership proof",
    }
    state = (
        "needs_projection_contacts"
        if min(low) < 0 or max(high) >= side
        else "input_tile_budget_needs_contacts"
        if count > 49
        else "ready"
    )
    if state != "ready":
        return InterfaceInputs(state, edge, cells, (), (), (), None, work)
    tiles = tuple(
        (z, a, b)
        for a in range(low[0], high[0] + 1)
        for b in range(low[1], high[1] + 1)
    )
    keys, missing, contents = receipt_contents(tiles, receipts, epoch)
    work["receipt_lookups"] = len(keys)
    if missing:
        return InterfaceInputs(
            "pending_sources", edge, cells, tiles, keys, missing, None, work
        )
    version = hashlib.sha256(
        json.dumps(
            [
                "fixed-water-interface-inputs-v1",
                edge,
                radius,
                epoch,
                model_revision,
                contents,
            ],
            sort_keys=True,
            separators=(",", ":"),
        ).encode()
    ).hexdigest()
    return InterfaceInputs("ready", edge, cells, tiles, keys, (), version, work)
