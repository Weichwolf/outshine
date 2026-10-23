"""Explicit independent-layer operator control over the retained native cut input."""

import copy
import json
from unittest.mock import patch

import numpy as np
from mvt_native_body_layout import prepare_native_body_plan
from mvt_t_connection_inputs import FIXTURE, inputs, source_rows
from mvt_t_connections import plan_connections
from stair_test_transform import rigid_coordinates


def prepared(angle=0, translation=(0, 0), *, independent=True, permute=False):
    case, _sources = inputs()
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    coordinates, rotation = rigid_coordinates(angle, np.array(translation))
    for way in case["ways"]:
        for key in ("source_xy_m", "sampled_design_xyz_m", "prepared_stations_xyz_m"):
            way[key] = coordinates(way[key])
    for cut in native["cuts"]:
        cut["center_xy_m"] = coordinates(cut["center_xy_m"])
        cut["original_source_xy_m"] = coordinates(cut["original_source_xy_m"])
        cut["tangent_xy"] = (np.array(cut["tangent_xy"]) @ rotation.T).tolist()
        for triangle in cut["triangles"]:
            triangle["enu_m"] = coordinates(triangle["enu_m"])
    sources = source_rows(case["ways"])
    with patch("mvt_t_profile_layout.prepare_stair_lofts", return_value=None):
        plan, contacts = prepare_native_body_plan(
            case, sources, plan_connections(sources), native
        )
    if independent:
        owner = plan["stairs"][0][0]
        row = copy.deepcopy(plan["rows"][owner])
        row["source_id"] = "counterexample-upper:" + str(row["source_id"])
        row["original"]["bridge"] = True
        row["separation_below_source_ids"] = [r["source_id"] for r in plan["rows"]]
        row["prior"] += 10
        row["height_prior_xyz_m"][:, 2] += 10
        plan["rows"].append(row)
        vertices = np.unique(plan["parts"][owner])
        if permute:
            vertices = vertices[::-1]
        offset = len(plan["xy"])
        remap = dict(zip(map(int, vertices), range(offset, offset + len(vertices))))
        plan["xy"] = np.vstack([plan["xy"], plan["xy"][vertices]])
        faces = np.vectorize(remap.__getitem__)(plan["parts"][owner])
        plan["parts"].append(np.roll(faces, 1, axis=1) if permute else faces)
        plan["part_global_faces"].append(plan["part_global_faces"][owner].copy())
        plan["prepared_domains"].append(plan["prepared_domains"][owner])
    return plan, contacts
