"""Query actual native interfaces and compare bounded near/outer port choices."""

import argparse
import copy
import hashlib
import json
import time
from collections import defaultdict
from itertools import pairwise
from pathlib import Path

import numpy as np
from compare_mvt_native_body import orientation_counts
from compare_mvt_profile_windows import fit_attempts
from mvt_native_cut_profiles import cut_profiles
from mvt_native_surface_queries import query_profiles
from mvt_profile_windows import select_window
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connections import plan_connections


def sample_requests(case, proposal, seed, paddings):
    if (
        not 1 <= len(paddings) <= 4
        or any(not 0 < a < b <= 128 for a, b in pairwise(paddings))
        or not 0 < paddings[0] <= 128
    ):
        raise ValueError("one to four bounded increasing paddings required")
    select_window(case, proposal, seed, padding_m=paddings[0])
    targets = defaultdict(list)
    for connection in proposal["connections"]:
        if connection["source_id"] == seed:
            targets[connection["target_source_id"]].append(
                connection["target_station_m"]
            )
    source_ids = {
        source["source_id"]: way["native_way"]
        for source, way in zip(source_rows(case["ways"]), case["ways"], strict=True)
    }
    return [
        {
            "native_way": source_ids[key],
            "source_station_m": station + direction * padding,
            "side": side,
            "padding_m": padding,
        }
        for key, stations in sorted(targets.items())
        for side, station, direction in (
            ("lower", min(stations), -1),
            ("upper", max(stations), 1),
        )
        for padding in paddings
    ]


def outer_candidates(native):
    grouped = defaultdict(list)
    for cut in native["cuts"]:
        grouped[cut["native_way"]].append(cut)
    result = copy.deepcopy(native)
    result["cuts"] = []
    for rows in grouped.values():
        ordered = sorted(rows, key=lambda row: row["source_station_m"])
        if len(ordered) < 2:
            raise ValueError("two distinct observed interfaces per source required")
        result["cuts"].extend((ordered[0], ordered[-1]))
    return result


def compare(case, native, crop, paddings):
    proposal = plan_connections(source_rows(case["ways"]))
    seeds = {connection["source_id"] for connection in proposal["connections"]}
    if len(seeds) != 1:
        raise ValueError(
            "comparison requires one changed seed; no place-specific selection"
        )
    seed = seeds.pop()
    queries = sample_requests(case, proposal, seed, paddings)
    began = time.perf_counter()
    candidates, profiles, notes = query_profiles(native, crop, queries)
    queried = time.perf_counter()
    _near, near = fit_attempts(case, candidates, proposal, seed, profiles, paddings)
    nearest_done = time.perf_counter()
    outer = outer_candidates(candidates)
    products, wide = fit_attempts(
        case, outer, proposal, seed, cut_profiles(outer), (paddings[0],)
    )
    outer_done = time.perf_counter()
    report = {
        "queries": notes,
        "queried_original_faces": sum(len(page["triangles"]) for page in crop["pages"]),
        "phase_times_s": {
            "queries": queried - began,
            "closest_fit_attempts": nearest_done - queried,
            "outer_fit_attempt": outer_done - nearest_done,
        },
        "bounded_closest_ports": near,
        "bounded_outer_ports": wide,
        "scope": "New finite geometric interfaces from retained native surfaces. Wider observed ports are a diagnostic alternative, not a general selection/ownership algorithm. Outside packing, native nav/terrain and world admission remain open.",
    }
    if products:
        data, _shared = products
        report["outer_body_float32"] = dict(
            orientation_counts(data), vertices=len(data["body_vertices"])
        )
    return candidates, outer, products, report


def display(
    crop, data, path, *, replacement_title="New queried outer interfaces / replacement"
):
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    original = np.array(
        [face["enu_m"] for page in crop["pages"] for face in page["triangles"]]
    )
    replacement = data["body_vertices"][data["body_faces"]]
    low, high = replacement.min(axis=(0, 1)), replacement.max(axis=(0, 1))
    margin = np.maximum((high - low) * 0.08, 1)
    low, high = low - margin, high + margin
    figure = plt.figure(figsize=(13, 6))
    for at, triangles, title in (
        (1, original, "Retained native surfaces"),
        (2, replacement, replacement_title),
    ):
        axis = figure.add_subplot(1, 2, at, projection="3d")
        axis.add_collection3d(
            Poly3DCollection(
                triangles,
                facecolors=(0.7, 0.74, 0.69, 1),
                edgecolors=(0.12, 0.15, 0.17, 0.35),
                linewidths=0.25,
            )
        )
        axis.set(
            xlim=(low[0], high[0]),
            ylim=(low[1], high[1]),
            zlim=(low[2], high[2]),
            xlabel="East / m",
            ylabel="North / m",
            zlabel="Up / m",
            title=title,
        )
        axis.set_box_aspect(np.maximum(high - low, 1))
        axis.view_init(elev=29, azim=-54)
    figure.suptitle(
        "Local geometry only; outside mesh, ownership, nav/terrain integration remain open"
    )
    figure.tight_layout()
    figure.savefig(path, dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    case = json.loads(
        FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
    )
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    raw = FIXTURE.with_name("mvt_stair_native_surface_crop.json").read_bytes()
    if (
        hashlib.sha256(raw).hexdigest()
        != native["provenance"]["complete_cropped_export_sha256"]
    ):
        raise ValueError("original retained native export hash mismatch")
    crop = json.loads(raw)
    candidates, outer, products, report = compare(case, native, crop, (2, 4, 8, 16))
    args.output.mkdir(parents=True, exist_ok=True)
    for name, value in (
        ("candidates.json", candidates),
        ("outer-interfaces.json", outer),
        ("report.json", report),
    ):
        (args.output / name).write_text(json.dumps(value, indent=2) + "\n")
    if products:
        data, shared = products
        np.savez(args.output / "body.npz", **data)
        (args.output / "shared-products.json").write_text(
            json.dumps(shared, indent=2) + "\n"
        )
        if args.image:
            display(crop, data, args.output / "body.png")
    print(
        json.dumps(
            {
                "queries": len(report["queries"]),
                "profiles": len(candidates["cuts"]),
                "outer_body": report["bounded_outer_ports"]["valid"],
            }
        )
    )


if __name__ == "__main__":
    main()
