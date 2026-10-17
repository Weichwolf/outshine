"""Inspect actual undeclared cap overlaps; never admit a contact or alter the world."""

import argparse
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_local_context import state_for
from stair_local_replay import load_chain
from stair_module_experiment import write_json
from stair_module_geometry import old_surfaces, prepared
from stair_module_inputs import inputs
from stair_patch_inputs import fresh
from stair_surface_cap import cap_properties
from stair_terrain_experiment import frozen


def undeclared(rows, planning, root, contacts):
    _nodes, original, domains, _masks, _old = planning
    result = []
    for i, row in enumerate(rows):
        caps = [root] + [
            c["cap"] for c in contacts if row["source_id"] in c["incident_source_ids"]
        ]
        declared = shapely.union_all(
            [
                shapely.union_all(shapely.polygons(c.vertices[c.triangles, :2]))
                for c in caps
            ]
        )
        result.append(original[i].difference(domains[i].union(declared)))
    return result


def inspect_pairs(rows, missing, chosen, sources, output, *, method="index"):
    if method not in ("index", "scan"):
        raise ValueError("known overlap query method required")
    records, queries, tested, copied, returned = [], 0, 0, 0, 0
    for row, shape in zip(rows, missing, strict=True):
        if shape.area <= 1e-7:
            continue
        for asset in chosen:
            queries += 1
            hits = (
                asset.tree.query(shape, predicate="intersects")
                if method == "index"
                else np.flatnonzero(shapely.intersects(asset.shapes, shape))
            )
            hits = np.sort(hits)
            returned += len(hits)
            tested += len(hits) if method == "index" else len(asset.shapes)
            if tested > 65536:
                raise ValueError("bounded overlap candidate work exhausted")
            for column, old_key in enumerate(asset.source_ids):
                indices = hits[asset.data["owners"][hits, column]]
                if not len(indices):
                    continue
                mask = shapely.intersection(
                    shape, shapely.union_all(asset.shapes[indices])
                )
                if mask.area <= 1e-7:
                    continue
                props, top, bottom = cap_properties(asset, indices, row["gradient"])
                name = "cap-" + str(len(records)) + ".npz"
                np.savez_compressed(
                    output / name,
                    top=top,
                    bottom=bottom,
                    asset_top_face_indices=indices,
                )
                copied += top.nbytes + bottom.nbytes + indices.nbytes
                new = sources[row["source_id"]]
                old = sources[old_key]
                records.append(
                    {
                        "source_id": row["source_id"],
                        "cached_source_id": old_key,
                        "asset_sha256": asset.identity,
                        "asset_owner_column": column,
                        "mask_wkt": mask.wkt,
                        "area_m2": float(mask.area),
                        "shared_original_nodes": sorted(
                            set(map(tuple, new["mvt_xy"]))
                            & set(map(tuple, old["mvt_xy"]))
                        ),
                        "new_profile_isotropic": new["properties"].get("subclass")
                        in ("footway", "cycleway"),
                        "new_profile_gradient_limit": row["gradient"],
                        "preconditions": props,
                        "cap_file": name,
                        "cap_sha256": digest(output / name),
                        "admitted_contact": False,
                    }
                )
    return records, {
        "asset_queries": queries,
        "returned_facets": returned,
        "full_scan_facets": tested if method == "scan" else 0,
        "index_internal_visits": "unmeasured" if method == "index" else None,
        "exported_array_bytes": copied,
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("explicit /tmp overlap output required")
    recipe, rows, context, sources, _sample, _layer = inputs(args.fixture)
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    state, _query, _axes, note = state_for(
        view, assets, supports, contacts, sources, recipe
    )
    root, line, ground, _upper = old_surfaces(state, recipe, sources)
    planning = prepared(rows, sources, root, line, ground)
    missing = undeclared(rows, planning, root["cached_cap"], state["module_contacts"])
    chosen = [a for a in assets if a.identity in note["roads"]["local_assets"]]
    args.output.mkdir()
    records, work = inspect_pairs(
        rows, missing, chosen, sources, args.output, method=args.method
    )
    report = {
        "commit": commit,
        "resident_sha256": digest(args.resident / "resident.json"),
        "parent_reports": manifest["parent_reports"],
        "fixture_sha256": digest(args.fixture),
        "vector_sha256": context["vector"]["sha256"],
        "origin_lat_lon": context["origin_lat_lon"],
        "before_source_ids": manifest["source_ids"],
        "proposed_source_ids": recipe["source_ids"],
        "admitted_source_ids": [],
        "method": args.method,
        "overlaps": records,
        "work": work,
        "scope": "Actual undeclared cap-mask candidates and local mesh preconditions only. No inferred/original graph contacts admitted; full height compatibility, wall/profile recovery, world physics and routing remain unverified.",
    }
    write_json(args.output / "comparison.json", report)
    print(json.dumps({"candidates": len(records), "work": work}), flush=True)
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (90, 90))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resident", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    run(parser.parse_args())
