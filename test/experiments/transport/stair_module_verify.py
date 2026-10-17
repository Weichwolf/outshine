"""Read actual complete module products; no height fitting or mesh rebuilding."""

import json
import resource
import time

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from shared_bridge_geometry import corridor
from stair_module_audit import audit
from stair_module_cache import load_cached
from stair_module_catalog import catalog
from stair_module_experiment import arguments, refused_catalog, write_json
from stair_module_geometry import old_surfaces
from stair_module_inputs import inputs, read_arrays
from stair_module_materials import body_slots
from stair_module_soil import selection
from stair_module_source_contract import verify_source_contract
from stair_module_world import field_link, inspect_world, source_ground
from stair_terrain_experiment import frozen


def saved_planning(output, original, ground):
    saved = json.loads((output / "planning.json").read_text())
    rows = saved["rows"]
    if len(rows) != len(original):
        raise ValueError("whole saved source membership changed")
    for before, after in zip(original, rows, strict=True):
        for key in ("source_id", "width_m", "gradient"):
            if before[key] != after[key]:
                raise ValueError("source profile or identity changed")
        for key in (
            "source_recipe",
            "profile_kind",
            "crossfall_limit",
            "traffic_profile",
        ):
            if before.get(key) != after.get(key):
                raise ValueError("explicit procedural road profile changed")
        for key in ("xy", "prior"):
            after[key] = np.array(after[key])
            if not np.array_equal(before[key], after[key]):
                raise ValueError("original axis or DEM prior changed")
        after["original"] = before["original"]
    domains = [corridor(row) for row in rows]
    old = shapely.union_all(shapely.polygons(ground.vertices[ground.triangles, :2]))
    clipped = [p.difference(old) for p in domains]
    masks = [shapely.from_wkt(w) for w in saved["masks_wkt"]]
    if (
        len(masks) != len(saved["nodes"])
        or len(masks) > 64
        or any(p.is_empty or not p.is_valid for p in masks)
    ):
        raise ValueError("bounded valid source-node masks required")
    return rows, (saved["nodes"], domains, clipped, masks, old)


def display_proof(args, report, state, data, body, parts, roles):
    shown = read_arrays(args.output / "after-display.npz")
    original = state["road_display"]
    for name in ("before", "after"):
        if (
            digest(args.output / (name + "-display.npz"))
            != report["display"][name + "_display_sha256"]
        ):
            raise ValueError("saved module display hash changed")
    if digest(args.output / "before-display.npz") != digest(
        state["before_display_path"]
    ):
        raise ValueError("before image lost whole original world")
    if any(not np.array_equal(shown[k][: len(v)], v) for k, v in original.items()):
        raise ValueError("old road presentation arrays changed")
    palette = np.array(
        json.loads((args.supports / "after-overview-scene.json").read_text())["colours"]
    )
    components = [
        (body.vertices, body.triangles, 1),
        (data["body_vertices"], data["body_faces"], 3),
    ]
    components.extend((p.vertices, p.triangles, 6) for p in state["piers"])
    vo, fo = len(original["vertices"]), len(original["triangles"])
    for index, (vertices, faces, slot) in enumerate(components):
        xyz = vertices.astype("f4").astype(float)
        if not np.array_equal(
            shown["vertices"][vo : vo + len(xyz)], xyz
        ) or not np.array_equal(shown["triangles"][fo : fo + len(faces)] - vo, faces):
            raise ValueError("shown body is not the actual admitted export")
        p = xyz[faces]
        normals = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
        lengths = np.linalg.norm(normals, axis=1)
        slots = np.full(len(faces), slot, int)
        if index == 1:
            slots = body_slots(data)
        if index == 0:
            slots[abs(normals[:, 2]) < lengths * 1e-6] = 6
            slots[np.flatnonzero(~parts["ground"])[roles]] = 6
        expected = np.repeat(normals / lengths[:, None], 3, axis=0)
        if (
            not np.array_equal(
                shown["normals"][3 * fo : 3 * (fo + len(faces))], expected
            )
            or not np.array_equal(shown["materials"][fo : fo + len(faces)], slots)
            or not np.array_equal(
                shown["face_base_colour_linear_rgba"][fo : fo + len(faces)],
                palette[slots],
            )
        ):
            raise ValueError("shown normal/material/colour ownership changed")
        vo, fo = vo + len(xyz), fo + len(faces)
    if vo != len(shown["vertices"]) or fo != len(shown["triangles"]):
        raise ValueError("unexplained extra display geometry")
    return {"components": len(components), "whole_actual_export": True}


def retained(args, report, commit):
    recipe = json.loads(args.fixture.read_text())
    state = load_cached(args, recipe)
    if (
        report.get("valid_retained_baseline") is not True
        or report["fixture_sha256"] != digest(args.fixture)
        or report["base_report_sha256"] != state["baseline_report_sha256"]
        or report["support_report_sha256"] != state["support_report_sha256"]
        or report.get("cache_parent_reports", []) != state["cache_parent_reports"]
        or report["candidate_source_ids"] != state["report"]["candidate_source_ids"]
        or report["refused_sources"] != recipe["source_ids"]
        or report["admitted_source_ids"]
        or not report["reason"]
    ):
        raise ValueError("retained partial-world binding changed")
    if set(report["artifacts"]) != {"source-catalog.json"} or any(
        p.name not in {"comparison.json", "source-catalog.json", "verification.json"}
        for p in args.output.iterdir()
    ):
        raise ValueError("refused module retained unexplained geometry")
    path = args.output / "source-catalog.json"
    if digest(path) != report["artifacts"][path.name] or json.loads(
        path.read_text()
    ) != refused_catalog(
        args.base, recipe, report["reason"], catalog_path=state["catalog_path"]
    ):
        raise ValueError("refused original source inventory changed")
    receipt = {
        "generation_commit": report["commit"],
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "valid_local_module": False,
        "valid_retained_baseline": True,
        "refused_sources": recipe["source_ids"],
        "admitted_source_ids": [],
    }
    write_json(args.output / "verification.json", receipt)
    print(
        json.dumps({"reader_commit": commit, "valid_retained_baseline": True}),
        flush=True,
    )
    return receipt


def verify(args):
    commit = frozen()
    began = time.perf_counter()
    report = json.loads((args.output / "comparison.json").read_text())
    if report.get("valid_local_module") is False:
        return retained(args, report, commit)
    recipe, original, context, sources, sample, layer = inputs(args.fixture)
    state = load_cached(args, recipe)
    if (
        report.get("valid_local_module") is not True
        or report["fixture_sha256"] != digest(args.fixture)
        or report["base_report_sha256"] != state["baseline_report_sha256"]
        or report["support_report_sha256"] != state["support_report_sha256"]
        or report.get("cache_parent_reports", []) != state["cache_parent_reports"]
        or report["origin_lat_lon"] != context["origin_lat_lon"]
        or report["admitted_source_ids"]
    ):
        raise ValueError("module source/frame/cache/non-admission binding changed")
    for name, expected in report["artifacts"].items():
        if digest(args.output / name) != expected:
            raise ValueError("saved module artifact changed: " + name)
    _old, line, ground, upper = old_surfaces(state, recipe, sources)
    rows, planning = saved_planning(args.output, original, ground)
    source_contract = {}
    if recipe.get("contact_mode") == "profile":
        source_contract = verify_source_contract(
            rows, sources, recipe, state["module_contacts"], line, planning[0]
        )
    data = read_arrays(args.output / "module.npz")
    if not np.array_equal(data["targets"], sample(data["surface_vertices"][:, :2])):
        raise ValueError("original module DEM targets changed")
    network = audit(state, recipe, data, rows, planning, line, upper)
    local = read_arrays(args.output / "local-layout.npz")
    local_field = read_arrays(args.output / "local-field.npz")
    plan = read_arrays(args.output / "world-layout.npz")
    field = read_arrays(args.output / "world-field.npz")
    arrays = read_arrays(args.output / "world-body.npz")
    body = Mesh(arrays.pop("vertices"), arrays.pop("faces"))
    chosen = read_arrays(args.output / "selection.npz")["old_selected_faces"]
    actual_ground = source_ground(ground, data)
    footprint = shapely.union_all(
        shapely.polygons(actual_ground.vertices[actual_ground.triangles, :2])
    )
    newfoot = shapely.union_all(
        shapely.polygons(data["surface_vertices"][data["surface_faces"], :2])
    )
    expected_chosen, _protected, _shapes, _affected = selection(state, newfoot)
    if not np.array_equal(chosen, expected_chosen):
        raise ValueError("bounded terrain replacement scope changed")
    linked = field_link(
        state, local, local_field["heights"], chosen, plan, field["heights"]
    )
    for layout in (local, plan):
        if not np.array_equal(layout["targets"], sample(layout["xy"])):
            raise ValueError("original whole terrain DEM targets changed")
    roles, world = inspect_world(
        state,
        local,
        local_field["heights"],
        chosen,
        plan,
        field["heights"],
        body,
        arrays,
        actual_ground,
        upper,
        footprint,
        expected_local_roles=local_field["constructed"],
    )
    if not np.array_equal(roles, field["constructed"]):
        raise ValueError("saved constructed-soil classes changed")
    inventory = catalog(args.base, state, rows, sources, layer, planning[1], context)
    if inventory != json.loads((args.output / "source-catalog.json").read_text()):
        raise ValueError("whole source-usage inventory changed")
    expected_ids = state["report"]["candidate_source_ids"] + [
        r["source_id"] for r in rows
    ]
    if expected_ids != report["candidate_source_ids"]:
        raise ValueError("whole old and new candidate membership changed")
    shown = display_proof(args, report, state, data, body, arrays, roles)
    receipt = {
        "generation_commit": report["commit"],
        "reader_commit": commit,
        "products_report_sha256": digest(args.output / "comparison.json"),
        "module": network,
        **({"source_contract": source_contract} if source_contract else {}),
        "world": world,
        "field_link": linked,
        "display": shown,
        "valid_local_module": True,
        "admitted_source_ids": [],
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    }
    write_json(args.output / "verification.json", receipt)
    print(
        json.dumps(
            {k: receipt[k] for k in ("reader_commit", "valid_local_module", "seconds")}
        ),
        flush=True,
    )
    return receipt


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())
