"""Offline metric boundary hints on SHA-pinned OSM polygons; no native release claim."""

import argparse
import json
import math
import time
from collections import Counter, defaultdict
from pathlib import Path

import matplotlib
import numpy as np
import shapely
from ground_edge_inputs import coalesced, polygons, prepare, resolved, segments
from outshine_ref.contracts import LocalFrame
from outshine_ref.materials import filtered_edge_weight

matplotlib.use("Agg")
from matplotlib import pyplot as plt


def buckets(edges, cell_m, reach_m):
    cells = defaultdict(list)
    lower = np.floor((edges.min(axis=1) - reach_m) / cell_m).astype(int)
    upper = np.floor((edges.max(axis=1) + reach_m) / cell_m).astype(int)
    for index, (low, high) in enumerate(zip(lower, upper, strict=True)):
        for y in range(low[1], high[1] + 1):
            for x in range(low[0], high[0] + 1):
                cells[(x, y)].append(index)
    return cells


def distances(edges, points, cells, cell_m):
    result = np.full(len(points), np.inf)
    for index, point in enumerate(points):
        candidates = cells.get(tuple(np.floor(point / cell_m).astype(int)), [])
        if not candidates:
            continue
        chosen = edges[candidates]
        direction = chosen[:, 1] - chosen[:, 0]
        fraction = np.clip(
            np.sum((point - chosen[:, 0]) * direction, axis=1)
            / np.sum(direction * direction, axis=1),
            0.0,
            1.0,
        )
        result[index] = np.min(
            np.linalg.norm(chosen[:, 0] + fraction[:, None] * direction - point, axis=1)
        )
    return result


def inspect(rows, cell_m, reach_m):
    counts = Counter()
    support_m = reach_m + 0.5 * cell_m
    halos = (0.0, reach_m, support_m)
    totals = {
        str(halo): {
            "references": 0,
            "cells": 0,
            "misses": 0,
            "build_ms": 0.0,
            "query_ms": 0.0,
        }
        for halo in halos
    }
    occupied = {str(halo): Counter() for halo in halos}
    references = {str(halo): Counter() for halo in halos}
    for row in rows:
        polygon = row["polygon"]
        edges = segments(polygon)
        selected = edges[np.linspace(0, len(edges) - 1, min(8, len(edges)), dtype=int)]
        tangent = selected[:, 1] - selected[:, 0]
        normal = tangent[:, ::-1] * [1.0, -1.0]
        normal /= np.linalg.norm(normal, axis=1)[:, None]
        points = np.concatenate(
            [
                selected.mean(axis=1) + offset * radius * normal
                for radius in (reach_m, support_m)
                for offset in (-0.9, -0.1, 0.1, 0.9)
            ]
        )
        expected = shapely.distance(polygon.boundary, shapely.points(points))
        counts.update(features=1, edges=len(edges), probes=len(points))
        counts["holes"] += sum(
            len(part.interiors) for part in shapely.get_parts(polygon)
        )
        for halo in halos:
            started = time.perf_counter()
            cells = buckets(edges, cell_m, halo)
            built = time.perf_counter()
            actual = distances(edges, points, cells, cell_m)
            done = time.perf_counter()
            values = totals[str(halo)]
            values["references"] += sum(map(len, cells.values()))
            values["max_references_per_seed"] = max(
                values.get("max_references_per_seed", 0),
                max(map(len, cells.values()), default=0),
            )
            values["cells"] += len(cells)
            values["misses"] += int(np.count_nonzero(abs(actual - expected) > 1e-7))
            values["band_misses"] = values.get("band_misses", 0) + int(
                np.count_nonzero(
                    (expected <= reach_m) & (abs(actual - expected) > 1e-7)
                )
            )
            values["build_ms"] += 1000 * (built - started)
            values["query_ms"] += 1000 * (done - built)
            occupied[str(halo)].update(cells.keys())
            references[str(halo)].update(
                {cell: len(values) for cell, values in cells.items()}
            )
    for halo, cells in occupied.items():
        totals[halo]["max_features_per_cell"] = max(cells.values(), default=0)
        totals[halo]["max_references_per_cell"] = max(
            references[halo].values(), default=0
        )
    if totals[str(reach_m)]["band_misses"] or totals[str(support_m)]["misses"]:
        raise ValueError("metric halo omitted a nearest source boundary")
    return {"geometry": dict(counts), "support_m": support_m, "variants": totals}


def plot(path):
    x = np.linspace(-3.0, 3.0, 1201, dtype=np.float32)
    fig, axes = plt.subplots(1, 3, figsize=(15, 4), constrained_layout=True)
    for ax, half in zip(axes, (0.02, 0.5, 2.0), strict=True):
        old = np.clip(0.5 + 0.5 * x / half, 0.0, 1.0)
        old = old * old * (3.0 - 2.0 * old) * (x >= 0)
        ax.plot(x, old, label="inside-only pixel transition", color="#ad4030")
        ax.plot(
            x,
            filtered_edge_weight(x, 0.9, half),
            label="metric band + box filter",
            color="#357c48",
        )
        ax.axvline(0, color=".6", linewidth=0.7)
        ax.set(
            title=f"half footprint {half:g} m",
            xlabel="signed boundary distance [m]",
            ylim=(-0.03, 1.03),
        )
        ax.grid(alpha=0.2)
    axes[0].set_ylabel("positive-side material weight")
    axes[-1].legend(loc="lower right", fontsize=8)
    fig.suptitle(
        "Ground contact reference: reach 0.9 m; one-dimensional normal footprint"
    )
    fig.savefig(path, dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--receipts",
        type=Path,
        default=Path("build/experiments/render/ground-edges/inputs/receipts.json"),
    )
    parser.add_argument(
        "--output", type=Path, default=Path("build/experiments/render/ground-edges")
    )
    parser.add_argument("--prepare-from-cache", type=Path)
    parser.add_argument("--cell-m", type=float, default=16.0)
    args = parser.parse_args()
    inventory = json.loads(
        Path("doc/references/data/osm/openfreemap-inventory-20261009.json").read_text()
    )
    if args.prepare_from_cache is not None:
        prepare(args.prepare_from_cache, args.receipts, inventory)
    sources = json.loads(args.receipts.read_text())
    if not sources["sites"]:
        raise ValueError("no SHA-verified OSM input; no boundary proof produced")
    recipes = json.loads(Path("src/assets/world/vegetation.json").read_text())
    reach_m = max(row["ground"].get("edgeReachM", 0.0) for row in recipes["templates"])
    if not math.isfinite(args.cell_m) or args.cell_m <= 0:
        raise ValueError("cell size must be finite positive metres")
    report = {
        "sources": sources["sites"],
        "cell_m": args.cell_m,
        "reach_m": reach_m,
        "sites": {},
        "omitted_sites": sorted(
            set(sources["requested_sites"]) - set(sources["sites"])
        ),
        "scope": "OSM polygon boundary references only; no native classification, render, GPU or A18 release.",
    }
    for site, source in sources["sites"].items():
        rows, omitted = polygons(site, source, inventory)
        report["sites"][site] = inspect(rows, report["cell_m"], report["reach_m"]) | {
            "omitted": omitted
        }
        known, unknown = resolved(rows, recipes)
        frame = LocalFrame(f"ground-edges-{site}-{source['receipt']['sha256'][:12]}")
        combined, coverage = coalesced(known, frame)
        report["sites"][site]["coalesced"] = (
            inspect(combined, report["cell_m"], report["reach_m"]) | coverage
        )
        report["sites"][site]["unresolved_source_classes"] = unknown
        print(site, json.dumps(report["sites"][site]["geometry"]), flush=True)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "proof.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(args.output / "metric-material-contacts.png")


if __name__ == "__main__":
    main()
