"""Validate stable old SITE products before asking for any new habitat proposals."""

import numpy as np
import shapely
from recipe_plant_sources import DATUM
from stair_occupancy_geometry import OccupancyError, polygonal_conflict


def empty_ledger(rows, rejected, reason):
    return [
        *rejected,
        *[
            {k: v for k, v in row.items() if k != "polygon"}
            | {
                "sites": [],
                "state": "unrequested",
                "reason": reason,
                "unrequested_sites": 0,
                "unrequested_scope": "fresh_proposals",
            }
            for row in rows
        ],
    ]


def local_support(data, row):
    result = data["terrain_patches"].nearby(row["polygon"], 0, max_faces=2048)
    if result.reason:
        raise OccupancyError("retained SITE terrain support: " + result.reason)
    mesh = result.mesh
    shape = polygonal_conflict(
        shapely.union_all(
            shapely.polygons(mesh.vertices[mesh.triangles, :2])
        ).intersection(row["polygon"])
    )
    if shape.is_empty:
        raise OccupancyError("retained SITE source has no current terrain support")
    return shape, dict(result.report)


def compatible_source(prior, record, row, frame):
    if prior["report"]["frame"] != frame:
        return "world_frame_changed"
    if not prior["compatible"]:
        return "plant_definition_or_seed_changed"
    if row is None:
        return "original_source_part_missing"
    old = prior["sources"][record["source_part_id"]]
    if any(
        old[k] != row[k]
        for k in (
            "source_id",
            "part",
            "original_feature",
            "projected_original_wkb",
            "properties",
        )
    ):
        return "original_source_part_changed"
    if row["properties"].get("class") not in ("wood", "grass"):
        return "current_habitat_policy_unsupported"
    return ""


def revalidate(data, query, proto, row, record, place, graph):
    old = record["result"]
    xy = np.asarray(old["world_origin_m"][:2], float)
    if (
        old["adapter"] != "transport-flat-plant-to-current-terrain/1"
        or old["local_height_datum"] != DATUM
        or not np.array_equal(old["placement"]["origin_m"], [*xy, 0.0])
        or not np.array_equal(old["placement"]["frame"], np.eye(3))
    ):
        raise OccupancyError("retained SITE placement contract changed")
    contact = data["sampler"].sample([xy])
    if contact.reasons != ("",):
        raise OccupancyError("retained SITE root contact: " + contact.reasons[0])
    if not np.array_equal(contact.points_enu_m[0], old["world_origin_m"]):
        raise OccupancyError("retained SITE root position changed")
    habitat = old["habitat"]
    canonical = (tuple(habitat["lower"]), habitat["side_m"], habitat["region_id"])
    support, work = local_support(data, row)
    value = place(
        row,
        xy,
        old["site_id"],
        support,
        data,
        query,
        proto,
        canonical,
        graph,
        contact=contact,
    )
    return value, work


def retained_plants(data, query, proto, rows, rejected, canonical, prior, limits):
    from recipe_plant_operators import SITE, prepared
    from stair_plant_sites import candidate, fresh_plants

    maximum, attempts_max = limits
    by_id = {r["id"]: r for r in rows}
    accepted, notes, entries, checked = [], [], {}, set()
    attempts = 0
    graph = None
    for index, record in enumerate(prior["records"]):
        key = record["source_part_id"]
        note = {
            "prior_index": index,
            "source_part_id": key,
            "site_id": record["result"]["site_id"],
        }
        row = by_id.get(key)
        reason = compatible_source(prior, record, row, data["frame"].identifier)
        if len(accepted) >= maximum or attempts >= attempts_max:
            reason = "requested_capacity_or_attempt_budget_fulfilled"
        if reason:
            notes.append(dict(note, state="deferred", reason=reason))
            continue
        attempts += 1
        checked.add((key, tuple(record["result"]["site_id"])))
        try:
            if graph is None:
                graph = prepared(SITE)
            value, work = revalidate(data, query, proto, row, record, candidate, graph)
            entry = {
                "site_id": value["site_id"],
                "root_xy_m": value["world_origin_m"][:2],
                "state": "prepared",
                "plant_index": len(accepted),
                "origin": "retained",
            }
            entries.setdefault(key, []).append(entry)
            accepted.append(
                {
                    "source_id": row["source_id"],
                    "part": row["part"],
                    "source_part_id": key,
                    "result": value,
                }
            )
            notes.append(dict(note, state="prepared", terrain_query=work))
        except (OccupancyError, shapely.errors.GEOSException) as error:
            notes.append(dict(note, state="deferred", reason=str(error)))
    fresh, ledger, budget = fresh_plants(
        data,
        query,
        proto,
        rows,
        rejected,
        canonical,
        maximum - len(accepted),
        attempts_max - attempts,
        excluded=checked,
        graph=graph,
    )
    for entry in ledger:
        for site in entry.get("sites", []):
            if site["state"] == "prepared":
                site["plant_index"] += len(accepted)
        if entry.get("id") in entries:
            entry["sites"] = entries[entry["id"]] + entry["sites"]
            entry["state"] = "sampled"
    budget.update(
        max_plants=maximum,
        max_attempts=attempts_max,
        fresh_attempts=budget["attempts"],
        attempts=attempts + budget["attempts"],
        retained_attempts=attempts,
        retained_plants=len(accepted),
        retention=notes,
        selection="Verified prior SITE products first; new proposals only for unfilled capacity",
    )
    return [*accepted, *fresh], ledger, budget
