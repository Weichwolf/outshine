"""Paired complete source window with admitted new mass families, flat without DEM."""

import argparse
import hashlib
import json
import resource
import time
from collections import Counter
from dataclasses import replace
from pathlib import Path

from city_scene import inputs
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from showcase_tiles import frame_proof, view_for
from terraced_city_parts import PreviousParts, fingerprint
from terraced_city_reference import admitted, unchanged_modelling
from tile_buildings import construct
from tile_dormers import DormerFacades
from tile_podiums import PodiumDetails
from tile_terraced import TerracedDetails, definitions

DATA = Path(__file__).with_name("data")


def emit(
    output, name, builder, previous, rows, window, view, receipts, proof, metadata
):
    start = time.perf_counter()
    mesh, source = construct(
        "Paris",
        prepared=(rows, {"invalid": [], "span_m": window["width_m"]}),
        ordinary_builder=builder,
        radial_builder=previous.radial,
    )
    if source["generated"] != 988 or source["omissions"]:
        raise ValueError("Paired whole-city output lost original source coverage")
    extra = definitions() if isinstance(builder, TerracedDetails) else {}
    if set(extra) & (
        set(source["material_definitions"]) | set(previous.owner.facades.bindings)
    ):
        raise ValueError(
            "Terraced component bindings collide with previous source materials"
        )
    source["material_definitions"].update(extra)
    checks = frame_proof(mesh, view)
    record = {
        "camera": camera(view),
        "source": source,
        "sources": receipts,
        "window": window,
        "facade_materials": previous.owner.facades.bindings,
        "ground_span_m": 10000,
        "frame": checks,
        "procedures": metadata,
        "mesh_array_sha256": fingerprint(mesh),
        "triangles": len(mesh["faces"]),
        "assembly_seconds": time.perf_counter() - start,
        "scope": "Complete unchanged988-source flat window and actual exterior-position demand. Previous same-view parts reused; old functional owners preserved. Earlier scoped physical source proof reused with unchanged modelling files, not a new full-cohort audit/cache/controller/native/light/shadow/device admission.",
    }
    write(output, name, mesh, record, proof)
    print(
        json.dumps(
            {
                "pass": name,
                "sources": source["generated"],
                "triangles": len(mesh["faces"]),
                "assembly_seconds": record["assembly_seconds"],
            }
        ),
        flush=True,
    )
    return record


def models(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the whole-city producer before image evidence")
    reference = json.loads(args.audit.read_text())
    if len(reference["original_census"]) != 988 or len(reference["records"]) != 71:
        raise ValueError(
            "This comparison requires the entire original988-source/71-candidate study"
        )
    unchanged = unchanged_modelling(reference)
    baseline = json.loads((DATA / "terraced-city-baseline.json").read_text())
    rows, receipts, window = inputs(
        "Paris", json.loads((DATA / "roof-crown-city-window.json").read_text())
    )
    records = admitted(rows, reference, baseline)
    if sorted((r["url"], r["sha256"]) for r in receipts["receipts"]) != sorted(
        (r["url"], r["sha256"]) for r in baseline["receipts"]
    ):
        raise ValueError("Whole-city source receipts changed")
    view = replace(view_for(rows), width=1920, height=1080)

    def progress(requested, accepted, identifier):
        print(
            json.dumps(
                {
                    "previous_requested": requested,
                    "previous_assets": accepted,
                    "id": identifier,
                }
            ),
            flush=True,
        )

    began = time.perf_counter()
    owner = PodiumDetails(rows, view, progress=progress)
    owner.facades = DormerFacades(owner.rows, view)
    previous = PreviousParts(owner)
    context = {
        "physical_reference": str(args.audit.resolve()),
        "physical_reference_sha256": hashlib.sha256(
            args.audit.read_bytes()
        ).hexdigest(),
        "physical_commit": reference["commit"],
        "unchanged_modelling_files": unchanged,
    }
    before = emit(
        args.output,
        "TerracedBefore",
        previous,
        previous,
        owner.rows,
        window,
        view,
        receipts,
        proof,
        context,
    )
    existing = {r["id"] for r in owner.completed}
    if not set(baseline["existing_function_ids"]).issubset(existing):
        raise ValueError("Previous source pipeline lost a protected functional owner")
    records = {key: value for key, value in records.items() if key not in existing}
    builder = TerracedDetails(previous, records)
    outcomes = []
    for full in [False, True] if args.full_near_reference else [False]:
        builder.full_near = full
        builder.completed.clear()
        tag = "TerracedFullNear" if full else "TerracedAfter"
        record = emit(
            args.output,
            tag,
            builder,
            previous,
            owner.rows,
            window,
            view,
            receipts,
            proof,
            context
            | {
                "new_sources": builder.completed,
                "protected_existing_sources": sorted(existing),
                "reference_only_full_near": full,
            },
        )
        if {r["id"] for r in builder.completed} != set(records):
            raise ValueError(
                "A proposed ordinary replacement bypassed its declared source route"
            )
        outcomes.append(
            {
                "pass": tag,
                "modes": dict(Counter(r["mode"] for r in builder.completed)),
                "new_sources": len(builder.completed),
                "triangles": sum(r["triangles"] for r in builder.completed),
                "assembly_seconds": record["assembly_seconds"],
            }
        )
    for key, (mesh, _info) in previous.parts.items():
        if fingerprint(mesh) != previous.hashes[key]:
            raise ValueError("Whole-city replacement mutated a previous source part")
    result = context | {
        "commit": proof["commit"],
        "complete": True,
        "sources": 988,
        "protected_existing_sources": sorted(existing),
        "previous_parts": len(previous.parts),
        "previous_triangle_count": before["triangles"],
        "outcomes": outcomes,
        "worker_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "total_after_source_seconds": time.perf_counter() - began,
        "scope": "Offline paired comparison, previous render parts and physical experiments reused explicitly; no native/cache/frame or general LOD acceptance.",
    }
    (args.output / "whole-city.json").write_text(json.dumps(result, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--audit", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--full-near-reference", action="store_true")
    args = parser.parse_args()
    if args.output.resolve() == Path(
        "/tmp"
    ) or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("--output requires an explicit subdirectory of /tmp")
    args.output.mkdir(parents=True, exist_ok=True)
    models(args)


if __name__ == "__main__":
    main()
