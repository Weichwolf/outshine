"""Replay finite delivered WATER3 cell/layer ownership, not Original-OSM closure."""

import argparse
import dataclasses
import json
import resource
import time
import tracemalloc
from collections import Counter
from pathlib import Path

import matplotlib.pyplot as plt
import shapely
from canonical_water_domains import resolve
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from owned_water_cells import normalize_cell
from water_body_domains import assemble
from water_domain_oracle import all_pairs, check_complete


def compare(path, output):
    data, snapshots, _ = observed_snapshots(path)
    manifest = json.loads(path.read_text())["observed_v3"]
    scope = "delivered-WATER3-water-layer:" + manifest["sha256"]
    # EOF/checksum-validated finite native surface table is this experiment's input.
    # This declaration never asserts completeness of provider raw/Original-OSM data.
    wall, cpu = time.perf_counter(), time.process_time()
    tracemalloc.start()
    cells = {
        tile: normalize_cell(dataclasses.replace(s, complete=True), scope=scope)
        for tile, s in snapshots.items()
    }
    normalized = {tile: c.snapshot for tile, c in cells.items()}
    records = {
        tile + (p.feature, p.part): (p, c.snapshot.source)
        for tile, c in cells.items()
        for p in c.snapshot.pieces
    }
    keys = sorted(records)
    polygons = [records[k][0].polygon for k in keys]
    inputs = [records[k][0].info for k in keys]
    sources = [records[k][1] for k in keys]
    bodies, rejected, _ = assemble(polygons, inputs, sources, assume_ground=True)
    oracle, oracle_work = all_pairs(polygons, inputs, sources, assume_ground=True)
    check_complete(polygons, bodies, oracle)
    if rejected:
        raise ValueError("normalized finite fixture rejected sources")
    partition = {
        keys[i]: tuple(keys[j] for j in b.sources) for b in bodies for i in b.sources
    }
    epoch = (data["sources"][0]["provider"], data["sources"][0]["revision"])
    options = {
        "epoch": epoch,
        "support": 0,
        "model_revision": scope,
        "assume_ground": True,
    }
    states, reasons, reads = Counter(), Counter(), []
    for key in keys:
        result = resolve(key, normalized, **options)
        states[result.state] += 1
        reasons[result.reason] += 1
        reads.append(result.work["tile_reads"])
        if result.state == "closed":
            other = resolve(key, dict(reversed(list(normalized.items()))), **options)
            if result.sources != partition[key] or result.version != other.version:
                raise ValueError("closed normalization changed partition/order")
    traced_peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    cost = {
        "wall_s": time.perf_counter() - wall,
        "cpu_s": time.process_time() - cpu,
        "python_traced_peak_bytes": traced_peak,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "normalization + independent oracle + all seeds/reversed closed seeds; imports/decoded fixture excluded from traced peak; not runtime cost",
        "copied_bytes": None,
        "total_allocations": None,
    }
    raw_keys = {
        t + (p.info["layer"], p.feature, p.part)
        for t, s in snapshots.items()
        for p in s.pieces
    }
    ledgers = [row for c in cells.values() for row in c.ledger]
    if {row["original"] for row in ledgers} != raw_keys:
        raise ValueError("original source identity lost")
    report = {
        "scope": scope,
        "provider_original_completeness_claimed": False,
        "input_sha256": manifest["sha256"],
        "source_cells": len(cells),
        "raw_source_parts": len(raw_keys),
        "normalized_parts": len(records),
        "empty_owned_cells": sum(not c.snapshot.pieces for c in cells.values()),
        "outside_parts_retained": sum(len(c.outside) for c in cells.values()),
        "outside_area_tile_units2": sum(r["outside_area"] for r in ledgers),
        "owned_area_tile_units2": sum(r["owned_area"] for r in ledgers),
        "max_area_balance_error_tile_units2": max(
            abs(r["area_balance_error"]) for r in ledgers
        ),
        "raw_geometry_unchanged": all(
            c.original is snapshots[t] or c.original.pieces == snapshots[t].pieces
            for t, c in cells.items()
        ),
        "retained_outside_wkb_bytes": sum(
            len(p.wkb) for c in cells.values() for p in c.outside.values()
        ),
        "normalized_wkb_bytes": sum(len(p.wkb) for p in polygons),
        "geometric_contact_intervals": sum(len(c.contacts) for c in cells.values()),
        "declared_support_tile_units": 0,
        "manufactured_neighbor_receipts": 0,
        "normalized_baseline_bodies": len(bodies),
        "oracle_work": oracle_work,
        "seed_states": dict(states),
        "seed_reasons": dict(reasons),
        "max_tile_reads": max(reads),
        "cost": cost,
        "limits": "Original/outside geometries retained; unknown cells stay unknown. Outside-to-recipient handoff is not implemented/admitted. Edge intervals are geometric candidates, not solved height contacts. No native/DB/IO/performance admission or cell-coarse height/bed product.",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").write_text(json.dumps(report, indent=2))
    selected = max(
        cells.values(), key=lambda c: sum(p.area for p in c.outside.values())
    )
    fig, axes = plt.subplots(1, 3, figsize=(15, 5), layout="constrained")
    sets = [
        ("Delivered buffered originals", [p.polygon for p in selected.original.pieces]),
        ("Owned closed cell", [p.polygon for p in selected.snapshot.pieces]),
        ("Retained outside; no neighbor receipt", list(selected.outside.values())),
    ]
    for ax, (title, shapes) in zip(axes, sets, strict=True):
        for shape in shapes:
            for part in shapely.get_parts(shape):
                if part.geom_type == "Polygon":
                    for ring in (part.exterior, *part.interiors):
                        ax.plot(*ring.xy, linewidth=0.7, color="tab:blue")
        _, x, y = selected.snapshot.tile
        ax.plot([x, x + 1, x + 1, x, x], [y, y, y + 1, y + 1, y], "k--")
        ax.set(
            title=title, xlabel="Global tile X", ylabel="Global tile Y", aspect="equal"
        )
    fig.savefig(output / "ownership.png", dpi=150)
    plt.close(fig)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(compare(args.fixture, args.output), indent=2))


if __name__ == "__main__":
    main()
