"""Bounded whole original sources and verified bodies; no old network planning."""

import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from capture_stair_context import parts
from outshine_ref.places import decode_vector
from outshine_ref.transport.horizontal import plan_horizontal_axis
from stair_context_inputs import FIXTURE as CONTEXT
from stair_context_inputs import PROFILES
from stair_cycle_inputs import sampler
from stair_junction_layout import INPUT
from stair_module_roads import road_profile
from wien_support_verify import unpack

FIXTURE = Path(__file__).with_name("fixtures") / "stair-west-ground-module-vienna.json"


def read_arrays(path):
    with np.load(path, allow_pickle=False) as archive:
        return {key: archive[key] for key in archive.files}


def cached(base, supports, recipe):
    report = json.loads((base / "comparison.json").read_text())
    replay = json.loads((base / "verification.json").read_text())
    source_recipe = FIXTURE.with_name(recipe["reference_fixture"])
    if (
        digest(source_recipe) != recipe["reference_fixture_sha256"]
        or report["fixture_sha256"] != digest(source_recipe)
        or replay.get("valid_local_candidate") is not True
        or replay["products_report_sha256"] != digest(base / "comparison.json")
        or report["after"]["body_sha256"] != digest(base / "after/body.npz")
        or report["source_catalog_sha256"] != digest(base / "source-catalog.json")
        or report["after"]["presentation"]["mesh_sha256"]
        != digest(base / "after/display-body.npz")
        or not 1 <= len(report["after"]["source_ids"]) <= 32
    ):
        raise ValueError(
            "verified bounded source baseline and original recipe required"
        )
    hashes = dict(
        report["artifacts"], **{"after/body.npz": report["after"]["body_sha256"]}
    )
    support_report = json.loads((supports / "comparison.json").read_text())
    support_replay = json.loads((supports / "verification.json").read_text())
    method = support_report["selected"]
    if (
        support_report["baseline_report_sha256"] != digest(base / "comparison.json")
        or support_replay.get("valid_local_candidate") is not True
        or support_replay["products_report_sha256"]
        != digest(supports / "comparison.json")
        or support_report["methods"][method]["mesh_sha256"]
        != digest(supports / (method + ".npz"))
        or support_report["display"]["after_display_sha256"]
        != digest(supports / "after-display.npz")
    ):
        raise ValueError("verified unchanged support baseline required")
    for name, expected in {**hashes, **support_report["baseline_artifacts"]}.items():
        if digest(base / name) != expected:
            raise ValueError("cached original artifact changed: " + name)
    bodies, contacts, _arrays = unpack(supports / (method + ".npz"))
    if len(bodies) > 128:
        raise ValueError("cached support count cap")
    return {
        "report": report,
        "support_report": support_report,
        "road": read_arrays(base / "after/body.npz"),
        "plan": read_arrays(base / "terrain-plan.npz"),
        "heights": read_arrays(base / "membrane-field.npz")["heights"],
        "piers": bodies,
        "pier_contacts": contacts,
        "hashes": hashes,
        "baseline_report_sha256": digest(base / "comparison.json"),
        "support_report_sha256": digest(supports / "comparison.json"),
    }


def source_context(source_recipe):
    context = json.loads(CONTEXT.read_text())
    native = json.loads(INPUT.read_text())
    receipt = dict(
        context["vector"], path=str(INPUT.with_name(native["origin"]["payload_file"]))
    )
    layer = decode_vector(receipt)["transportation"]
    sources = parts(
        layer,
        tuple(receipt["tile"]),
        np.array(context["origin_lat_lon"]),
    )
    sample = sampler(source_recipe, json.loads(source_recipe.read_text()), context)
    return context, sources, sample, layer


def reference_profiles():
    return {
        p["kind"]: p
        for template in json.loads(PROFILES.read_text())["templates"]
        for p in template.get("osm", [])
        if p.get("layer") == "streets"
    }


def inputs(path=FIXTURE):
    recipe = json.loads(path.read_text())
    if recipe.get("version") != 1:
        raise ValueError("known module recipe version required")
    source_recipe = FIXTURE.with_name(recipe["reference_fixture"])
    if digest(source_recipe) != recipe["reference_fixture_sha256"]:
        raise ValueError("original reference fixture changed")
    context, sources, sample, layer = source_context(source_recipe)
    keys = recipe["source_ids"]
    if (
        not isinstance(keys, list)
        or not 1 <= len(keys) <= 8
        or len(set(keys)) != len(keys)
    ):
        raise ValueError("one to eight unique whole new sources required")
    profiles = reference_profiles()
    rows = []
    bindings = recipe.get("source_profiles", {})
    if not isinstance(bindings, dict) or not set(bindings) <= set(keys):
        raise ValueError("explicit profiles may bind only selected original sources")
    for key in keys:
        source = sources[key]
        props = source["properties"]
        kind = props.get("subclass")
        extra = {}
        if key in bindings:
            kind = bindings[key]
            extra = road_profile(source, profiles.get(kind, {}), kind)
        if (
            (kind not in ("cycleway", "footway") and not extra)
            or props.get("brunnel")
            or props.get("indoor")
            or props.get("layer", 0) != 0
            or props.get("level", 0) != 0
        ):
            raise ValueError("ordinary exterior ground source required")
        xy = np.array(source["xy_m"])
        if extra and "road_axis" in recipe:
            axis, note = plan_horizontal_axis(xy, recipe["road_axis"])
            note["original_corridor_retained"] = True
            extra.update(surface_axis_xy_m=axis, horizontal_curve_plan=note)
        rows.append(
            {
                "source_id": key,
                "xy": xy,
                "prior": sample(xy),
                "width_m": profiles[kind]["widthM"],
                "gradient": profiles[kind]["maxGradient"],
                "original": {
                    "bridge": False,
                    "source": {"feature": source["original_feature"]},
                },
                **extra,
            }
        )
    return recipe, rows, context, sources, sample, layer
