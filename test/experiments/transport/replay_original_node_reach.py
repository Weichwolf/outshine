"""Observe original-point displacement without admitting a whole missing Way."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import (
    positions_in_frame,
    surface_reach_bound,
)


def observations(source, groups, budget):
    if (
        source.binding.get("source_facts_schema")
        != "outshine/native-mvt-source-facts/4"
    ):
        raise ValueError("actual bound v4 source facts required")
    missing = source.binding.get("missing_original_ways", ())
    count = sum(len(row["OriginalPoints"]) for row in missing)
    if count > budget.incidence_visits:
        raise ValueError("original node observation budget")
    positions = {
        int(group.identifier.rsplit(":", 1)[1]): group.position for group in groups
    }
    rows, distances = [], []
    for way in missing:
        if way["OriginalWayState"] != 1:
            raise ValueError("exact original Way required for this diagnostic")
        for ordinal, (raw, point) in enumerate(
            zip(way["Points"], way["OriginalPoints"], strict=True)
        ):
            distance = np.nan
            if point["State"] == 2:
                anchor = raw["LongitudeDeg"], raw["LatitudeDeg"]
                target = np.asarray([positions[point["Group"]]])
                xyz = positions_in_frame(anchor, target, np.zeros(1))[0]
                distance = np.linalg.norm(xyz)
            elif point["State"] != 1:
                raise ValueError("unresolved original node position")
            rows.append((way["Way"]["StreetWay"], ordinal, point["State"]))
            distances.append(distance)
    return np.array(rows, dtype=np.uint64).reshape(-1, 3), np.array(distances)


def plot(output, distances, bound):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    observed = np.isfinite(distances)
    fig, axis = plt.subplots(figsize=(10, 4), layout="constrained")
    axis.scatter(np.flatnonzero(observed), distances[observed], s=14)
    axis.axhline(bound, color="C3", label=f"Derived point bound: {bound:.6f} m")
    axis.set(
        xlabel="Ordered original missing-Way point",
        ylabel="Zero-height WGS84 chord (m)",
        title=f"{observed.sum()} published-group observations; {(~observed).sum()} native positions unavailable\nNo whole-way / field influence or source admission",
    )
    axis.legend()
    fig.savefig(output / "original-node-reach.png", dpi=140)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--snap-m", type=float, required=True)
    parser.add_argument("--sphere-radius-m", type=float, required=True)
    args = parser.parse_args()
    bound = surface_reach_bound(args.snap_m, args.sphere_radius_m)
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        raise ValueError("Private diagnostics belong in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    budget = PortBudget()
    started = time.process_time()
    source, groups, _ways, _binding, _reader = load_bridge_source_facts(
        args.input, budget=budget
    )
    preparation_cpu = time.process_time() - started
    started = time.process_time()
    indices, distances = observations(source, groups, budget)
    observed = np.isfinite(distances)
    if np.any(distances[observed] > bound):
        raise ValueError("actual native group displacement exceeds producer bound")
    comparison_cpu = time.process_time() - started
    np.savez_compressed(
        args.output / "observations.npz", indices=indices, distances_m=distances
    )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_facts_digest": source.binding["source_facts_digest"],
        "native_producer_commit": source.binding["native_capture"][
            "native_producer_commit"
        ],
        "producer_parameters": {
            "snap_m": args.snap_m,
            "sphere_radius_m": args.sphere_radius_m,
        },
        "point_count": len(distances),
        "observed_published_points": int(observed.sum()),
        "unobserved_valid_native_points": int((~observed).sum()),
        "maximum_observed_chord_m": float(distances[observed].max(initial=0)),
        "surface_reach_bound_m": bound,
        "retained_result_array_bytes": indices.nbytes + distances.nbytes,
        "reader_preparation_cpu_s": preparation_cpu,
        "comparison_cpu_s_single_run": comparison_cpu,
        "total_internal_copied_bytes": None,
        "scope": "Current StreetGraph producer parameters are explicit assumptions, not encoded by v4. Point metric only; no inserted normalized-way vertices, width/terrain/C1 influence, Source/Unknown/World guard change or device/frame claim. State1 positions stay unobserved, never zero.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    plot(args.output, distances, bound)
    print(json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
