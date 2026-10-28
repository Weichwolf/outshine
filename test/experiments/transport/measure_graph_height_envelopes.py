import argparse
import json
import time
from pathlib import Path

import memray
import numpy as np
from graph_height_envelopes import envelope, lp_reference

parser = argparse.ArgumentParser(
    description="Measure scalar graph-envelope/LP stages on a replay model"
)
parser.add_argument("--input", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=False)
a = np.load(args.input / "model.npz")
z = a["positions"][:, 2]
graph = [[] for _ in z]
for (i, j), w in zip(a["edges"], a["weights"]):
    graph[i].append((int(j), float(w)))
    graph[j].append((int(i), float(w)))


def full():
    u, us = envelope(graph, z)
    negative, ls = envelope(graph, -z)
    l = -negative
    rho = float(max(0.0, (l - u).max())) / 2
    return (u + l) / 2, rho, {"upper": us, "lower": ls}


def lp():
    x, stats = lp_reference(z, a["edges"], a["weights"])
    return x[:-1], float(x[-1]), stats


records = {}
for name, fn in (("envelopes", full), ("lp", lp)):
    start = time.perf_counter()
    h, rho, work = fn()
    ms = 1000 * (time.perf_counter() - start)
    assert np.max(abs(h[np.isfinite(z)] - z[np.isfinite(z)])) <= rho + 1e-7
    assert np.max(abs(h[a["edges"][:, 0]] - h[a["edges"][:, 1]]) - a["weights"]) <= 1e-7
    trace = out / (name + ".bin")
    with memray.Tracker(str(trace), trace_python_allocators=True):
        measured = fn()
    reader = memray.FileReader(str(trace))
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.MUNMAP,
        memray.AllocatorType.PYMALLOC_FREE,
    }
    requests = bytes_ = 0
    for r in reader.get_allocation_records():
        if r.allocator not in frees:
            requests += r.n_allocations
            bytes_ += r.size
    records[name] = {
        "total_ms": ms,
        "rho_m": rho,
        "work": work,
        "allocation_requests": requests,
        "cumulative_requested_bytes": bytes_,
        "tracked_peak_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
    }
report = {
    "scope": "Single shared-host diagnostic calls. Equal prepared graph/observations, all solver imports already loaded; envelopes include two traversals/midpoint; LP includes sparse assembly plus solve. Decoding/graph/adjacency preparation and independent validation are excluded from both. Allocation bytes are not total copied bytes or process/native/device peaks.",
    "nodes": len(z),
    "edges": len(a["edges"]),
    "observations": int(np.isfinite(z).sum()),
    "methods": records,
    "known_numeric_output_bytes": {"envelopes": 3 * len(z) * 8, "lp": (len(z) + 1) * 8},
    "unmeasured": "All temporary copies, total Python/native process memory and stable repeated/device timings.",
}
(out / "costs.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
