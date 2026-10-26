"""Bounded local uses on existing ground cells; frontage placement remains inferred."""

import math

import numpy as np
import shapely
from outshine_ref.buildings.interiors.geometry import rectangle


def ground_value(value):
    if value is None:
        return True
    if isinstance(value, bool):
        return False
    try:
        return math.isfinite(float(value)) and float(value) == 0
    except (ValueError, TypeError):
        return False


def local_uses(row):
    result, rejected = [], []
    for e in row.get("context", {}).get("evidence", ()):
        if e.get("role") not in ("retail", "food"):
            continue
        reason = None
        if e.get("relation") != "unique-point-host" or e.get("scope") != "partial-use":
            reason = "Local use lacks a unique containing host"
        elif not ground_value(e.get("level")) or not ground_value(
            e.get("vertical_layer")
        ):
            reason = "Local use belongs to a non-ground or ambiguous level"
        else:
            xy = np.asarray(e.get("position_xy", ()))
            if xy.shape != (2,) or not np.isfinite(xy).all():
                reason = "Local use lacks a finite source position"
            elif not row["polygon"].covers(shapely.Point(xy)):
                reason = "Local use position is outside its source host"
        if reason:
            rejected.append({"id": e["id"], "source_id": row["id"], "reason": reason})
        else:
            result.append(e)
    return sorted(result, key=lambda e: e["id"]), rejected


class ShopfrontDetails:
    def __init__(self, entrances):
        self.entrances = entrances
        self.planned, self.rejected = [], []

    def plan(self, row, entry):
        uses, rejected = local_uses(row)
        self.rejected.extend(rejected)
        if not uses:
            return None
        try:
            if len(uses) > 64:
                raise ValueError("Shopfront exceeds 64 source-use candidates")
            if entry is None:
                raise ValueError(
                    "Shopfront has no existing valid ground-floor entrance"
                )
            frame = entry["frame"]
            width, floor = entry["cell_width_m"], entry["floor_m"]
            if frame["columns"] > 256:
                raise ValueError("Shopfront exceeds 256 existing facade columns")
            if width < 1.8 or floor < 2.6:
                raise ValueError("Existing ground cell cannot hold a shopfront")
            a, tangent = np.asarray(frame["a"]), np.asarray(entry["door"]["tangent"])
            cells = {}
            for use in uses:
                u = (np.asarray(use["position_xy"]) - a) @ tangent
                centre = int(np.clip(math.floor(u / width), 0, frame["columns"] - 1))
                for column in range(
                    max(0, centre - 1), min(frame["columns"], centre + 2)
                ):
                    if column != entry["column"]:
                        cells.setdefault(column, []).append(use)
            if len(cells) > 32:
                raise ValueError("Shopfront exceeds 32 existing ground cells")
            accepted = []
            for column, evidence in sorted(cells.items()):
                centre = a + (column + 0.5) * width * tangent
                slab = rectangle(centre, tangent, width, 0.0, 0.18)
                outside = rectangle(centre, tangent, width, 0.02, 0.0)
                if slab.difference(
                    row["polygon"]
                ).area > 1e-8 or not self.entrances.clear(row, outside, floor):
                    self.rejected.append(
                        {
                            "id": frame["id"],
                            "cell": [column, 0],
                            "reason": "Shopfront crosses footprint or a known neighbor",
                        }
                    )
                    continue
                role = (
                    "retail" if any(e["role"] == "retail" for e in evidence) else "food"
                )
                accepted.append(
                    {
                        "id": frame["id"] + f"/cell-{column}-0/shopfront",
                        "column": column,
                        "role": role,
                        "fractions": [0.88, 0.80] if role == "retail" else [0.82, 0.74],
                        "evidence_ids": sorted(e["id"] for e in evidence),
                        "level_origin": "procedural-ground-assumption"
                        if any(e.get("level") is None for e in evidence)
                        else "supplied-ground-level",
                    }
                )
            if not accepted:
                raise ValueError("No free existing shopfront cell beside the entrance")
            result = {"frame": frame, "cells": accepted, "entry": entry}
            self.planned.append(
                {
                    "source_id": row["id"],
                    "facade_id": frame["id"],
                    "entrance_id": entry["id"],
                    "cells": accepted,
                    "placement_origin": "procedural placement on the existing entrance facade; source POI is not a surveyed frontage",
                    "scope": "Local ground-floor display fronts; no claim about whole-building use or a complete shop programme",
                }
            )
            return result
        except ValueError as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            return None
