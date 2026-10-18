"""Independently inspect saved local geometry, source contracts and whole contacts."""

import json
import resource
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from stair_local_context import selected_keys, state_for
from stair_local_generate import arguments
from stair_local_physics import piers, validate
from stair_local_replay import load_chain, products
from stair_module_catalog import catalog
from stair_module_experiment import refused_catalog, write_json
from stair_module_family import (
    audit,
)
from stair_module_family import (
    surfaces as old_surfaces,
)
from stair_module_family import (
    verify_sources as verify_source_contract,
)
from stair_module_inputs import inputs
from stair_module_soil import sampled_pins, selection
from stair_module_verify import saved_planning
from stair_pier_ground import foot_windows
from stair_terrain_experiment import frozen


def whole_foot_pins(state, xy, chosen, region, old_xyz, values):
    windows = shapely.union_all(foot_windows(state, region))
    selected = shapely.dwithin(windows, shapely.points(xy), 1e-7)
    original = Mesh(old_xyz, state["plan"]["faces"][chosen])
    expected = sampled_pins(xy[selected], [original])
    if not np.isfinite(expected).all():
        raise ValueError("original whole-foot terrain references unresolved")
    both = np.isfinite(values[selected])
    if np.max(abs(values[selected][both] - expected[both]), initial=0) > 1e-7:
        raise ValueError("whole-foot and road reference heights conflict")
    values[selected] = expected


def local_contract(state, saved, data, sample, recipe):
    local, field, chosen = saved["local"], saved["field"], saved["chosen"]
    old = state["plan"]
    new_bed = data["surface_vertices"] - [0, 0, 0.3]
    new_foot = shapely.union_all(shapely.polygons(new_bed[data["surface_faces"], :2]))
    expected, protected, shapes, _affected = selection(state, new_foot)
    if not np.array_equal(chosen, expected) or chosen.dtype.kind != "b":
        raise ValueError("saved complete local selection changed")
    old_xyz = np.c_[old["xy"], state["heights"]]
    policy = recipe.get("pier_ground_policy", "whole_parent")
    if policy not in ("whole_parent", "whole_foot"):
        raise ValueError("explicit known saved pier ground policy required")
    old_ids = np.flatnonzero(chosen & old["ground"])
    if policy == "whole_parent":
        old_ids = np.r_[old_ids, protected]
    values = sampled_pins(
        local["xy"],
        [Mesh(old_xyz, old["faces"][old_ids]), Mesh(new_bed, data["surface_faces"])],
    )
    region = shapely.union_all(shapes[chosen])
    if policy == "whole_foot":
        whole_foot_pins(state, local["xy"], chosen, region, old_xyz, values)
    boundary = shapely.dwithin(region.boundary, shapely.points(local["xy"]), 1e-7)
    lookup = {tuple(p): h for p, h in zip(old["xy"], state["heights"], strict=True)}
    values[boundary] = [lookup[tuple(p)] for p in local["xy"][boundary]]
    if (
        not np.array_equal(boundary, local["boundary"])
        or not (
            np.array_equal(values, local["bed"], equal_nan=True)
            if policy == "whole_parent"
            else np.allclose(values, local["bed"], rtol=0, atol=1e-7, equal_nan=True)
        )
        or not np.array_equal(local["targets"], sample(local["xy"]))
        or not np.array_equal(data["targets"], sample(data["surface_vertices"][:, :2]))
    ):
        raise ValueError("saved local raw targets or actual immutable pins changed")
    if not np.array_equal(field["heights"][boundary], values[boundary]):
        raise ValueError("saved local exact outer boundary heights changed")


def verify(args):
    commit, began = frozen(), time.perf_counter()
    updated, _all_assets, _supports, contacts, manifest, report, saved = products(
        args.resident, args.output, args.prior_module
    )
    if report["fixture_sha256"] != digest(args.fixture):
        raise ValueError("original local source recipe changed")
    recipe, original, context, sources, sample, layer = inputs(args.fixture)
    if context["origin_lat_lon"] != manifest["origin_lat_lon"]:
        raise ValueError("original local source frame changed")
    if saved is None:
        expected = refused_catalog(
            Path(manifest["catalog"]).parent,
            recipe,
            report["reason"],
            catalog_path=Path(manifest["catalog"]),
        )
        if expected != json.loads((args.output / "source-catalog.json").read_text()):
            raise ValueError("retained complete source catalogue changed")
        receipt = {
            "reader_commit": commit,
            "products_report_sha256": digest(args.output / "comparison.json"),
            "valid_local_module": False,
            "valid_retained_baseline": True,
            "admitted_source_ids": [],
        }
    else:
        if saved["field"]["heights"].min() < manifest["height_floor_m"]:
            raise ValueError("saved local terrain crosses its frozen cached datum")
        old_view, assets, supports, _old_contacts, _manifest = load_chain(
            args.resident, args.prior_module
        )
        state, selected, _axes, note = state_for(
            old_view, assets, supports, contacts, sources, recipe
        )
        _old, line, ground, upper = old_surfaces(state, recipe, sources)
        rows, planning = saved_planning(args.output, original, ground)
        source = verify_source_contract(
            rows,
            sources,
            recipe,
            state["module_contacts"],
            line,
            planning[0],
            cached_grids=state.get("cached_box_grids"),
        )
        if saved["keys"]["context"] != [list(k) for k in selected.face_keys]:
            raise ValueError(
                "saved context differs from complete resident neighbourhood"
            )
        geometry = audit(
            state, recipe, saved["data"], rows, planning, line, upper, sources=sources
        )
        local_contract(state, saved, saved["data"], sample, recipe)
        footprint = shapely.union_all(
            shapely.polygons(
                saved["data"]["surface_vertices"][saved["data"]["surface_faces"], :2]
            )
        )
        if (
            selected_keys(old_view, selected, saved["chosen"], footprint, contacts)
            != saved["keys"]["removals"]
        ):
            raise ValueError("saved stable removal witness changed")
        physical = validate(
            updated,
            state,
            saved["local"],
            saved["field"]["heights"],
            saved["field"]["constructed"],
            saved["chosen"],
            saved["data"],
            upper,
        )
        inventory = catalog(
            Path(manifest["catalog"]).parent,
            {
                "catalog_path": Path(manifest["catalog"]),
                "report": {"after": {"source_ids": manifest["source_ids"]}},
            },
            rows,
            sources,
            layer,
            planning[1],
            context,
        )
        if inventory != json.loads((args.output / "source-catalog.json").read_text()):
            raise ValueError("original full source catalogue or disposition changed")
        receipt = {
            "reader_commit": commit,
            "products_report_sha256": digest(args.output / "comparison.json"),
            "source_contract": source,
            "context": note,
            "module": geometry,
            "physical": physical,
            "piers": piers(updated, contacts),
            "valid_local_module": True,
            "admitted_source_ids": [],
        }
    receipt.update(
        seconds=time.perf_counter() - began,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
    )
    write_json(args.output / "verification.json", receipt)
    print(
        json.dumps(
            {
                k: receipt[k]
                for k in (
                    "reader_commit",
                    "valid_local_module",
                    "seconds",
                    "peak_rss_bytes",
                )
            }
        ),
        flush=True,
    )
    return receipt


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    verify(arguments())
