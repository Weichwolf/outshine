"""Actual whole Vienna input as one source-bound instance, never a road recipe."""

import json

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import LocalFrame
from recipe_graph import external
from recipe_graph_data import digest as content_digest
from recipe_transport_values import descriptor, payload_bytes, sealed
from shared_bridge_geometry import corridor
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES
from stair_cycle_contacts import bindings
from stair_cycle_inputs import FIXTURE, load
from stair_junction_layout import INPUT
from stair_network_inputs import FIXTURE as NETWORK
from stair_outer_inputs import FIXTURE as OUTER

SOURCE_TYPE = "transport.source-network/1"
SOUTH = FIXTURE.with_name("stair-cycle-south-vienna.json")


def terrain_corners(rows):
    ground = shapely.union_all(
        [corridor(row) for row in rows if not row["original"]["bridge"]]
    )
    low = np.array(ground.bounds[:2]) - 15
    high = np.array(ground.bounds[2:]) + 15
    return np.array([low, [low[0], high[1]], high, [high[0], low[1]]])


def inputs_for(path=SOUTH):
    case, rows, nodes, stairs, context, sample = load(path)
    if not 1 <= len(rows) <= 63 or len({r["source_id"] for r in rows}) != len(rows):
        raise ValueError("Bounded unique source network required")
    selection = json.loads(path.read_text())
    windows = []
    for row in selection["elevation"]:
        window = path.with_name(row["window_file"])
        if digest(window) != row["window_sha256"]:
            raise ValueError("Actual transport DEM snapshot changed")
        with np.load(window, allow_pickle=False) as saved:
            windows.append({"receipt": row, "arrays": dict(saved)})
    corners = terrain_corners(rows)
    corner_heights = sample(corners)
    receipts = {
        name: digest(source)
        for name, source in (
            ("selection", path),
            ("outer", OUTER),
            ("network", NETWORK),
            ("context", CONTEXT),
            ("native_input", INPUT),
            ("profiles", PROFILES),
            ("original_pbf", INPUT.with_name(case["origin"]["payload_file"])),
        )
    }
    value = sealed(
        {
            "case": case,
            "rows": rows,
            "nodes": nodes,
            "stairs": stairs,
            "context": context,
            "selection": selection,
            "dem_windows": windows,
            "terrain_corners_xy": corners,
            "terrain_corner_priors": corner_heights,
            "protected_profiles": bindings(path, rows),
            "input_receipts": receipts,
        }
    )
    frame_id = "transport-local-" + content_digest(context["origin_lat_lon"])
    LocalFrame(frame_id)
    metadata = {
        "frame_id": frame_id,
        "origin_lat_lon": context["origin_lat_lon"],
        "axes": "east,north,up",
        "units": "m",
        "height_datum": "retained-source-priors-geodetic-datum-unverified",
        "representation": "source-context-json-and-float64-xy-priors-dem",
    }
    return {
        "source": external(
            SOURCE_TYPE, value, descriptor(value), payload_bytes(value), metadata
        )
    }


def validate_source(product):
    value = product.value
    rows = value["rows"]
    if (
        not 1 <= len(rows) <= 63
        or len({row["source_id"] for row in rows}) != len(rows)
        or sum(len(row["xy"]) for row in rows) > 4096
        or len(value["nodes"]) > 128
        or len(value["stairs"]) > 16
    ):
        raise ValueError("Transport source count/work budget exceeded")
    origin = value["context"]["origin_lat_lon"]
    if (
        tuple(product.metadata.get("origin_lat_lon", ())) != tuple(origin)
        or product.metadata.get("frame_id")
        != "transport-local-" + content_digest(origin)
        or product.metadata.get("representation")
        != "source-context-json-and-float64-xy-priors-dem"
    ):
        raise ValueError("Transport source frame/representation disagrees with payload")
    for row in rows:
        xy, prior = row["xy"], row["prior"]
        if (
            xy.dtype != np.dtype("float64")
            or prior.dtype != np.dtype("float64")
            or xy.ndim != 2
            or xy.shape[1] != 2
            or len(xy) < 2
            or prior.shape != (len(xy),)
            or not np.isfinite(xy).all()
            or not np.isfinite(prior).all()
        ):
            raise ValueError(
                "Transport source requires actual finite binary64 axes/priors"
            )
