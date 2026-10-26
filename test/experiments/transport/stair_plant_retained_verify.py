"""Prior identity/selection audit; accepted plants also receive the global full scan."""

import json

import numpy as np
import shapely
from stair_occupancy_geometry import OccupancyError
from stair_plant_prior import previous
from stair_plant_retention import compatible_source, revalidate


def verify_retention(products, data, query, proto, report, rows, graph, place):
    info = report.get("prior_plants")
    notes = report["budget"].get("retention", [])
    if info is None:
        if notes:
            raise ValueError("retained SITE notes lack a verified prior input")
        return
    prior = previous(info["products"], info["receipt"])
    if info != prior["proof"] or len(notes) != len(prior["records"]):
        raise ValueError("complete unchanged prior SITE provenance required")
    if report["prototype_preparation"]["reused"] != prior["compatible"]:
        raise ValueError(
            "declared physical prototype reuse differs from its definition"
        )
    accepted = json.loads((products / "plants.json").read_text())
    by_id = {r["id"]: r for r in rows}
    attempts, kept = 0, 0
    maximum, limit = report["budget"]["max_plants"], report["budget"]["max_attempts"]
    for index, (record, note) in enumerate(zip(prior["records"], notes, strict=True)):
        key = record["source_part_id"]
        identity = record["result"]["site_id"]
        if (note["prior_index"], note["source_part_id"], note["site_id"]) != (
            index,
            key,
            identity,
        ):
            raise ValueError("retained prior index/source/SITE binding changed")
        row = by_id.get(key)
        reason = compatible_source(prior, record, row, data["frame"].identifier)
        if kept >= maximum or attempts >= limit:
            reason = "requested_capacity_or_attempt_budget_fulfilled"
        if reason:
            if note.get("reason") != reason or note["state"] != "deferred":
                raise ValueError(
                    "retained source/frame/definition/budget refusal differs"
                )
            continue
        attempts += 1
        if note["state"] == "prepared":
            if kept >= len(accepted):
                raise ValueError("retained valid SITE missing from accepted products")
            current = accepted[kept]
            if (
                current["source_part_id"] != key
                or (current["source_id"], current["part"])
                != (record["source_id"], record["part"])
                or current["result"]["site_id"] != identity
                or not np.array_equal(
                    current["result"]["world_origin_m"],
                    record["result"]["world_origin_m"],
                )
            ):
                raise ValueError("retained source/SITE position/order was replaced")
            for field in ("lower", "side_m", "region_id"):
                if (
                    current["result"]["habitat"][field]
                    != record["result"]["habitat"][field]
                ):
                    raise ValueError("retained SITE canonical region was reinterpreted")
            for field in ("origin_m", "frame", "plant_recipe", "source_id", "site_id"):
                if (
                    current["result"]["placement"][field]
                    != record["result"]["placement"][field]
                ):
                    raise ValueError(
                        "retained SITE physical placement/form was replaced"
                    )
            kept += 1
        elif note["state"] == "deferred":
            try:
                revalidate(data, query, proto, row, record, place, graph)
            except (OccupancyError, shapely.errors.GEOSException) as error:
                if note.get("reason") != str(error):
                    raise ValueError(
                        "retained physical/local-budget refusal differs"
                    ) from error
            else:
                raise ValueError("valid retained SITE was silently discarded")
        else:
            raise ValueError("explicit retained SITE outcome required")
    if (attempts, kept) != (
        report["budget"]["retained_attempts"],
        report["budget"]["retained_plants"],
    ):
        raise ValueError("retained attempts/accepted product count differs")
