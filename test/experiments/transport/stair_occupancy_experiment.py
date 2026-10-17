"""Actual-body affine spatial handoff prototype; private explicit /tmp outputs only."""

import argparse
import json
import resource
import time
from pathlib import Path

import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import SourceFeature
from outshine_ref.pipeline import adjusted_footprints
from stair_local_replay import load_chain
from stair_occupancy_handoff import OccupancyQuery, adjusted
from stair_occupancy_inputs import (
    actual_sources,
    buildings,
    full_tile_sampler,
    reservations,
)
from stair_occupancy_pipeline import handoff, probes
from stair_occupancy_reuse import reused
from stair_shared_headroom import bindings
from stair_shared_headroom_volumes import supplement
from stair_terrain_experiment import frozen


def compare(query, footprints):
    runs = {}
    reference = None
    for method in ("scan", "index"):
        changes, notes = adjusted(query, footprints, method=method)
        actual = adjusted_footprints(footprints, changes)
        if any(n.get("deferred") for n in notes):
            raise ValueError("actual handoff has deferred constraints")
        states = [(p.key, p.geometry.wkb, p.heights, p.attributes) for p in actual]
        if reference is not None and states != reference:
            raise ValueError("full scan and spatial query disagree")
        reference = states
        runs[method] = {
            "notes": notes,
            "total_seconds": sum(n["seconds"] for n in notes),
            "query_rows": sum(n["query_rows"] for n in notes),
            "xy_candidates": sum(n["xy_candidates"] for n in notes),
            "height_candidates": sum(n["height_candidates"] for n in notes),
        }
    return changes, actual, runs


def save_volumes(output, rows, owners):
    records = [
        {
            "owner_id": r.owner_id,
            "purpose": r.purpose,
            "source_ids": keys,
            "geometry_wkb": r.geometry.wkb_hex,
            "origin_xy_m": r.origin_xy_m,
            "lower_plane_m": r.lower_plane_m,
            "upper_plane_m": r.upper_plane_m,
        }
        for r, keys in zip(rows, owners, strict=True)
    ]
    path = output / "volumes.json"
    path.write_text(json.dumps(records, separators=(",", ":")) + "\n")
    return {"file": path.name, "sha256": digest(path), "bytes": path.stat().st_size}


def probe_results(query, probe_inputs, seen):
    probes_result = []
    for p, blocked in zip(probe_inputs, seen["probe_blocked"], strict=True):
        cut, _note = query.conflict(p)
        envelope, _note = query.conflict(p, envelope=True)
        xy = query.tree.query(p.geometry, predicate="intersects")
        probes_result.append(
            {
                "id": p.source_id,
                "geometry_wkb": p.geometry.wkb_hex,
                "xy": list(p.geometry.centroid.coords[0]),
                "heights_m": [p.heights.lower_m, p.heights.upper_m],
                "transport_blocked": not cut.is_empty,
                "building_or_transport_blocked": blocked,
                "constant_facet_envelope_blocked": not envelope.is_empty,
                "projected_shadow_blocked": bool(len(xy)),
            }
        )
    return probes_result


def prepare_volumes(assets, supports, contacts, sources, manifest, modules, prior=None):
    if prior is None:
        rows, owners, error = reservations(
            assets, supports, contacts, sources, manifest
        )
        preparation = {
            "method": "fresh",
            "reused_physical_volumes": 0,
            "fitted_physical_volumes": len(rows),
        }
    else:
        rows, owners, error, preparation = reused(prior, assets, sources, manifest)
    extra, origins, records, work = supplement(
        rows,
        owners,
        bindings(
            assets,
            () if "borrowed_headroom_contacts" in manifest else modules,
            prepared_contacts=manifest.get("borrowed_headroom_contacts", ()),
        ),
        sources,
    )
    return (
        (*rows, *extra),
        (*owners, *origins),
        error,
        {"records": records, "work": work, "preparation": preparation},
    )


def run(args):
    commit = frozen()
    began = time.perf_counter()
    if not args.output.resolve().is_relative_to(Path("/tmp").resolve()):
        raise ValueError("private occupancy outputs must live under /tmp")
    args.output.mkdir(parents=True, exist_ok=False)
    view, assets, supports, contacts, manifest = load_chain(
        args.resident, args.prior_module
    )
    context, sources, _old_sample, _layer = actual_sources()
    rows, owners, error, headroom = prepare_volumes(
        assets,
        supports,
        contacts,
        sources,
        manifest,
        args.prior_module,
        args.prior_occupancy,
    )
    sample = full_tile_sampler(args.dem, context)
    originals, ledger, receipt = buildings(context, sample)
    probe_inputs, probe_omissions = probes(assets, sample)
    query = OccupancyQuery(rows)
    changes, actual, building_runs = compare(query, originals)
    _probe_changes, _actual_probes, probe_runs = compare(query, probe_inputs)
    original_transport = tuple(
        SourceFeature(k, sources[k]["shape"], sources[k]["properties"])
        for k in manifest["source_ids"]
    )
    world, seen = handoff(
        view.patches[0].frame,
        originals,
        rows,
        changes,
        probe_inputs,
        transport_sources=original_transport,
    )
    if [(p.key, p.geometry.wkb) for p in world.footprints] != [
        (p.key, p.geometry.wkb) for p in actual
    ]:
        raise ValueError("actual shared plan handoff differs from standalone changes")
    probes_result = probe_results(query, probe_inputs, seen)
    (args.output / "building-sources.json").write_text(
        json.dumps(ledger, indent=2) + "\n"
    )
    (args.output / "probes.json").write_text(json.dumps(probes_result, indent=2) + "\n")
    footprints = [
        {
            "source_id": p.source_id,
            "part": p.part,
            "geometry_wkb": p.geometry.wkb_hex,
            "heights_m": [p.heights.lower_m, p.heights.upper_m],
        }
        for p in actual
    ]
    (args.output / "adjusted-footprints.json").write_text(
        json.dumps(footprints, indent=2) + "\n"
    )
    report = {
        "commit": commit,
        "frame": manifest["frame"],
        "resident": str(args.resident.resolve()),
        "root_resident_sha256": manifest["root_resident_sha256"],
        "prior_modules": [str(p.resolve()) for p in args.prior_module],
        "parent_reports": manifest["parent_reports"],
        "source_ids": manifest["source_ids"],
        "vector": receipt,
        "dem": {"path": str(args.dem.resolve()), "sha256": digest(args.dem)},
        "volumes": save_volumes(args.output, rows, owners),
        "borrowed_headroom": headroom,
        "support_ownership_report": {
            "path": str(Path(manifest["support_mesh"]).parent / "comparison.json"),
            "sha256": digest(Path(manifest["support_mesh"]).parent / "comparison.json"),
        },
        "actual_road_faces": sum(len(a.data["top_faces"]) for a in assets),
        "actual_supports": len(supports),
        "actual_support_cap_faces": sum(len(lo.triangles) for lo, _ in contacts),
        "cap_outward_padding_bound_m": error,
        "building_source_count": len(ledger),
        "building_used_count": len(originals),
        "building_adjusted_count": len(changes),
        "removed_building_area_m2": sum(p.geometry.area for p in originals)
        - sum(p.geometry.area for p in actual),
        "building_queries": building_runs,
        "probe_queries": probe_runs,
        "probe_omissions": probe_omissions,
        "probe_free_below_transport": sum(
            not p["transport_blocked"] for p in probes_result
        ),
        "probe_projected_shadow_blocks": sum(
            p["projected_shadow_blocked"] for p in probes_result
        ),
        "probe_constant_envelope_blocks": sum(
            p["constant_facet_envelope_blocked"] for p in probes_result
        ),
        "probe_total": len(probes_result),
        "shapely": shapely.__version__,
        "geos": shapely.geos_version_string,
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "admitted_source_ids": [],
        "scope": "Experimental subtype only; shared API unchanged. Diagnostic consumer callbacks, not actual building/vegetation recipes. Rational query rectangle; final GEOS XY clipping binary64. No native/runtime admission.",
    }
    for name in ("building-sources.json", "probes.json", "adjusted-footprints.json"):
        report.setdefault("artifacts", {})[name] = digest(args.output / name)
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in report.items()
                if k not in ("building_queries", "probe_queries", "source_ids")
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--resident", type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    parser.add_argument("--dem", type=Path, required=True)
    parser.add_argument("--prior-occupancy", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_CPU, (180, 180))
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3,) * 2)
    run(parser.parse_args())
