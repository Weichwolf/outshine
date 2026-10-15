"""Source-scoped office programme on existing room identities, without fabricated whole-building use."""

import numpy as np
import shapely
from building_layout import AGENT_RADIUS_M
from building_vertical import level
from outshine_ref.buildings.interiors import Furnishing, layout

DESK = Furnishing("workstation", 1.5, 1.15, 1.35, 0.80)
STORAGE = Furnishing("storage", 1.05, 0.40, 1.55, 0.75)
MEETING = Furnishing("meeting-group", 2.4, 2.0, 1.1, 0.80)
TEA = Furnishing("tea-counter", 1.8, 0.65, 1.9, 0.85)
WC = Furnishing("wc", 0.48, 0.72, 0.85, 0.90)
BASIN = Furnishing("washbasin", 0.62, 0.50, 0.92, 0.80)


def office_scope(plan):
    direct = next(
        (
            str(plan["source_properties"][key])
            for key in ("building:use", "building", "subclass", "class")
            if plan["source_properties"].get(key) not in (None, "yes", "building")
        ),
        None,
    )
    if direct == "office":
        return tuple(range(plan["storeys"])), {
            "origin": "supplied-building-use",
            "evidence_ids": (),
        }
    uses = [
        r
        for r in plan.get("context", {}).get("evidence", ())
        if r.get("role") == "office"
        and r.get("properties", {}).get("class") == "office"
    ]
    if not uses:
        return (), {
            "origin": "unavailable",
            "reason": "No actual office building attribute or uniquely hosted office POI",
        }
    explicit = []
    assumed = []
    outside = []
    for row in uses:
        number = row.get("level")
        if number is None:
            assumed.append(row["id"])
            continue
        try:
            value = float(number)
        except (TypeError, ValueError):
            outside.append(row["id"])
            continue
        if not value.is_integer() or not 0 <= value < plan["storeys"]:
            outside.append(row["id"])
        else:
            explicit.append((int(value), row["id"]))
    floors = sorted({i for i, _ in explicit} | ({0} if assumed else set()))
    return tuple(floors), {
        "origin": "local-office-poi",
        "evidence_ids": [r["id"] for r in uses],
        "explicit_levels": explicit,
        "assumed_ground_floor_ids": assumed,
        "out_of_plan_level_ids": outside,
        "poi_positions": [
            {"id": r["id"], "xy": r.get("position_xy"), "level": r.get("level")}
            for r in uses
        ],
        "tenant_extent_origin": "Procedural candidate floor programme, not a measured or supplied tenant boundary",
        "scope": "Local office evidence; candidate floor extent and missing ground level are procedural assumptions, never supplied whole-building use",
    }


def portals(current, index):
    room = current["rooms"][index]
    anchors = []
    reserved = []
    for door in current["doors"]:
        if index not in door["rooms"]:
            continue
        centre = np.asarray(door["centre"])
        tangent = np.asarray(door["tangent"])
        normal = np.array([-tangent[1], tangent[0]])
        options = [centre + normal * 0.55, centre - normal * 0.55]
        inside = next(
            (
                p
                for p in options
                if room.buffer(-AGENT_RADIUS_M, join_style="mitre").covers(
                    shapely.Point(p)
                )
            ),
            None,
        )
        if inside is None:
            raise ValueError("Office portal has no full-radius room-side anchor")
        anchors.append(inside)
        approach = shapely.LineString([centre, inside]).buffer(
            0.42, cap_style="square", join_style="mitre"
        )
        reserved.append(approach)
    if not anchors:
        raise ValueError("Office room has no existing physical portal")
    return anchors, reserved


def requests(kind, area):
    if kind == "meeting":
        return (MEETING,)
    if kind == "tea":
        return (TEA,)
    if kind == "sanitary":
        return (WC, BASIN)
    if kind == "archive":
        return (STORAGE, STORAGE)
    count = min(4, max(1, int(area / 16.0)))
    return tuple([DESK] * count + [STORAGE] + ([TEA] if kind == "work-tea" else []))


def room_kinds(current, eligible, daylight):
    forced = current.get("programme_kinds", {})
    if any(
        i not in eligible or kind not in ("sanitary", "tea")
        for i, kind in forced.items()
    ):
        raise ValueError("Support programme must name actual eligible support rooms")
    kinds = {i: "work" if i in daylight else "archive" for i in eligible} | forced
    choices = daylight.difference(forced)
    if not choices:
        raise ValueError("Office programme lacks a retained daylight meeting room")
    meeting = max(choices, key=lambda i: (current["rooms"][i].area, -i))
    kinds[meeting] = "meeting"
    remaining = [i for i in eligible if i != meeting and i not in forced]
    if len(remaining) >= 2 and "sanitary" not in forced.values():
        sanitary = min(
            remaining, key=lambda i: (i in daylight, current["rooms"][i].area, i)
        )
        kinds[sanitary] = "sanitary"
    working = [i for i in remaining if kinds[i] == "work"]
    if working and "tea" not in forced.values():
        kitchen = max(working, key=lambda i: (current["rooms"][i].area, -i))
        kinds[kitchen] = "work-tea"
    return kinds


def programme(plan, method="wall-anchors", beam_width=4, objective="open-activity"):
    floors, evidence = office_scope(plan)
    records = []
    unresolved = []
    for floor in floors:
        current = level(plan, floor)
        eligible = [
            i for i, k in enumerate(current["room_kinds"]) if k == "generic-room"
        ]
        if len(eligible) < 2:
            unresolved.append(
                {
                    "storey": floor,
                    "reason": "Office programme needs work and shared meeting/support rooms",
                }
            )
            continue
        daylight = {
            i
            for i in eligible
            if any(i in window["rooms"] for window in current["windows"])
        }
        if len(daylight) < 2:
            unresolved.append(
                {
                    "storey": floor,
                    "reason": "Office programme lacks daylight rooms for both work and meeting",
                }
            )
            continue
        kinds = room_kinds(current, eligible, daylight)
        for index in eligible:
            identifier = plan["storey_room_ids"][floor][index]
            anchors, reserved = portals(current, index)
            required = requests(kinds[index], current["rooms"][index].area)
            placement_method = (
                "wall-anchors"
                if index in current.get("programme_kinds", {})
                else method
            )
            result = layout(
                current["rooms"][index],
                anchors,
                required,
                identifier + "/furniture",
                reserved,
                placement_method,
                beam_width=beam_width,
                objective=objective,
            )
            daylight = any(index in window["rooms"] for window in current["windows"])
            record = {
                "room_id": identifier,
                "room_index": index,
                "storey": floor,
                "kind": kinds[index],
                "area_m2": current["rooms"][index].area,
                "floor_z_m": current["floor_z_m"],
                "ceiling_z_m": current["ceiling_z_m"],
                "daylight_opening": daylight,
                "portal_anchors": anchors,
                "reserved": reserved,
                "required": required,
                "placement_method": placement_method,
                "result": result,
                "origin": "Procedural programme fitted to existing connected source room; no claim about actual interior",
            }
            if record["kind"] in ("work", "work-tea", "meeting") and not daylight:
                record["result"] = result | {
                    "status": "incomplete",
                    "reason": "Work/meeting room lacks an actual planned daylight opening",
                }
            records.append(record)
    if not floors:
        unresolved.append(
            {
                "reason": evidence.get(
                    "reason",
                    "Office POI level lies outside the inferred/source storey plan",
                )
            }
        )
    for floor in floors:
        current = [r for r in records if r["storey"] == floor]
        kinds = {r["kind"] for r in current}
        required = (
            "meeting" in kinds,
            "sanitary" in kinds,
            bool(kinds & {"work", "work-tea"}),
            bool(kinds & {"tea", "work-tea"}),
        )
        if not all(required):
            unresolved.append(
                {
                    "storey": floor,
                    "reason": "Office programme misses required meeting, sanitary, work or tea function",
                }
            )
    complete = (
        bool(records)
        and not unresolved
        and all(r["result"]["status"] == "complete" for r in records)
    )
    return {
        "id": plan["id"] + "/office-programme",
        "status": "complete-control" if complete else "incomplete",
        "evidence": evidence,
        "floors": floors,
        "rooms": tuple(records),
        "unresolved": unresolved,
        "method": method,
        "support_placement_method": "wall-anchors",
        "beam_width": beam_width,
        "objective": objective,
        "scope": "Bounded office work/meeting/tea/sanitary furnishing control; services/ventilation/occupancy regulations, other use families and native admission remain open",
    }
