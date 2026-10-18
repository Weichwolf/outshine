"""Compare actual candidate junction cells; positive XY checks never admit a world."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from shared_bridge_geometry import corridor
from stair_cycle_contacts import interior_profile_xy, profile
from stair_junction_layout import arrangement
from stair_landing_pads import node_profiles
from stair_local_context import state_for
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_geometry import old_surfaces, prepared
from stair_module_inputs import inputs
from stair_module_node_heights import plan_heights
from stair_module_sections import cap_edge_constraints
from stair_module_topology import check_surface
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen


def analytic_sections(row, *, extent):
    xy = np.asarray(row.get("surface_axis_xy_m", row["xy"]))
    selected = dict(row, xy=xy)
    lines = [profile(selected, 0)]
    lines.extend(interior_profile_xy(selected, i) for i in range(1, len(xy) - 1))
    lines.append(profile(selected, -1))
    return [shapely.LineString(p[1] + extent * (p[[0, -1]] - p[1])) for p in lines]


def candidate_rows(rows, nodes, plans, mode):
    result = [
        dict(row, junction_pads_xy_m=list(row.get("junction_pads_xy_m", [])))
        for row in rows
    ]
    if mode == "source_hull":
        return result
    for plan in plans:
        node = next(n for n in nodes if n["mvt_xy"] == plan["mvt_xy"])
        if mode == "level_box":
            shape = plan["shape"]
        else:
            profiles = [
                p for _i, _v, p in node_profiles(rows, node, include_interiors=True)
            ]
            shape = shapely.MultiPoint(np.vstack(profiles)).envelope
        for owner in node["incident"]:
            result[owner]["junction_pads_xy_m"].append(
                np.array(shape.exterior.coords[:-1])
            )
    return result


def inspect_variant(output, rows, old, caps, plans, original_union, sections):
    domains = [corridor(row).difference(old) for row in rows]
    constraints = cap_edge_constraints(caps, domains)
    constraints.extend(p["shape"].boundary for p in plans)
    provider = (
        None
        if sections == "legacy_extended"
        else lambda row: analytic_sections(
            row, extent=4 if sections == "analytic_extended" else 1
        )
    )
    xy, faces, owners, work = arrangement(
        rows, domains, [], constraints=constraints, section_provider=provider
    )
    try:
        check_surface(xy, faces)
        reason = ""
    except ValueError as error:
        reason = str(error)
    used = np.unique(faces)
    q = xy.astype("f4").astype(float)
    p = q[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    region = shapely.union_all([corridor(row) for row in rows])
    np.savez_compressed(output, xy=xy, faces=faces, owners=owners)
    return {
        "vertices": len(xy),
        "faces": len(faces),
        "used_vertices": len(used),
        "unique_float32_vertices": len(np.unique(q[used], axis=0)),
        "nonpositive_float32_faces": int(
            np.sum(a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0] <= 0)
        ),
        "xy_valid_double_float32": not reason,
        "reason": reason,
        "added_union_area_m2": float(region.difference(original_union).area),
        "removed_union_area_m2": float(original_union.difference(region).area),
        "union_hausdorff_m": float(region.hausdorff_distance(original_union)),
        "work": work,
        "arrays_sha256": digest(output),
        "accepted_product": False,
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("explicit /tmp comparison output required")
    recipe, rows, context, sources, _sample, _layer = inputs(args.fixture)
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    state, _query, _axes, _note = state_for(
        view, assets, supports, contacts, sources, recipe
    )
    root, line, ground, _upper = old_surfaces(state, recipe, sources)
    nodes, original, _domains, _masks, old = prepared(rows, sources, root, line, ground)
    caps = [root["cached_cap"]] + [c["cap"] for c in root["additional_contacts"]]
    points = [line[1, :2], *[c["xy"] for c in root["additional_contacts"]]]
    plans = plan_heights(
        rows, nodes, points, include_interiors=root["include_interiors"]
    )
    args.output.mkdir()
    variants = []
    for mode in ("source_hull", "profile_box", "level_box"):
        candidate = candidate_rows(rows, nodes, plans, mode)
        for sections in ("legacy_extended", "analytic_extended", "analytic_whole"):
            name = mode + "-" + sections
            result = inspect_variant(
                args.output / (name + ".npz"),
                candidate,
                old,
                caps,
                plans,
                shapely.union_all(original),
                sections,
            )
            variants.append(dict(pad_recipe=mode, section_recipe=sections, **result))
    report = {
        "commit": commit,
        "fixture_sha256": digest(args.fixture),
        "resident_sha256": digest(args.resident / "resident.json"),
        "parent_reports": manifest["parent_reports"],
        "vector_sha256": context["vector"]["sha256"],
        "origin_lat_lon": context["origin_lat_lon"],
        "before_source_ids": manifest["source_ids"],
        "proposed_source_ids": recipe["source_ids"],
        "new_level_nodes": [p["mvt_xy"] for p in plans],
        "admitted_source_ids": [],
        "variants": variants,
        "scope": "Candidate XY constructions only. Boxes are explicitly different procedural footprints; exact old caps stay fixed. No source-reuse/height/profile/world admission or performance comparison.",
    }
    write_json(args.output / "comparison.json", report)
    print(
        json.dumps(
            {
                "variants": len(variants),
                "xy_valid_candidates": sum(
                    v["xy_valid_double_float32"] for v in variants
                ),
                "admitted_sources": 0,
            }
        ),
        flush=True,
    )
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())
