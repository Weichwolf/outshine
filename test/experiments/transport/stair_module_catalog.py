"""Append four-source usage against the old catalog, retaining every original ID."""

import copy
import json

import shapely
from stair_outer_context import area_catalog


def catalog(base, state, rows, sources, layer, domains, context):
    prior = json.loads((base / "source-catalog.json").read_text())
    old = set(state["report"]["after"]["source_ids"])
    new = {r["source_id"] for r in rows}
    if (
        old & new
        or len(sources) != prior["original_line_parts"]
        or prior["payload_sha256"] != context["vector"]["sha256"]
    ):
        raise ValueError(
            "original source inventory or nonoverlapping module membership changed"
        )
    previous = {r["source_id"]: r for r in prior["unadmitted_sources"]}
    if set(previous) != sources.keys() - old or not new <= previous.keys():
        raise ValueError("old source catalog is incomplete")
    result = copy.deepcopy(prior)
    tree = shapely.STRtree(domains)
    remaining = []
    for key, row in previous.items():
        if key in new:
            continue
        row = copy.deepcopy(row)
        source = sources[key]
        if row["properties"] != source["properties"]:
            raise ValueError("original unused source properties changed")
        width = row["width_m"]
        band = source["shape"].buffer(
            width / 2 if width else 32,
            cap_style="flat",
            join_style="mitre",
            mitre_limit=2,
        )
        for i in tree.query(band, predicate="intersects"):
            area = band.intersection(domains[int(i)]).area
            if area > 1e-7:
                row["candidate_overlaps"].append(
                    {"candidate_source_id": rows[int(i)]["source_id"], "area_m2": area}
                )
        row["candidate_overlaps"].sort(key=lambda q: q["candidate_source_id"])
        if row["candidate_overlaps"] and "failed_recipe_trial" not in row:
            row["reason"] = (
                "unplanned_candidate_band_overlap"
                if width
                else "unresolved_profile_potential_overlap"
            )
        remaining.append(row)
    area_updates = area_catalog(layer, tree, domains, rows, context)
    area_prior = {
        (r["feature_id"], r["geometry_type"]): r
        for r in result["unadmitted_non_line_features"]
    }
    for row in area_updates:
        existing = area_prior[(row["feature_id"], row["geometry_type"])]
        existing["candidate_overlaps"].extend(row["candidate_overlaps"])
        existing["candidate_overlaps"].sort(key=lambda q: q["candidate_source_id"])
        if existing["candidate_overlaps"]:
            existing["reason"] = "unplanned_transport_area_overlap"
    result.update(selected_line_parts=len(old | new), unadmitted_sources=remaining)
    return result
