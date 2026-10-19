"""Compare shared and cache-cell-isolated far cuts using original source plans."""

import argparse
import collections
import hashlib
import json
import platform
import resource
import tempfile
import tracemalloc
from pathlib import Path

import numpy as np
from far_product_cut import FarProductHierarchy, box_triangles


def inputs(path):
    raw = path.read_bytes()
    data = json.loads(raw)
    sources = data["sources"]
    bounds = np.full((len(sources), 2, 3), np.nan)
    eligible = np.zeros(len(sources), bool)
    for index, source in enumerate(sources):
        if source["index"] != index:
            raise ValueError("original source order required")
        if source["bounds"] is not None:
            bounds[index] = [source["bounds"]["min"], source["bounds"]["max"]]
        surface = source["surface"]
        eligible[index] = bool(
            surface
            and surface["shapes"]
            and surface["minimum_height_m"] == 0
            and all(not shape["holes"] for shape in surface["shapes"])
        )
    known = np.isfinite(bounds).all(axis=(1, 2))
    eligible &= known
    return data, bounds, known, eligible, hashlib.sha256(raw).hexdigest()


def memory(hierarchy):
    return sum(
        a.nbytes
        for a in (
            hierarchy.tree.bounds,
            hierarchy.tree.nodes,
            hierarchy.tree.order,
            hierarchy.tree.position,
            hierarchy.ineligible,
            hierarchy.unknown,
        )
    )


def query(trees, count, observer, profile):
    represented = []
    work = collections.Counter()
    proxies = []
    remaining = 100000
    for addresses, hierarchy in trees:
        cut = hierarchy.select(observer, node_budget=remaining, **profile)
        work.update(cut.work)
        remaining -= cut.work["node_tests"]
        represented.extend(addresses[cut.unknown])
        for node, proxy in cut.nodes:
            members = addresses[hierarchy.members(node)]
            represented.extend(members)
            if proxy:
                box = hierarchy.tree.nodes[node]["bounds"]
                triangles = box_triangles(box)
                if not ((triangles >= box[0]) & (triangles <= box[1])).all():
                    raise ValueError("proxy escapes its enclosure")
                work.update(
                    generated_proxy_triangles=len(triangles),
                    generated_triangle_bytes=triangles.nbytes,
                )
                proxies.append({"members": members.tolist(), "bounds": box.tolist()})
    if not np.array_equal(np.sort(represented), np.arange(count)):
        raise ValueError("lost or repeated original source address")
    work["verified_original_source_addresses"] = count
    return {"work": dict(work), "proxies": proxies}


def compare(path):
    data, bounds, known, eligible, digest = inputs(path)
    n = len(bounds)
    shared = FarProductHierarchy(bounds, known=known, proxy_eligible=eligible)
    cells = np.array([source["cell"] for source in data["sources"]])
    isolated = []
    for cell in np.unique(cells):
        addresses = np.flatnonzero(cells == cell)
        isolated.append(
            (
                addresses,
                FarProductHierarchy(
                    bounds[addresses],
                    known=known[addresses],
                    proxy_eligible=eligible[addresses],
                ),
            )
        )
    anchor = np.array(data["anchor_ecef"])
    up = anchor / np.linalg.norm(anchor)
    eye = np.array(data["view"]["eye_ecef"]) - anchor
    profile = {
        "focal_px": data["view"]["focal_px"],
        "width": 1280,
        "height": 720,
        "error_px": 1.0,
    }
    cases = []
    for offset in (0, 10000, 50000, 100000):
        observer = np.array([eye + offset * up - 64, eye + offset * up + 64])
        cases.append(
            {
                "eye_altitude_offset_m": offset,
                "observer_box": observer.tolist(),
                "shared": query([(np.arange(n), shared)], n, observer, profile),
                "cells": query(isolated, n, observer, profile),
            }
        )
    keys = [(s["index"], s["id"], s["kind"], s["cell"]) for s in data["sources"]]
    return {
        "parent": data["parent"],
        "input": str(path),
        "sha256": digest,
        "sources": n,
        "known_bounds": int(known.sum()),
        "geometrically_eligible": int(eligible.sum()),
        "source_catalog_sha256": hashlib.sha256(json.dumps(keys).encode()).hexdigest(),
        "profile": profile,
        "observer_box_half_extent_m": 64,
        "shared_array_bytes": memory(shared),
        "cell_array_bytes": sum(memory(h) for _, h in isolated),
        "cases": cases,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--surfaces", type=Path, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    target = args.output.resolve()
    temporary = Path(tempfile.gettempdir()).resolve()
    if target == temporary or not target.is_relative_to(temporary) or target.exists():
        parser.error("fresh explicit system-temp output required")
    target.mkdir(parents=True)
    tracemalloc.start()
    results = [compare(path) for path in args.surfaces]
    _current, peak = tracemalloc.get_traced_memory()
    snapshot = tracemalloc.take_snapshot()
    tracemalloc.stop()
    report = {
        "results": results,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "traced_heap_peak_bytes": peak,
        "traced_live_allocations": sum(
            s.count for s in snapshot.statistics("traceback")
        ),
        "process_peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_name("far_product_cut.py"),
                Path(__file__).with_name("facing_intervals.py"),
            )
        },
        "scope": "Geometric box-candidate ROI only. Complete original source addresses before cell emission, no source ID replacement. Source envelopes are not opaque coverage. No source payload/decode avoided by this reader; no rendered/native product/material/topology/image/VS/device acceptance. Original plans remain fallback; high-altitude offsets are declared additional poses, not a changed ground gate. Traced live allocations are not cumulative allocation events. RSS includes Python, JSON, both candidate hierarchies and diagnostic proxy arrays.",
    }
    (target / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            [
                {
                    "parent": r["parent"],
                    "cases": [
                        {
                            "offset": c["eye_altitude_offset_m"],
                            "shared": c["shared"]["work"],
                            "cells": c["cells"]["work"],
                        }
                        for c in r["cases"]
                    ],
                }
                for r in results
            ]
        ),
        flush=True,
    )
