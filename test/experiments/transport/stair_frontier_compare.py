"""Compare source-node discovery on cached real inputs, without 3D admission."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
import shapely
from capture_stair_context import parts
from outshine_ref.places import acquire, covering_tiles, decode_vector
from outshine_ref.transport.frontiers import discover_source_modules
from stair_frontier_sources import reason
from stair_module_experiment import write_json
from stair_module_inputs import reference_profiles
from stair_terrain_experiment import frozen


def case_records(sources):
    points = np.vstack([s["xy_m"] for s in sources.values()])
    domain = shapely.box(*(points.min(axis=0) - 64), *(points.max(axis=0) + 64))
    profiles = reference_profiles()
    records = []
    for key in sorted(sources):
        source = sources[key]
        why = reason(dict(source, source_id=key), set(), domain, profiles)
        records.append(
            {
                "source_id": key,
                "nodes": source["mvt_xy"],
                "length_m": float(source["shape"].length),
                "eligible": not bool(why),
                "reason": why,
            }
        )
    return records


def compare(records):
    eligible = sorted(
        (r for r in records if r["eligible"]),
        key=lambda r: (-r["length_m"], r["source_id"]),
    )
    roots = [
        {"root_id": r["source_id"] + "@0", "node": r["nodes"][0]} for r in eligible[:4]
    ]
    results = {}
    for method in ("index", "scan"):
        begin = time.perf_counter()
        results[method] = discover_source_modules(records, roots, method=method)
        results[method]["seconds"] = time.perf_counter() - begin
    for left, right in zip(
        results["index"]["proposals"], results["scan"]["proposals"], strict=True
    ):
        for key in ("root_id", "source_ids", "connections", "length_m", "stop_reason"):
            if left[key] != right[key]:
                raise ValueError(
                    "indexed and full-scan source selection differ: " + key
                )
    return roots, results


def run(args):
    commit = frozen()
    if (
        args.output.exists()
        or not args.output.name.startswith("outshine-")
        or not args.output.resolve().is_relative_to(tempfile.gettempdir())
    ):
        raise ValueError("fresh explicit outshine-* system-temp output required")
    gallery = json.loads(args.gallery.read_text())
    proof = []
    for case in gallery["cases"]:
        if case["id"] == "Wien":
            continue
        lat, lon = case["origin_lat_lon"]
        tile = covering_tiles(lat, lon, 0, gallery["zoom"])[0]
        url = gallery["provider_prefix"] + "/" + "/".join(map(str, tile)) + ".pbf"
        receipt = acquire(("vector", tile, url), args.sources, fetch=False)
        layer = decode_vector(receipt)["transportation"]
        sources = parts(layer, tile, np.array([lat, lon]))
        records = case_records(sources)
        roots, results = compare(records)
        proof.append(
            {
                "place": case["id"],
                "origin_lat_lon": [lat, lon],
                "receipt": receipt,
                "sources": len(records),
                "roots": roots,
                "results": results,
                "scope": "Original cached provider inputs; explicit-node selection and cost only. Roots are topology controls, not verified cached profiles. No terrain, physical or 3D world admission.",
            }
        )
        print(case["id"], {m: r["seconds"] for m, r in results.items()}, flush=True)
    args.output.mkdir(parents=True)
    write_json(args.output / "comparison.json", {"commit": commit, "cases": proof})


def argument_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--gallery",
        type=Path,
        default=Path(__file__).parent / "reference" / "transport-gallery.json",
    )
    parser.add_argument("--sources", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    run(argument_parser().parse_args())
