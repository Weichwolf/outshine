"""Scalar graph benchmark; original source scope remains unqualified."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from bridge_source_fact_inputs import load_bridge_source_facts
from canonical_port_native_inputs import FIXTURE, terrain_input
from graph_height_envelope_audit import all_queries, lazy_witness, plot
from graph_height_envelopes import envelope, lp_reference, query
from outshine_ref.transport.canonical_ports import PortBudget
from outshine_ref.transport.canonical_ports._geodesy import positions_in_frame
from outshine_ref.transport.canonical_ports._ground_scope_index import (
    prepare_ground_scope_index,
)
from outshine_ref.transport.canonical_ports._source import component

INPUT = Path(__file__).parent / "fixtures/bridge-source-facts"

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("--plot", action="store_true")
args = parser.parse_args()
out = args.output
out.mkdir(parents=True, exist_ok=False)
s, g, w, b, _ = load_bridge_source_facts(INPUT)
idx, reason, index_work = prepare_ground_scope_index(
    s, g, w, source_binding=s.binding, candidate_binding=b
)
assert not reason
selected, closure, source_work = component(
    idx.index, (4975830414568911138, 0), idx.unknown, PortBudget()
)
nodes, spans, _ = idx.index
ids = sorted({n for e in selected for n in spans[e].nodes})
lookup = {n: i for i, n in enumerate(ids)}
manifest = json.loads((INPUT / "manifest.json").read_text())
terrain, terrain_binding = terrain_input(
    FIXTURE, {"terrain": manifest["terrain_reference"]}
)
geographic = np.array([nodes[n] for n in ids])
h = np.full(len(ids), np.nan)
missing_dem = []
for j, point in enumerate(geographic):
    try:
        h[j] = terrain(np.array([point]))[0]
    except ValueError as error:
        missing_dem.append({"node": ids[j], "reason": str(error)})
known = np.isfinite(h)
anchor = nodes[12915210334868635654]
xyz = positions_in_frame(anchor, geographic, np.zeros(len(ids)))
z = np.full(len(ids), np.nan)
z[known] = positions_in_frame(anchor, geographic[known], h[known])[:, 2]
xyz[:, 2] = z
assert known.any()
usable = [e for e in selected if spans[e].maximum_gradient is not None]
excluded = [
    {"edge": e, "reason": "missing_original_MaxGradient"}
    for e in selected
    if spans[e].maximum_gradient is None
]
edges = np.array([[lookup[n] for n in spans[e].nodes] for e in usable])
weights = np.array([spans[e].maximum_gradient for e in usable]) * np.linalg.norm(
    xyz[edges[:, 0], :2] - xyz[edges[:, 1], :2], axis=1
)
assert np.all(weights > 0)
graph = [[] for _ in ids]
for (a, b), weight in zip(edges, weights):
    graph[a].append((int(b), float(weight)))
    graph[b].append((int(a), float(weight)))
start = time.perf_counter()
upper, upwork = envelope(graph, z)
negative, lowwork = envelope(graph, -z)
lower = -negative
resolved = np.isfinite(upper) & np.isfinite(lower)
rho = float(max(0.0, np.max((lower - upper)[resolved])) / 2)
heights = np.full(len(z), np.nan)
heights[resolved] = (upper[resolved] + lower[resolved]) / 2
envelope_ms = 1000 * (time.perf_counter() - start)
guard = (
    16
    * np.finfo(float).eps
    * len(z)
    * max(1.0, abs(z[known]).max(), len(z) * weights.max())
)
assert np.max(abs(heights[known] - z[known])) <= rho + guard
finite_edges = resolved[edges].all(axis=1)
assert (
    np.max(
        abs(heights[edges[finite_edges, 0]] - heights[edges[finite_edges, 1]])
        - weights[finite_edges]
    )
    <= guard
)
solution, lp = lp_reference(z, edges, weights)
assert abs(solution[-1] - rho) <= guard
queries = []
observed = np.flatnonzero(known)
for origin in (
    lookup[12915210334868635654],
    int(observed[0]),
    int(observed[len(observed) // 2]),
    int(observed[-1]),
):
    start = time.perf_counter()
    lo, hi, work = query(
        graph,
        z,
        origin,
        float(z[known].min()),
        float(z[known].max()),
        guard,
        10 * len(edges),
    )
    elapsed_ms = 1000 * (time.perf_counter() - start)
    assert (
        work["complete"]
        and abs(lo - lower[origin]) <= guard
        and abs(hi - upper[origin]) <= guard
    )
    queries.append(
        {
            "node": ids[origin],
            "lower_m": lo,
            "upper_m": hi,
            "elapsed_ms": elapsed_ms,
            "work": work,
        }
    )
_, _, incomplete = query(
    graph,
    z,
    lookup[12915210334868635654],
    float(z[known].min()),
    float(z[known].max()),
    guard,
    1,
)
assert not incomplete["complete"]
audit = all_queries(graph, z, lower, upper, guard)


def sample(node):
    try:
        height = terrain(geographic[[node]])
    except ValueError:
        return None
    return float(positions_in_frame(anchor, geographic[[node]], height)[0, 2])


witness = lazy_witness(
    graph,
    sample,
    lookup[12915210334868635654],
    0.5,
    max_samples=len(z),
    max_visits=10 * len(edges),
)
assert witness["state"] == "infeasible" and witness["sample_attempts"] == 2
for observation in witness["observations"]:
    observation["source_node_id"] = ids[observation["node"]]
if args.plot:
    plot(out, xyz, z, audit, witness)
report = {
    "scope": "Scalar fixed-XY graph model only: original captured component remains source-unqualified (coverage/Reason5/frontiers), three missing profiles explicitly excluded. No C1/cross-section/finite contact/bridge/World qualification. Global DEM extrema for query stopping were obtained from the fully loaded benchmark; no avoided IO is claimed.",
    "runtime_commit": subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=Path(__file__).parent, text=True
    ).strip(),
    "source_binding": s.binding,
    "candidate_binding": b,
    "terrain_binding": terrain_binding,
    "source_component_spans": len(selected),
    "source_nodes": len(ids),
    "model_edges": len(edges),
    "excluded_sources": excluded,
    "source_closure": closure,
    "source_work": source_work,
    "source_anchor": anchor,
    "height_model": "Original zero-height geographic positions in anchor WGS84 ENU give fixed XY; finite DEM observations give scalar ENU z; missing DEM is no observation, |za-zb| <= SourceMaxGradient * fixed-XY-edge-length.",
    "rho_m": rho,
    "requested_prior_limit_m": 0.5,
    "prior_limit_feasible": rho <= 0.5,
    "envelope_ms": envelope_ms,
    "upper_work": upwork,
    "lower_work": lowwork,
    "lp": lp,
    "lp_rho_m": float(solution[-1]),
    "floating_guard_m": guard,
    "queries": queries,
    "all_query_audit": audit,
    "lazy_local_witness": witness,
    "incomplete_budget_control": incomplete,
    "all_dem_observations": int(known.sum()),
    "missing_dem": missing_dem,
    "unresolved_model_nodes": [ids[j] for j in np.flatnonzero(~resolved)],
    "minimum_source_height_m": float(z[known].min()),
    "maximum_source_height_m": float(z[known].max()),
}


def serial(x):
    if hasattr(x, "items"):
        return dict(x)
    if hasattr(x, "tolist"):
        return x.tolist()
    raise TypeError(type(x).__name__)


(out / "report.json").write_text(json.dumps(report, indent=2, default=serial) + "\n")
np.savez_compressed(
    out / "model.npz",
    node_ids=np.array(ids, dtype=np.uint64),
    positions=xyz,
    edges=edges,
    span_ids=np.array(usable, dtype=np.uint64),
    weights=weights,
    upper=upper,
    lower=lower,
    heights=heights,
    lp_heights=solution[:-1],
)
print(
    json.dumps(
        {
            k: report[k]
            for k in (
                "source_nodes",
                "model_edges",
                "rho_m",
                "prior_limit_feasible",
                "envelope_ms",
                "upper_work",
                "lower_work",
                "lp",
                "lp_rho_m",
                "floating_guard_m",
                "queries",
            )
        },
        indent=2,
    )
)
