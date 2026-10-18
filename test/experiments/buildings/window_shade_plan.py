"""Stable exterior shade slots on the existing core-local opening blueprint."""

from dataclasses import dataclass, replace

import numpy as np
import shapely
from building_layout import rectangle
from building_plan import FLOOR_M
from terraced_facade_prepare import positions

PROGRAMMES = ("eyebrow", "returned-hood")
HEADROOM_M = 2.10
BACK_M = 0.015


@dataclass(frozen=True)
class Shade:
    identifier: str
    window_id: str
    storey: int
    programme: str
    centre: tuple
    tangent: tuple
    width_m: float
    depth_m: float
    bottom_m: float
    cap_bottom_m: float
    top_m: float
    thickness_m: float = 0.07
    back_m: float = BACK_M


@dataclass(frozen=True)
class ShadePlan:
    source_id: str
    programme: str
    slots: tuple
    rejected: tuple


def local_slot(window, storey, programme):
    head = window.bottom_m + window.height_m
    cap_bottom = head + 0.18
    bottom = (
        cap_bottom
        if programme == "eyebrow"
        else max(head - 0.35, storey.bottom_m + FLOOR_M + HEADROOM_M)
    )
    return Shade(
        window.identifier + "/shade/" + programme,
        window.identifier,
        window.storey,
        programme,
        window.centre,
        window.tangent,
        window.width_m + 0.30,
        0.40 if programme == "eyebrow" else 0.60,
        bottom,
        cap_bottom,
        cap_bottom + 0.08,
    )


def conflict(slot, mass):
    storey = mass.storeys[slot.storey]
    if (
        slot.bottom_m < storey.bottom_m + FLOOR_M + HEADROOM_M
        or slot.top_m > storey.top_m - 0.06
    ):
        return "insufficient headroom or source storey height"
    if slot.programme == "returned-hood" and slot.cap_bottom_m - slot.bottom_m < 0.12:
        return "insufficient return height"
    projection = rectangle(
        slot.centre, slot.tangent, slot.width_m, slot.depth_m, slot.back_m
    )
    anchor = rectangle(slot.centre, slot.tangent, slot.width_m, 0, slot.back_m)
    for index, other in enumerate(mass.storeys):
        if other.bottom_m >= slot.top_m or other.top_m <= slot.bottom_m:
            continue
        polygon = shapely.from_wkb(other.footprint_wkb)
        allowed = anchor if index == slot.storey else shapely.Polygon()
        if projection.difference(allowed).intersection(polygon).area > 1e-8:
            return "opposing or intersecting storey in exterior projection"
    if not shapely.from_wkb(storey.footprint_wkb).buffer(1e-8).covers(anchor):
        return "insufficient wall attachment"
    return None


def overlapping_ids(slots):
    if not slots:
        return set()
    footprints = [
        rectangle(s.centre, s.tangent, s.width_m, s.depth_m, s.back_m) for s in slots
    ]
    pairs = shapely.STRtree(footprints).query(footprints, predicate="intersects").T
    rejected = set()
    for a, b in pairs:
        first, second = slots[a], slots[b]
        if (
            a < b
            and min(first.top_m, second.top_m) > max(first.bottom_m, second.bottom_m)
            and footprints[a].intersection(footprints[b]).area > 1e-8
        ):
            rejected.update((first.identifier, second.identifier))
    return rejected


def plan_shades(prepared, programme, *, max_candidates=768):
    if programme not in PROGRAMMES:
        raise ValueError("Unknown window shade programme")
    if type(max_candidates) is not int or max_candidates <= 0:
        raise ValueError("Shade candidate budget must be a positive integer")
    windows = prepared.facade.windows
    if len(windows) > max_candidates:
        raise ValueError("Shade candidate budget exceeded before planning")
    if len({w.identifier for w in windows}) != len(windows):
        raise ValueError("Shade windows require unique identities")
    candidates, slots, rejected = [], [], []
    core = prepared.mass.core
    across, along = np.asarray(core.across_xy), np.asarray(core.along_xy)
    for window in windows:
        slot = local_slot(window, prepared.local.storeys[window.storey], programme)
        reason = conflict(slot, prepared.local)
        if reason:
            rejected.append((slot.identifier, reason))
            continue
        candidates.append(slot)
    overlaps = overlapping_ids(candidates)
    for slot in candidates:
        if slot.identifier in overlaps:
            rejected.append((slot.identifier, "intersecting exterior shade bounds"))
            continue
        centre = positions(np.array([[*slot.centre, 0.0]]), core)[0, :2]
        tangent = slot.tangent[0] * across + slot.tangent[1] * along
        slots.append(replace(slot, centre=tuple(centre), tangent=tuple(tangent)))
    return ShadePlan(prepared.mass.identifier, programme, tuple(slots), tuple(rejected))
