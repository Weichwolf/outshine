"""Height-resolved original habitat proposals consumed by the existing SITE recipe."""

import numpy as np
import shapely
from outshine_ref.contracts import BuildingFootprint, LocalFrame
from outshine_ref.vegetation import HabitatStand, StandHabitat
from recipe_graph_data import plain
from recipe_plant_operators import SITE, prepared
from recipe_plant_sources import DATUM, RootSite, site_input
from recipe_plant_values import seed_input
from stair_consumer_inputs import source_rows
from stair_occupancy_geometry import OccupancyError, polygonal_conflict
from stair_plant_geometry import bounds_query


def region(data):
    xy = data["terrain"].vertices[:, :2]
    lower = np.floor(xy.min(axis=0))
    side = float(2 ** np.ceil(np.log2((xy.max(axis=0) - lower).max() + 1)))
    return tuple(lower), side, "Wien-transport-terrain-near-v1"


def terrain_domain(data):
    xy = data["terrain"].vertices[data["terrain"].triangles, :2]
    return polygonal_conflict(shapely.union_all(shapely.polygons(xy)))


def candidate(row, xy, site_id, support, data, query, proto, canonical, graph):
    contact = data["sampler"].sample([xy])
    if contact.reasons != ("",):
        raise OccupancyError("current terrain contact: " + contact.reasons[0])
    origin = contact.points_enu_m[0]
    body = bounds_query(row["id"], proto["bounds_m"], origin)
    root_shape = BuildingFootprint(row["id"], support, body.heights)
    cuts, cost = query.conflict(root_shape)
    resolved = polygonal_conflict(support.difference(cuts))
    if not resolved.covers(shapely.Point(xy)):
        raise OccupancyError("actual plant-height band blocks root support")
    conflict, checked = query.conflict(body)
    if not conflict.is_empty:
        raise OccupancyError("whole measured plant body enters transport/building")
    lower, side, region_id = canonical
    frame = LocalFrame(
        data["frame"].identifier
        + ":flat-plant-root:"
        + row["id"]
        + ":site:"
        + ",".join(map(str, site_id))
    )
    habitat = StandHabitat.prepare(
        frame,
        row["source_id"],
        region_id,
        row["polygon"],
        lower,
        side,
        support=resolved,
    )
    site = RootSite(
        habitat,
        {
            "frame": frame.identifier,
            "source_id": row["source_id"],
            "region_id": region_id,
            "source_part": row["part"],
            "original_properties": row["properties"],
            "world_frame": data["frame"].identifier,
        },
        tuple(int(v) for v in site_id),
        np.r_[xy, 0.0],
        np.eye(3),
    )
    result = graph.evaluate(
        {"seed": seed_input(718), "site": site_input(site)},
        outputs=("placement", "plant"),
    )
    if plain(result.outputs["plant"].value) != proto["recipe"]:
        raise ValueError("placed real plant recipe differs from measured whole body")
    return {
        "site_id": list(site.site_id),
        "world_origin_m": origin.tolist(),
        "terrain_face": int(contact.face_indices[0]),
        "terrain_normal": contact.normals_enu[0].tolist(),
        "world_frame": data["frame"].identifier,
        "local_height_datum": DATUM,
        "adapter": "transport-flat-plant-to-current-terrain/1",
        "transform": "world Z = unchanged flat SITE Z + actual root Z; XY retained",
        "habitat": habitat.recipe(),
        "placement": plain(result.outputs["placement"].value),
        "placement_metadata": dict(result.outputs["placement"].metadata),
        "query_heights_m": [body.heights.lower_m, body.heights.upper_m],
        "query_geometry_wkb": body.geometry.wkb_hex,
        "root_support_query": cost,
        "whole_body_query": checked,
    }


def proposals(data, proto, domain, canonical, rows, query):
    lower, side, region_id = canonical
    ledger, pool = [], []
    bounds = np.asarray(proto["bounds_m"])
    original_order = {r["id"]: i for i, r in enumerate(rows)}

    def distance(row):
        i = int(query.tree.nearest(row["polygon"]))
        return float(row["polygon"].distance(query.shapes[i])), row["id"]

    for row in sorted(rows, key=distance):
        entry = {k: v for k, v in row.items() if k != "polygon"}
        entry["sites"] = []
        try:
            if row["properties"].get("class") not in ("wood", "grass"):
                raise OccupancyError(
                    "young planting pilot requires supplied grass/wood habitat"
                )
            if len(pool) + 16 > 1024:
                raise OccupancyError("explicit proposal budget exhausted")
            support = polygonal_conflict(row["polygon"].intersection(domain))
            if support.is_empty:
                raise OccupancyError("original habitat outside current terrain")
            part_region = region_id + ":" + row["id"]
            habitat = StandHabitat.prepare(
                data["frame"],
                row["source_id"],
                part_region,
                row["polygon"],
                lower,
                side,
                support=support,
            )
            stand = HabitatStand(
                habitat,
                ("pinus-sylvestris-young-pilot",),
                (16,),
                (float(bounds[1, 2] - bounds[0, 2]),),
                (float(np.ptp(bounds[:, :2], axis=0).max()),),
                levels=0,
                seed=718,
            )
            sites = stand.sites((0, 0, 0), max_sites=16, max_triangles=32768)
            if sites is None:
                raise OccupancyError("bounded canonical habitat sites unresolved")
            for xy, identity in zip(sites["positions_xy_m"], sites["ids"], strict=True):
                point = shapely.Point(xy)
                nearest = int(query.tree.nearest(point))
                distance = float(point.distance(query.shapes[nearest]))
                pool.append(
                    (
                        distance,
                        row["id"],
                        tuple(map(int, identity)),
                        xy,
                        row,
                        support,
                        (lower, side, part_region),
                        entry,
                    )
                )
            entry.update(
                state="proposed",
                unrequested_sites=16,
                unrequested_reason="explicit accepted-plant/attempt budget",
            )
        except (OccupancyError, ValueError, shapely.errors.GEOSException) as error:
            entry.update(state="deferred", reason=str(error))
        ledger.append(entry)
    return sorted(pool, key=lambda p: p[:3]), sorted(
        ledger, key=lambda r: original_order[r["id"]]
    )


def plants(data, query, proto, *, max_plants=4, max_attempts=128):
    if (
        type(max_plants) is not int
        or not 0 <= max_plants <= 64
        or type(max_attempts) is not int
        or not 0 <= max_attempts <= 1024
    ):
        raise ValueError("explicit bounded plant and attempt counts required")
    rows, rejected = source_rows(data, "landcover")
    canonical = region(data)
    pool, ledger = proposals(data, proto, terrain_domain(data), canonical, rows, query)
    graph = prepared(SITE)
    accepted, attempts = [], 0
    for distance, _key, identity, xy, row, support, part_canonical, entry in pool:
        if len(accepted) >= max_plants or attempts >= max_attempts:
            break
        attempts += 1
        record = {
            "site_id": list(identity),
            "root_xy_m": xy.tolist(),
            "nearest_volume_xy_distance_m": distance,
        }
        try:
            value = candidate(
                row, xy, identity, support, data, query, proto, part_canonical, graph
            )
            record.update(state="prepared", plant_index=len(accepted))
            accepted.append(
                {
                    "source_id": row["source_id"],
                    "part": row["part"],
                    "source_part_id": row["id"],
                    "result": value,
                }
            )
        except (OccupancyError, shapely.errors.GEOSException) as error:
            record.update(state="deferred", reason=str(error))
        entry["sites"].append(record)
        entry["state"] = "sampled"
        entry["unrequested_sites"] -= 1
    return (
        accepted,
        [*rejected, *ledger],
        {
            "attempts": attempts,
            "max_attempts": max_attempts,
            "max_plants": max_plants,
            "canonical_region": canonical,
            "proposals": len(pool),
            "max_proposals": 1024,
            "habitat_policy": "Procedural young Pinus planting on supplied grass/wood, no botanical observation; source and root proposals nearest actual volumes first",
            "selection": "nearest XY distance to actual transport/building volumes, then source-part/site key; height/crown still checked",
        },
    )
