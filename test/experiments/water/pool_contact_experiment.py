"""Original swimming-pool body and bounded exact single-cell contact with open ports."""

import argparse
import gc
import json
import os
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from outshine_ref.water.geodetic import local_points
from pool_body_experiment import FIXTURE, construct, original
from pool_terrain_contact import contact_patch


def measured(body, terrain, faces, repeats):
    times = []
    for _ in range(repeats + 1):
        wall, cpu = time.perf_counter(), time.process_time()
        result = contact_patch(body, terrain, faces)
        times.append([time.perf_counter() - wall, time.process_time() - cpu])
        del result
    gc.collect()
    tracemalloc.start()
    vertices, indices, obligations, _ = contact_patch(body, terrain, faces)
    _, peak = tracemalloc.get_traced_memory()
    blocks = sum(s.count for s in tracemalloc.take_snapshot().statistics("filename"))
    tracemalloc.stop()
    return {
        "first_wall_cpu_s": times[0],
        "following_wall_cpu_s": times[1:],
        "median_wall_cpu_s": np.median(times[1:], axis=0).tolist(),
        "python_traced_peak_bytes": peak,
        "python_traced_live_blocks": blocks,
        "mesh_numeric_bytes": vertices.nbytes + indices.nbytes,
        "obligation_numeric_bytes": obligations.nbytes,
        "scope": "Bounded rational/GEOS CPU reference; no native win. Tracemalloc excludes GEOS heap, live blocks are not cumulative allocations.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("bounded repeat count between 1 and 21 required")
    data, source, replay, triangles, _ = original(args.input)
    body = construct(data, source, triangles)
    lonlat = np.array([[p["lon"], p["lat"]] for p in data["water"]["points"]])
    terrain = local_points(
        data["water"]["meshAnchor"],
        lonlat,
        np.array(data["terrain"]["cellCornersM"]).ravel(),
    )
    faces = np.array([[0, 2, 3], [0, 3, 1]])
    vertices, indices, obligations, report = contact_patch(body, terrain, faces)
    try:
        contact_patch(body, terrain, faces, method="geos")
        report["geos_reference"] = {"state": "valid on this input/platform"}
    except ValueError as error:
        report["geos_reference"] = {"state": "declined", "reason": str(error)}
    report["source_replay"] = replay
    report["source_metadata"] = source
    report["source_terrain_scope"] = (
        "Four actual saved geodetic/F32 heights placed in existing WGS84 ENU model; explicit reference diagonal, not native vertex/shader word equivalence."
    )
    report["loadavg"] = os.getloadavg()
    report["cost"] = measured(body, terrain, faces, args.repeats)
    report["process_peak_rss_before_plot_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(args.output / "body.npz", **body)
    np.savez(
        args.output / "terrain.npz",
        original_vertices=terrain,
        original_faces=faces,
        vertices=vertices,
        faces=indices,
        boundary_obligations=obligations,
    )
    if args.render:
        from pool_contact_view import display

        display(body, terrain, faces, vertices, indices, args.output)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
