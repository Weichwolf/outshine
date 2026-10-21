"""Original-source rational overlap versus complete pairs and numeric controls."""

import argparse
import hashlib
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from inspect_source_membership import DEFAULT, inspect
from water_exact_membership import ExactBudget, exact_overlap


def resolve(triangle, sources, *, indexed, trace=False):
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    tree = shapely.STRtree(sources) if indexed else None
    ids = sorted(tree.query(triangle)) if tree is not None else range(len(sources))
    budget = ExactBudget()
    areas, missing = exact_overlap(
        list(triangle.exterior.coords)[:3], sources[list(ids)], budget
    )
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _live, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
    report = {
        "indexed": indexed,
        "sources": len(sources),
        "candidate_sources": len(ids),
        "seconds": elapsed,
        "work": dict(budget.counts),
        "counted_operations": budget.operations,
        "peak_checked_rational_bits": budget.peak_bits,
        "uncovered_area_m2": float(missing),
        "exact_uncovered": str(missing),
        "members": [
            {"source_index": int(i), "area_m2": float(a), "exact_area": str(a)}
            for i, a in zip(ids, areas, strict=True)
            if a > 0
        ],
        **memory,
    }
    return (
        {int(i): a for i, a in zip(ids, areas, strict=True) if a > 0},
        missing,
        report,
    )


def display(triangle, sources, path):
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon

    points = np.array(triangle.exterior.coords)[:3]
    origin = points[0]
    tangent = points[1] - origin
    tangent /= np.linalg.norm(tangent)
    normal = np.array([-tangent[1], tangent[0]])
    ids = sorted(shapely.STRtree(sources).query(triangle))
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    for axis, magnified in zip(axes, (False, True), strict=True):
        for position, i in enumerate(ids):
            rings = (sources[i].exterior, *sources[i].interiors)
            for ring_index, ring in enumerate(rings):
                xy = np.array(ring.coords) - origin
                if magnified:
                    xy = np.c_[xy @ tangent, (xy @ normal) * 1e9]
                axis.add_patch(
                    Polygon(
                        xy,
                        fill=True,
                        facecolor=plt.colormaps["tab10"](position)
                        if ring_index == 0
                        else "white",
                        alpha=0.3,
                        label=f"Original source {i}",
                    )
                )
        xy = points - origin
        if magnified:
            xy = np.c_[xy @ tangent, (xy @ normal) * 1e9]
        axis.add_patch(
            Polygon(
                xy,
                fill=False,
                edgecolor="crimson",
                linewidth=1.6,
                label="Unchanged native triangle",
            )
        )
        if magnified:
            axis.set(
                xlim=(-0.5, 12.4),
                ylim=(-200, 250),
                xlabel="Along long edge / m",
                ylabel="Transverse / nm",
                title="Diagnostic transverse magnification",
            )
        else:
            axis.set(
                xlim=(-0.8, 0.8),
                ylim=(-12.5, 0.5),
                xlabel="East offset / m",
                ylabel="North offset / m",
                title="Physical local coordinates",
            )
        axis.legend(fontsize=7)
    fig.suptitle(
        "Unchanged source/triangle geometry; exact owner 46 and nonzero uncovered remainder"
    )
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--indexed", action="store_true")
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    original = inspect(DEFAULT)
    with np.load(DEFAULT) as data:
        sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
        triangle = shapely.from_wkb(data["triangle_wkb"].tobytes())
        native_indices = data["source_indices"]
    _areas, _missing, report = resolve(
        triangle, sources, indexed=args.indexed, trace=args.trace
    )
    for row in report["members"]:
        row["native_source_index"] = int(native_indices[row["source_index"]])
    report.update(
        original=original,
        input_sha256=hashlib.sha256(DEFAULT.read_bytes()).hexdigest(),
        process_peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        scope="Exact rational overlap/coverage of stored original binary64 geometry only. No vertex, polygon, identity or height changes; no native/world/device admission. STRtree/GEOS allocation and opaque memory are outside the rational operation counter.",
    )
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(triangle, sources, args.output / "geometry.png")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "sources",
                    "candidate_sources",
                    "seconds",
                    "counted_operations",
                    "uncovered_area_m2",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
