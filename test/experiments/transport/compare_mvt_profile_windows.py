"""Window-selection requests, unchanged captured-port control and bounded metadata cost."""

import argparse
import copy
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import mvt_profile_windows as windows
import numpy as np
from mvt_native_cut_profiles import cut_profiles
from mvt_stair_boundary_neighbours import node_at
from mvt_t_connection_inputs import FIXTURE, source_rows
from mvt_t_connections import plan_connections
from test_mvt_native_body import construct


def scan_lookup(way):
    def at(point):
        try:
            return node_at(way, point)
        except (ValueError, KeyError):
            return None

    return at


def qualify(case, native, result):
    if result["status"] != "captured_candidate":
        return None, {
            "valid": False,
            "reason": "boundary_contract_or_window_incomplete",
        }
    selected = {w["source_id"] for w in result["windows"]}
    keys = {
        tuple(key)
        for window in result["windows"]
        for key in window.get("captured_profile_keys", [])
    }

    local = copy.deepcopy(case)
    local["ways"] = [
        way for way in local["ways"] if f"native-part:{way['native_way']}" in selected
    ]
    for way in local["ways"]:
        way["sampled_design_xyz_m"] = [
            row[:3] for row in way["sampled_design_xy_height_node"]
        ]
    ports = copy.deepcopy(native)
    ports["cuts"] = [
        cut
        for cut in ports["cuts"]
        if (f"native-part:{cut['native_way']}", cut["source_station_m"]) in keys
    ]
    _solver, _plan, _contacts, _heights, data, geometry, proof, fixed, products = (
        construct(local, ports)
    )
    return (data, products), {
        "valid": True,
        "geometry": geometry,
        "profile_audit": proof,
        "fixed_contacts": fixed,
        "scope": "Python body/consumers for automatically selected unchanged captured interfaces. No new actual cuts, outside/native nav/terrain or device/Place admission.",
    }


def fit_attempts(case, native, proposal, seed, ports, paddings, *, qualifier=qualify):
    if not 1 <= len(paddings) <= 4 or any(
        not 0 <= a < b <= 128
        for a, b in zip((0, *paddings[:-1]), paddings, strict=True)
    ):
        raise ValueError(
            "one to four strictly increasing positive bounded paddings required"
        )
    attempts = []
    rejected = set()
    for padding in paddings:
        candidate = windows.select_window(
            case, proposal, seed, padding_m=padding, interfaces=ports
        )
        if candidate["status"] != "captured_candidate":
            attempts.append(
                {
                    "padding_m": padding,
                    "status": "input_required_or_deferred",
                    "selection": candidate,
                }
            )
            return None, {
                "valid": False,
                "reason": "selection_contract_or_budget",
                "attempts": attempts,
            }
        key = tuple(
            (row["source_id"], tuple(row["source_station_range_m"]))
            for row in candidate["windows"]
        )
        if key in rejected:
            attempts.append(
                {"padding_m": padding, "status": "same_rejected_boundaries_skipped"}
            )
            continue
        try:
            products, proof = qualifier(case, native, candidate)
        except ValueError as error:
            products, proof = None, {"valid": False, "reason": str(error)}
        attempts.append(
            {
                "padding_m": padding,
                "status": "qualified" if proof["valid"] else "fit_rejected",
                "selection": candidate,
                "proof": proof,
            }
        )
        if proof["valid"]:
            return products, {"valid": True, "attempts": attempts}
        rejected.add(key)
    return None, {
        "valid": False,
        "reason": "finite_window_schedule_exhausted",
        "attempts": attempts,
    }


def compare(case, proposal, seed, ports, method, repeats, trace):
    lookup = windows._node_lookup if method == "indexed" else scan_lookup
    if trace:
        tracemalloc.start()
    began = time.perf_counter()
    with patch.object(windows, "_node_lookup", lookup):
        for _ in range(repeats):
            requested = windows.select_window(case, proposal, seed, padding_m=2)
            captured = windows.select_window(
                case, proposal, seed, padding_m=2, interfaces=ports
            )
    elapsed = time.perf_counter() - began
    memory = {}
    if trace:
        _current, memory["python_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        memory["python_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    samples = sum(len(w["sampled_design_xy_height_node"]) for w in case["ways"])
    visits = (
        samples
        if method == "indexed"
        else sum(
            len(w["sampled_design_xy_height_node"]) * len(w["source_xy_m"])
            for w in case["ways"]
        )
    )
    report = {
        "method": method,
        "repeats": repeats,
        "instrumented": trace,
        "request_and_captured_pair_s": elapsed / repeats,
        "native_node_lookup_sample_visits_per_pair": 2 * visits,
        "common_sample_validation_visits_per_pair": 2 * samples,
        "process_peak_rss_bytes": rss if sys.platform == "darwin" else rss * 1024,
        **memory,
        "scope": "Repeated same pair of full metadata selections, after source/proposal/canonical-port preparation; context normalization, dictionaries/arrays, station requests and cut choice included. Body/consumer qualification and images/export follow measurement. Reference scans use the existing exact-coordinate node_at helper; indexed path prepares exact XY→native-node sets once per source per selection. No fine-IO/native/frame claim.",
    }
    return {"requested": requested, "captured": captured}, report


def native_plane_evidence(native, requests):
    evidence = []
    for request in requests:
        triangles = np.array(
            [
                triangle["enu_m"]
                for cut in native["cuts"]
                if f"native-part:{cut['native_way']}" == request["source_id"]
                for triangle in cut["triangles"]
            ]
        )
        if len(triangles):
            values = (triangles[:, :, :2] - request["center_xy_m"]) @ np.array(
                request["tangent_xy"]
            )
            count = int(np.sum((values.min(axis=1) <= 0) & (values.max(axis=1) >= 0)))
        else:
            count = 0
        evidence.append(
            {
                "source_id": request["source_id"],
                "source_station_m": request["source_station_m"],
                "stored_owned_faces": len(triangles),
                "floating_infinite_plane_crossing_faces": count,
                "scope": "Only frozen faces at the four original control cuts. Zero crossings cannot supply the new interface; a positive count does not establish completeness at another station.",
            }
        )
    return evidence


def display(case, results, ports, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(1, 2, figsize=(12, 6))
    by_key = {
        f"native-part:{w['native_way']}": np.array(w["source_xy_m"])
        for w in case["ways"]
    }
    selected = {w["source_id"] for w in results["captured"]["windows"]}
    for axis in axes:
        for key, xy in by_key.items():
            axis.plot(
                xy[:, 0],
                xy[:, 1],
                color="#45674a" if key in selected else "#b9bdb9",
                linewidth=1.5,
            )
        for request in results["requested"]["profile_requests"]:
            point = request["center_xy_m"]
            axis.plot(*point, "x", color="#b67432", markersize=8)
        for port in ports:
            xy = np.array(port["profile_xyz_m"])[:, :2]
            axis.plot(xy[:, 0], xy[:, 1], color="#7b4c82", linewidth=2)
        axis.set_aspect("equal")
        axis.set(xlabel="east / m", ylabel="north / m")
    points = np.concatenate([np.array(p["profile_xyz_m"])[:, :2] for p in ports])
    low, high = points.min(axis=0) - 3, points.max(axis=0) + 3
    axes[1].set(xlim=(low[0], high[0]), ylim=(low[1], high[1]))
    axes[0].set_title("Seven retained sources; four remain unchanged")
    axes[1].set_title("Orange: new profile requests; purple: captured control ports")
    figure.suptitle(
        "Bounded source-station selection; new native boundary heights are required"
    )
    figure.tight_layout()
    figure.savefig(output / "profile-windows.png", dpi=150, bbox_inches="tight")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--method", choices=("indexed", "scan"), default="indexed")
    parser.add_argument("--repeats", type=int, default=100)
    parser.add_argument("--trace-memory", action="store_true")
    parser.add_argument("--image", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 1000:
        raise ValueError("bounded repeat count required")
    case = json.loads(
        FIXTURE.with_name("mvt_stair_boundary_neighbours.json").read_text()
    )
    native = json.loads(
        FIXTURE.with_name("mvt_stair_native_crosscuts.json").read_text()
    )
    proposal = plan_connections(source_rows(case["ways"]))
    ports = cut_profiles(native)
    seeds = sorted({c["source_id"] for c in proposal["connections"]})
    if len(seeds) != 1:
        raise ValueError(
            "fixed comparison expects one automatically identified changed seed"
        )
    results, report = compare(
        case, proposal, seeds[0], ports, args.method, args.repeats, args.trace_memory
    )
    products, qualification = fit_attempts(
        case, native, proposal, seeds[0], ports, (2, 4, 8, 16)
    )
    report["captured_control_qualification"] = qualification
    report["new_port_input_evidence"] = native_plane_evidence(
        native, results["requested"]["profile_requests"]
    )
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "windows.json").write_text(json.dumps(results, indent=2) + "\n")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    if products is not None:
        data, shared = products
        np.savez(args.output / "body.npz", **data)
        (args.output / "shared-products.json").write_text(
            json.dumps(shared, indent=2) + "\n"
        )
    if args.image:
        display(case, results, ports, args.output)
    print(
        json.dumps(
            {
                "method": args.method,
                "request_and_captured_pair_s": report["request_and_captured_pair_s"],
                "qualified": qualification["valid"],
            }
        )
    )


if __name__ == "__main__":
    main()
