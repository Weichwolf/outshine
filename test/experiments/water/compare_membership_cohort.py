"""Finite regenerated native-union cohort: area oracle versus membership predicates."""

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
from water_exact_membership import ExactBudget, ExactBudgetExceeded, exact_overlap
from water_positive_membership import positive_overlap, prepare_source


def cohort(path, max_faces):
    provenance = inspect(path)
    with np.load(path) as data:
        sources = shapely.get_parts(shapely.from_wkb(data["sources_wkb"].tobytes()))
        union = shapely.from_wkb(data["union_wkb"].tobytes())
        original_ids = data["source_indices"].copy()
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(union))
    if not 1 <= len(triangles) <= max_faces:
        raise ValueError("regenerated cohort exceeds explicit face budget")
    return sources, union, triangles, original_ids, provenance


def run(
    path, method, *, max_faces=4096, global_work=10000000, face_work=100000, trace=False
):
    if method not in ("areas", "rational", "filtered") or any(
        type(n) is not int or not 1 <= n <= cap
        for n, cap in ((max_faces, 4096), (global_work, 10000000), (face_work, 100000))
    ):
        raise ValueError("known method and explicit finite cohort budgets required")
    if trace:
        tracemalloc.start()
    started = time.perf_counter()
    sources, union, triangles, original_ids, provenance = cohort(path, max_faces)
    prepared = [prepare_source(p) for p in sources] if method != "areas" else None
    tree = shapely.STRtree(sources)
    ready = time.perf_counter()
    rows, counts, total = [], {}, 0
    numeric_empty_conflicts = numeric_partial_conflicts = 0
    for i, triangle in enumerate(triangles):
        ids = sorted(tree.query(triangle))
        vertices = np.array(triangle.exterior.coords)[:3]
        work = ExactBudget(limit=min(face_work, global_work - total))
        row = {"face": i, "candidate_sources": list(map(int, ids))}
        try:
            if method == "areas":
                areas, missing = exact_overlap(vertices, sources[ids], work)
                members = [
                    int(j) for j, area in zip(ids, areas, strict=True) if area > 0
                ]
                row["exact_uncovered"] = str(missing)
            else:
                members = [
                    int(j)
                    for j in ids
                    if positive_overlap(
                        vertices, prepared[j], work, exact=method == "rational"
                    )
                ]
            row.update(
                state="complete",
                members=members,
                native_members=original_ids[members].tolist(),
            )
            # Numeric overlay is a diagnosis, not the oracle or an admission path.
            numeric_area = shapely.area(shapely.intersection(sources[ids], triangle))
            numeric = [
                int(j) for j, area in zip(ids, numeric_area, strict=True) if area > 0
            ]
            row["numeric_members"] = numeric
            if numeric != members:
                numeric_empty_conflicts += not bool(numeric)
                numeric_partial_conflicts += bool(numeric)
        except (ValueError, ExactBudgetExceeded) as error:
            row.update(state="declined", reason=str(error))
        total += work.operations
        for key, value in work.counts.items():
            counts[key] = counts.get(key, 0) + value
        rows.append(row)
        if total == global_work:
            rows.extend(
                {"face": j, "state": "global_budget"}
                for j in range(i + 1, len(triangles))
            )
            break
    finished = time.perf_counter()
    memory = {}
    if trace:
        _live, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
    parts = shapely.get_parts(union)
    report = {
        "method": method,
        "faces": len(triangles),
        "sources": len(sources),
        "union_parts": len(parts),
        "union_holes": sum(len(p.interiors) for p in parts),
        "source_input_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "regenerated_wkb_sha256": hashlib.sha256(
            shapely.to_wkb(shapely.GeometryCollection(triangles))
        ).hexdigest(),
        "geos": shapely.geos_version_string,
        "provenance": provenance,
        "all_complete": all(r["state"] == "complete" for r in rows),
        "numeric_empty_conflicts": numeric_empty_conflicts,
        "numeric_nonempty_conflicts": numeric_partial_conflicts,
        "owned_edge_array_bytes": sum(p.edges.nbytes for p in prepared)
        if prepared
        else 0,
        "counted_work": counts,
        "total_counted_work": total,
        "limits": {
            "faces": max_faces,
            "global_work": global_work,
            "face_work": face_work,
        },
        "setup_s": ready - started,
        "evaluation_s": finished - ready,
        "total_s": finished - started,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "rows": rows,
        "scope": "New GEOS CDT of captured original union, not an original complete native mesh replay. Positive-area source IDs only for predicates; areas/owner ranking/coverage separate. Timing includes input decode/hash validation, generation, preparation, query and numeric diagnosis. No native/frame/Places admission; opaque allocations/copy traffic unmeasured.",
        **memory,
    }
    return sources, union, triangles, report


def display(union, triangles, report, path):
    import matplotlib.pyplot as plt
    from shapely.plotting import plot_polygon

    fig, axis = plt.subplots(figsize=(10, 7))
    for part in shapely.get_parts(union):
        plot_polygon(part, ax=axis, add_points=False, color="steelblue", alpha=0.3)
    conflicts = [
        r["face"]
        for r in report["rows"]
        if r["state"] == "complete" and r["members"] != r["numeric_members"]
    ]
    points = shapely.get_coordinates(shapely.centroid(triangles[conflicts]))
    axis.scatter(
        points[:, 0],
        points[:, 1],
        s=12,
        color="crimson",
        label="Sources missed by numeric overlay",
    )
    axis.set(
        aspect="equal",
        xlabel="Native East / m",
        ylabel="Native North / m",
        title=f"Regenerated cohort: {len(triangles)} triangles / original union, no source moved",
    )
    axis.legend()
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--method", choices=("areas", "rational", "filtered"), required=True
    )
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    _sources, union, triangles, report = run(args.input, args.method, trace=args.trace)
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if args.image:
        display(union, triangles, report, args.output / "cohort.png")
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "method",
                    "all_complete",
                    "faces",
                    "numeric_empty_conflicts",
                    "numeric_nonempty_conflicts",
                    "total_s",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
