"""Bounded explicit module parents, retaining their actual bodies and source IDs."""

import copy
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from stair_cycle_contacts import profile
from stair_module_inputs import cached, read_arrays
from stair_module_materials import colour_component


def merge_road(road, module, count):
    """Concatenate immutable components; metadata bottoms retain top ordering."""
    if count + module["owners"].shape[1] > 64:
        raise ValueError("cached source-mask capacity exhausted")
    offset = len(road["body_vertices"])
    n = len(module["surface_vertices"])
    owners = module["owners"].astype(np.uint64) @ (
        np.uint64(1)
        << np.arange(count, count + module["owners"].shape[1], dtype=np.uint64)
    )
    return {
        "body_vertices": np.vstack([road["body_vertices"], module["body_vertices"]]),
        "body_faces": np.vstack([road["body_faces"], module["body_faces"] + offset]),
        "top_faces": np.vstack([road["top_faces"], module["surface_faces"] + offset]),
        "bottom_faces": np.vstack(
            [road["bottom_faces"], module["surface_faces"] + n + offset]
        ),
        "face_sources": np.r_[road["face_sources"].astype(np.uint64), owners],
    }


def parent_report(path, fixture, state, chain):
    report = json.loads((path / "comparison.json").read_text())
    verification = json.loads((path / "verification.json").read_text())
    recipe = json.loads(fixture.read_text())
    keys = recipe["source_ids"]
    required = {
        "module.npz",
        "local-layout.npz",
        "local-field.npz",
        "world-layout.npz",
        "world-field.npz",
        "world-body.npz",
        "selection.npz",
        "planning.json",
        "source-catalog.json",
        *(
            phase + "-" + view + "-scene.json"
            for phase in ("before", "after")
            for view in ("overview", "network")
        ),
    }
    if (
        report.get("valid_local_module") is not True
        or verification.get("valid_local_module") is not True
        or verification["products_report_sha256"] != digest(path / "comparison.json")
        or report["fixture_sha256"] != digest(fixture)
        or report["base_report_sha256"] != state["baseline_report_sha256"]
        or report["support_report_sha256"] != state["support_report_sha256"]
        or report.get("cache_parent_reports", []) != chain
        or report["origin_lat_lon"] != state["support_report"]["origin_lat_lon"]
        or report["candidate_source_ids"]
        != state["report"]["candidate_source_ids"] + keys
        or not 1 <= len(keys) <= 8
        or set(keys) & set(state["report"]["candidate_source_ids"])
        or report["admitted_source_ids"]
        or verification["admitted_source_ids"]
        or set(report["artifacts"]) != required
    ):
        raise ValueError("cached module chain/source/frame/reader binding changed")
    for name, expected in report["artifacts"].items():
        if digest(path / name) != expected:
            raise ValueError("cached module artifact changed: " + name)
    for phase in ("before", "after"):
        if (
            digest(path / (phase + "-display.npz"))
            != report["display"][phase + "_display_sha256"]
        ):
            raise ValueError("cached module presentation changed")
    if digest(path / "before-display.npz") != digest(state["before_display_path"]):
        raise ValueError("cached module lost previous complete display")
    return report, recipe


def remaining_outer_profiles(profiles, recipe):
    consumed = {
        (c["cached_source_id"], 0 if c["cached_vertex"] == 0 else -1)
        for c in recipe.get("endpoint_contacts", [])
    }
    return [
        p
        for p in profiles
        if ("vertex" in recipe["port"] or p["source_id"] != recipe["port"]["source_id"])
        and (p["source_id"], p.get("end")) not in consumed
    ]


def extend(state, path, fixture, chain, palette):
    report, recipe = parent_report(path, fixture, state, chain)
    module = read_arrays(path / "module.npz")
    saved = json.loads((path / "planning.json").read_text())["rows"]
    keys = recipe["source_ids"]
    if [r["source_id"] for r in saved] != keys or module["owners"].shape[1] != len(
        keys
    ):
        raise ValueError("cached module owner order changed")
    state.setdefault("cached_module_rows", {}).update(
        {r["source_id"]: r for r in saved}
    )
    old = state["road"]
    if "road_ground" not in state:
        xyz = np.c_[state["plan"]["xy"], state["heights"]]
        known = {
            p.tobytes() for p in xyz[state["plan"]["faces"][state["plan"]["ground"]]]
        }
        state["road_ground"] = np.array(
            [p.tobytes() in known for p in old["body_vertices"][old["bottom_faces"]]]
        )
        if state["road_ground"].sum() != state["plan"]["ground"].sum():
            raise ValueError("original cached ground ownership changed")
    outer = remaining_outer_profiles(
        state["report"]["after"]["proposed_outer_profiles"], recipe
    )
    for p in report["module"]["profiles"]["outer_profiles"]:
        i = keys.index(p["source_id"])
        row = dict(saved[i], xy=np.array(saved[i]["xy"]))
        cap = Mesh(
            module["surface_vertices"], module["surface_faces"][module["owners"][:, i]]
        )
        line = (
            SurfaceSampler(cap, xy_tolerance_m=1e-7)
            .sample(profile(row, p["end"]), max_queries=3, max_candidates=65536)
            .points_enu_m
        )
        if not np.isfinite(line).all():
            raise ValueError("cached whole outer profile lacks actual source cap")
        outer.append(dict(p, proposed_vertices_enu_m=line.tolist()))
    state["road"] = merge_road(
        old, module, len(state["report"]["candidate_source_ids"])
    )
    state["road_ground"] = np.r_[
        state["road_ground"], np.ones(len(module["surface_faces"]), bool)
    ]
    state["plan"] = read_arrays(path / "world-layout.npz")
    state["heights"] = read_arrays(path / "world-field.npz")["heights"]
    state["report"] = {
        "after": {
            "source_ids": report["candidate_source_ids"],
            "proposed_outer_profiles": outer,
        },
        "candidate_source_ids": report["candidate_source_ids"],
    }
    state["baseline_report_sha256"] = digest(path / "comparison.json")
    state["catalog_path"] = path / "source-catalog.json"
    state["before_display_path"] = path / "after-display.npz"
    first_face = len(state["road_display"]["triangles"])
    state["road_display"] = append(
        state["road_display"],
        [(Mesh(module["body_vertices"].astype("f4"), module["body_faces"]), 3)],
        palette,
    )
    colour_component(state["road_display"], first_face, module, palette)
    chain.append(
        {
            "report_sha256": state["baseline_report_sha256"],
            "fixture_sha256": digest(fixture),
        }
    )


def load_cached(args, recipe):
    parents = getattr(args, "cached_module", []) or []
    if len(parents) > 4 or len({Path(p).resolve() for p, _ in parents}) != len(parents):
        raise ValueError(
            "at most four unique explicitly ordered cached modules required"
        )
    state = cached(args.base, args.supports, recipe)
    state["catalog_path"] = args.base / "source-catalog.json"
    state["before_display_path"] = args.supports / "after-display.npz"
    state["road_display"] = read_arrays(args.base / "after/display-body.npz")
    palette = np.array(
        json.loads((args.supports / "after-overview-scene.json").read_text())["colours"]
    )
    chain = []
    for path, fixture in parents:
        extend(state, Path(path), Path(fixture), chain, palette)
    state["cache_parent_reports"] = copy.deepcopy(chain)
    return state
