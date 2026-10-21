"""Portable input adapters; assumptions stay separate from supplied MVT facts."""

import json
from pathlib import Path

FIXTURE = Path(__file__).with_name("fixtures") / "mvt_stair_t_junction.json"


def inputs(path=FIXTURE):
    case = json.loads(path.read_text())
    return case, source_rows(case["ways"])


def source_rows(ways):
    rows = []
    for way in ways:
        tags = way["tags"]
        rows.append(
            {
                "source_id": f"native-part:{way['native_way']}",
                "provider_feature_id": way["provider_feature_id"],
                "tile": way["tile"],
                "supplied_tags": tags.copy(),
                "xy_m": way["source_xy_m"],
                "kind": tags["subclass"],
                "half_width_m": way["half_width_m"],
                "layer": way["derived_layer"],
                "bridge": way["derived_bridge"],
                "tunnel": False,
                "foot": tags.get("foot") != "no" and tags.get("access") != "no",
                "endpoint_roles": ["connectable", "connectable"],
                "position_error_bound_m": None,
                "recipe_assumptions": [
                    "ground_tunnel_absence",
                    "public_foot_access_from_path_class",
                    "connectable_stair_endpoints",
                ],
            }
        )
    return rows


def vienna_inputs():
    path = FIXTURE.with_name("stair-context-vienna.json")
    case = json.loads(path.read_text())
    rows = []
    # The fixture supplies an extent, but not a generalization error guarantee.
    # Do not convert tile quantization into a claimed provider error bound.
    for source in case["sources"]:
        properties = source["properties"]
        kind = properties.get("subclass")
        if kind not in ("steps", "path", "footway", "pedestrian"):
            continue
        layer = properties.get("level", 0)
        if type(layer) is not int:
            layer = None
        rows.append(
            {
                "source_id": source["source_id"],
                "xy_m": source["xy_m"],
                "kind": kind,
                "half_width_m": 0.75,
                "layer": layer,
                "bridge": properties.get("brunnel") == "bridge",
                "tunnel": properties.get("brunnel") == "tunnel",
                "foot": properties.get("foot") != "no"
                and properties.get("access") not in ("no", "private"),
                "endpoint_roles": ["connectable", "connectable"],
                "position_error_bound_m": None,
                "supplied_tags": properties,
                "recipe_assumptions": [
                    "width_1.5m",
                    "default_level_0_if_absent",
                    "public_foot_access_from_path_class",
                    "connectable_stair_endpoints",
                ],
            }
        )
    return rows
