"""Actual same-body deck/hull alternatives; source preparation is measured apart."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from bridge_end_body import BodyRecipe, build_end_body
from bridge_end_body_checks import qualify_body
from bridge_end_surface_inputs import actual_end_inputs
from replay_canonical_bridge_ports import allocations
from replay_canonical_ports import process_peak, serializable


def space_data(row):
    return {
        "owner_id": row.owner_id,
        "purpose": row.purpose,
        "geometry_wkb_hex": row.geometry.wkb_hex,
        "origin_xy_m": row.origin_xy_m,
        "lower_plane_m": row.lower_plane_m,
        "upper_plane_m": row.upper_plane_m,
    }


def signature(products, *, spaces=False):
    result = hashlib.sha256()
    for product, _ in products:
        for key in ("surface", "body"):
            result.update(product[key].vertices.tobytes())
            result.update(product[key].triangles.tobytes())
        if spaces:
            result.update(
                json.dumps(
                    [space_data(r) for r in product["occupancies"]], sort_keys=True
                ).encode()
            )
    return result.hexdigest()


def replay(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, index, prepared, ground, preparation = actual_end_inputs()
    before = (
        ground["surface"].vertices.tobytes() + ground["surface"].triangles.tobytes()
    )
    unknown = source.unknown_nodes
    groups = [
        v["source_terminal"]["group"]
        for v in ground["terminal_values"]
        if any(
            index.index[1][m.edge[:2]].bridge
            for m in index.groups_by_node[v["source_terminal"]["source_node"]].members
        )
    ]
    if len(groups) != 2:
        raise ValueError("Two actual bound end groups required")
    recipe = BodyRecipe()

    def stage(method):
        start_cpu, start_wall = time.process_time(), time.perf_counter()
        products = []
        for group in groups:
            product, report = build_end_body(
                prepared, ground, index, group, recipe=recipe, method=method
            )
            report["whole_body_quality"] = qualify_body(product, report["body"])
            products.append((product, report))
        return products, {
            "cpu_s": time.process_time() - start_cpu,
            "wall_s": time.perf_counter() - start_wall,
        }

    results = {method: stage(method)[0] for method in ("faces", "whole")}
    expected = signature(results["whole"])
    if signature(results["faces"]) != expected:
        raise ValueError("Compared deck geometry differs")
    space_signatures = {m: signature(p, spaces=True) for m, p in results.items()}
    samples = {method: [] for method in results}
    for turn in range(args.rounds):
        for method in tuple(results) if turn % 2 == 0 else tuple(reversed(results)):
            value, cost = stage(method)
            if signature(value, spaces=True) != space_signatures[method]:
                raise ValueError("Timed geometry or occupied space differs")
            samples[method].append(cost)
    peak, peak_source = process_peak()
    traces = {}
    if args.trace:
        import memray

        for method in results:
            path = args.output / (method + ".bin")
            with memray.Tracker(
                str(path), trace_python_allocators=True, native_traces=True
            ):
                value, _ = stage(method)
            if signature(value, spaces=True) != space_signatures[method]:
                raise ValueError("Traced geometry or occupied space differs")
            traces[method] = allocations(path) | {
                "scope": "Two warm source-bound end surfaces, whole closed decks, occupied-space products and independent whole-output checks. Source/ground preparation, profiling and plots excluded; allocation requests are not copied bytes."
            }
    if (
        unknown != source.unknown_nodes
        or before
        != ground["surface"].vertices.tobytes() + ground["surface"].triangles.tobytes()
    ):
        raise ValueError("Original Ground or Source Unknown was changed")
    arrays = {
        "ground_vertices": ground["surface"].vertices,
        "ground_faces": ground["surface"].triangles,
    }
    for i, (product, _) in enumerate(results["whole"]):
        for key in ("surface", "body"):
            arrays[f"end{i}_{key}_vertices"] = product[key].vertices
            arrays[f"end{i}_{key}_faces"] = product[key].triangles
    np.savez(args.output / "geometry.npz", **arrays)
    for products in results.values():
        for product, record in products:
            record["occupied_space"] = [space_data(r) for r in product["occupancies"]]
    report = {
        "schema": "outshine/bridge-end-bodies/1",
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "worktree_status": subprocess.check_output(
            ["git", "status", "--porcelain"], text=True
        ),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "source_ground_preparation": preparation,
        "source_unknown_count": len(unknown),
        "original_ground_sha256": hashlib.sha256(before).hexdigest(),
        "ground_unchanged": True,
        "artifact_sha256": expected,
        "space_signatures": space_signatures,
        "process_peak_rss_bytes_before_profile_plot": peak,
        "process_peak_source": peak_source,
        "stored_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        "methods": {
            method: {
                "records": [r for _, r in products],
                "runs": samples[method],
                "allocations": traces.get(method),
            }
            for method, products in results.items()
        },
        "scope": "Exactly same actual top/closed-body geometry, conserved source-bound local StreetWay/owner IDs and original Unknown. Conservative envelopes satisfy the same retained1e-8m maximum exact cap-padding contract, with different measured padding rather than asserted equal occupied volume. Local recipe thickness, no survey/statics. No new library/native/World admission or terrain/crossing clearance; existing affine-query numeric limitations retained. Total copied bytes, native assets/cache/device/GPU/frame costs unmeasured.",
    }
    (args.output / "report.json").write_text(
        json.dumps(serializable(report), indent=2, default=lambda v: v.tolist()) + "\n"
    )
    if args.plot:
        from bridge_end_body_plot import draw

        draw(results["whole"], args.output)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "methods": {
                    method: {
                        "cpu_s": [v["cpu_s"] for v in samples[method]],
                        "volumes": sum(len(p["occupancies"]) for p, _ in products),
                        "allocations": traces.get(method),
                    }
                    for method, products in results.items()
                },
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--plot", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.rounds <= 16:
        parser.error("--rounds must be1..16")
    replay(args)


if __name__ == "__main__":
    main()
