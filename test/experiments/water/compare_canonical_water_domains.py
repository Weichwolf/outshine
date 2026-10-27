"""Replay admitted WATER3 snapshots; never promote their coverage to raw completeness."""

import argparse
import dataclasses
import hashlib
import json
import resource
import time
import tracemalloc
from collections import Counter, defaultdict
from pathlib import Path

import numpy as np
import shapely
from canonical_water_domains import Piece, Snapshot, resolve
from native_river_inputs import surface_rings
from native_water_observation import NO_INPUT, read_v3
from water_body_domains import assemble
from water_domain_oracle import all_pairs, check_complete

DEFAULT = Path(__file__).parent / "fixtures/feldkirch-ill-native-inputs.json"


def observed_snapshots(path):
    manifest = json.loads(path.read_text())["observed_v3"]
    data = read_v3(path.parent / manifest["file"], manifest["sha256"])
    rows = defaultdict(list)
    for index, surface in enumerate(data["surfaces"]):
        if int(surface["input"]) == NO_INPUT:
            raise ValueError("surface without original provenance")
        info = data["inputs"][int(surface["input"])]
        source = data["sources"][info["source"]]
        zoom, _x, _y = source["tile"]
        rings = []
        for lat_lon in surface_rings(data, index):
            latitude = np.deg2rad(lat_lon[:, 0])
            rings.append(
                np.c_[
                    (lat_lon[:, 1] + 180) / 360 * (1 << zoom),
                    (1 - np.arcsinh(np.tan(latitude)) / np.pi) / 2 * (1 << zoom),
                ]
            )
        polygon = shapely.normalize(shapely.Polygon(rings[0], rings[1:]))
        rows[tuple(source["tile"])].append(
            (info["feature"], polygon.wkb, polygon, info, index)
        )
    snapshots, indices = {}, {}
    for source in data["sources"]:
        tile = tuple(source["tile"])
        parts = Counter()
        pieces = []
        digest = hashlib.sha256(("admitted-WATER3-v1:" + source["digest"]).encode())
        for feature, wkb, polygon, info, index in sorted(
            rows[tile], key=lambda r: (r[0], r[1])
        ):
            part = parts[feature]
            parts[feature] += 1
            pieces.append(Piece(feature, part, polygon, info))
            indices[tile + (feature, part)] = index
            digest.update(json.dumps(info, sort_keys=True).encode() + wkb)
            digest.update(data["surfaces"][index]["level_m"].tobytes())
        # False by default: the native processed-tile flag does not certify raw sources.
        snapshots[tile] = Snapshot(
            tile, source, digest.hexdigest(), False, tuple(pieces)
        )
    return data, snapshots, indices


def compare(path):
    data, snapshots, indices = observed_snapshots(path)
    epoch = (data["sources"][0]["provider"], data["sources"][0]["revision"])
    options = {
        "epoch": epoch,
        "support": 1 / 64,
        "model_revision": "admitted-fixture-topology-v1",
        "assume_ground": True,
    }
    # Conditional experiment only: complete w.r.t. this finite normalized asset table.
    conditional = {
        t: dataclasses.replace(s, complete=True) for t, s in snapshots.items()
    }
    keys = sorted(indices)
    source_records = {
        t + (p.feature, p.part): (p, s.source)
        for t, s in conditional.items()
        for p in s.pieces
    }
    polygons = [source_records[k][0].polygon for k in keys]
    inputs = [source_records[k][0].info for k in keys]
    sources = [source_records[k][1] for k in keys]
    baseline, rejected, baseline_work = assemble(
        polygons, inputs, sources, assume_ground=True, broad_phase="tree"
    )
    oracle, oracle_work = all_pairs(polygons, inputs, sources, assume_ground=True)
    check_complete(polygons, baseline, oracle)
    if rejected:
        raise ValueError("complete fixture baseline rejected sources")
    partition = {
        keys[i]: tuple(keys[j] for j in body.sources)
        for body in baseline
        for i in body.sources
    }
    results, states, reasons = [], Counter(), Counter()
    wall, cpu = time.perf_counter(), time.process_time()
    tracemalloc.start()
    for key in keys:
        result = resolve(key, conditional, **options)
        states[result.state] += 1
        reasons[result.reason] += 1
        if result.state == "closed":
            if result.sources != partition[key]:
                raise ValueError("closure differs from exhaustive fixture partition")
            reordered = resolve(
                key, dict(reversed(list(conditional.items()))), **options
            )
            if (reordered.version, reordered.geometry.wkb) != (
                result.version,
                result.geometry.wkb,
            ):
                raise ValueError(
                    "source window insertion order changed canonical product"
                )
        results.append((key, result))
    traced_peak = tracemalloc.get_traced_memory()[1]
    allocations = sum(
        s.count for s in tracemalloc.take_snapshot().statistics("filename")
    )
    tracemalloc.stop()
    cost = {
        "wall_s": time.perf_counter() - wall,
        "cpu_s": time.process_time() - cpu,
        "python_traced_peak_bytes": traced_peak,
        "retained_traced_blocks": allocations,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "all seeds plus closed-seed reversed-map replay; dependency/import/decoded input memory excluded from traced peak",
    }
    versions = defaultdict(set)
    for _key, result in results:
        if result.owner is not None:
            versions[result.owner].add(result.version)
    if any(len(v) != 1 for v in versions.values()):
        raise ValueError("different starts in one closed domain changed version")
    raw = resolve(keys[0], snapshots, **options)
    if raw.state != "pending_sources" or raw.geometry is not None:
        raise ValueError("admitted native flag falsely certified raw completeness")
    report = {
        "scope": "Conditional topology of admitted normalized WATER3 pieces, not full raw-source/hydraulic closure or height fit; decoded snapshots already resident, no DB/IO benchmark",
        "input_sha256": json.loads(path.read_text())["observed_v3"]["sha256"],
        "original_source_surfaces": len(data["surfaces"]),
        "source_tiles": len(snapshots),
        "declared_support_tile_units": options["support"],
        "observed_no_support_violations": True,
        "raw_source_completeness": raw.state,
        "baseline_bodies": len(baseline),
        "baseline_work": baseline_work,
        "independent_all_pairs_oracle_work": oracle_work,
        "pending_baseline_domains": len(
            {partition[k] for k, r in results if r.state == "pending_sources"}
        ),
        "states_by_seed": dict(states),
        "reasons_by_seed": dict(reasons),
        "closed_domains": len(versions),
        "max_tile_reads": max(r.work["tile_reads"] for _, r in results),
        "tile_reads_p50_p95_p99": np.percentile(
            [r.work["tile_reads"] for _, r in results], [50, 95, 99]
        ).tolist(),
        "max_snapshot_lookups": max(r.work["snapshot_lookups"] for _, r in results),
        "max_source_records": max(r.work["source_records"] for _, r in results),
        "max_coordinates": max(r.work["coordinates"] for _, r in results),
        "total_relation_tests": sum(r.work["relation_tests"] for _, r in results),
        "geometry_wkb_bytes": sum(r.work["geometry_wkb_bytes"] for _, r in results),
        "snapshot_payload_bytes": None,
        "copied_bytes": None,
        "cost": cost,
        "rows": [
            {
                "seed": k,
                "native_surface": indices[k],
                "state": r.state,
                "reason": r.reason,
                "owner": r.owner,
                "version": r.version,
                "required": r.required,
                "sources": r.sources,
                "work": r.work,
            }
            for k, r in results
        ],
    }
    return report, results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report, _results = compare(args.fixture)
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "rows"}, indent=2))


if __name__ == "__main__":
    main()
