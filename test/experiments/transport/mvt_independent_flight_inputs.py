"""Declared two-network controls derived from retained inputs, not new observations."""

import copy
import json

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connection_inputs import inputs as original_inputs
from mvt_t_connections import plan_connections
from stair_test_transform import rigid_coordinates


def _move(case, native, coordinates, rotation, height_m):
    def xyz(value):
        result = np.array(coordinates(value))
        if height_m:
            result[:, 2] += height_m
        return result.tolist()

    for way in case["ways"]:
        way["source_xy_m"] = coordinates(way["source_xy_m"])
        for key in ("sampled_design_xyz_m", "prepared_stations_xyz_m"):
            way[key] = xyz(way[key])
    for cut in native["cuts"]:
        cut["center_xy_m"] = coordinates(cut["center_xy_m"])
        cut["original_source_xy_m"] = coordinates(cut["original_source_xy_m"])
        cut["tangent_xy"] = (np.array(cut["tangent_xy"]) @ rotation.T).tolist()
        for triangle in cut["triangles"]:
            triangle["enu_m"] = xyz(triangle["enu_m"])


def inputs(
    arrangement="stacked",
    *,
    height_m=10.0,
    global_pose=None,
    cut_scope="flight_footprint",
):
    if arrangement not in ("stacked", "crossed") or not np.isfinite(height_m):
        raise ValueError("known finite controlled two-flight placement required")
    case, lower = original_inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    upper_case, upper_native = copy.deepcopy(case), copy.deepcopy(native)
    stair = next(s for s in lower if s["kind"] == "steps")
    centre = np.asarray(stair["xy_m"]).mean(axis=0)
    angle = 0.0 if arrangement == "stacked" else np.pi / 2
    rotation = np.array(
        [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
    )
    translation = centre - centre @ rotation.T
    coordinates, rotation = rigid_coordinates(angle, translation)

    _move(upper_case, upper_native, coordinates, rotation, height_m)
    mapping = {}
    for way in upper_case["ways"]:
        previous = way["native_way"]
        way["native_way"] = "derived-upper:" + str(previous)
        mapping[f"native-part:{way['native_way']}"] = f"native-part:{previous}"
        way["derived_from_native_way"] = previous
    for cut in upper_native["cuts"]:
        cut["derived_from_native_way"] = cut["native_way"]
        cut["native_way"] = "derived-upper:" + str(cut["native_way"])
    original_provenance = copy.deepcopy(case["provenance"])
    case["ways"].extend(upper_case["ways"])
    native["cuts"].extend(upper_native["cuts"])
    if global_pose is not None:
        posed, pose_rotation = rigid_coordinates(
            global_pose[0], np.asarray(global_pose[1])
        )
        _move(case, native, posed, pose_rotation, 0)
    lower = source_rows(case["ways"][: len(lower)])
    upper = source_rows(case["ways"][len(lower) :])
    for source in upper:
        source["derived_from_source_id"] = mapping[source["source_id"]]
        source["recipe_assumptions"].append(
            "controlled_upper_instance_not_a_provider_observation"
        )
    subplans = [plan_connections(s) for s in (lower, upper)]
    if any(len(p["connections"]) != 2 for p in subplans):
        raise ValueError("each controlled network must qualify both own stair ports")
    proposal = {
        "status": "controlled_independent_networks_pending_full_body_admission",
        "station_cut_scope": cut_scope,
        "connections": [c for p in subplans for c in p["connections"]],
        "axes_xy_m": {k: v for p in subplans for k, v in p["axes_xy_m"].items()},
        "independent_subplans": subplans,
    }
    derivation = {
        "kind": "controlled_two_network_composition_not_additional_provider_evidence",
        "arrangement": arrangement,
        "upper_source_mapping": mapping,
        "upper_xy_rotation": rotation.tolist(),
        "upper_xy_translation_m": translation.tolist(),
        "upper_height_translation_m": height_m,
        "original_provenance": original_provenance,
        "global_pose": global_pose,
    }
    case["provenance"]["controlled_derivation"] = derivation
    native["provenance"]["controlled_derivation"] = derivation
    return case, lower + upper, proposal, native, derivation


def prepared(
    arrangement="stacked",
    *,
    height_m=10.0,
    global_pose=None,
    cut_scope="flight_footprint",
):
    case, sources, proposal, native, derivation = inputs(
        arrangement, height_m=height_m, global_pose=global_pose, cut_scope=cut_scope
    )
    plan, contacts = prepare_native_body_plan(case, sources, proposal, native)
    upper = derivation["upper_source_mapping"]
    lower = [
        source["source_id"] for source in sources if source["source_id"] not in upper
    ]
    for row in plan["rows"]:
        if row["source_id"] in upper:
            row["separation_below_source_ids"] = lower.copy()
    for contact in contacts:
        if contact["source_id"] in upper or global_pose is not None:
            contact["provenance"] = (
                "Controlled rigid/height transform of retained native profile; no new native/provider observation. See controlled_derivation."
            )
    plan["controlled_derivation"] = derivation
    for source in sources:
        if source["source_id"] in upper or global_pose is not None:
            plan["source_metadata"][source["source_id"]]["controlled_derivation"] = {
                "original_source_id": upper.get(
                    source["source_id"], source["source_id"]
                ),
                "instance": "upper"
                if source["source_id"] in upper
                else "rigid_control",
            }
    return plan, contacts
