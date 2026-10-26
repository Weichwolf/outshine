"""Fresh complete ready-consumer cost; no original source or recipe rebuild."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from attributed_prisms import load_product, render_product
from verify_prism_lighting import legacy_module


def run(args):
    load, query = load_product, render_product
    if args.legacy is not None:
        old = legacy_module(args.legacy)
        load, query = old.load_product, old.render_product
    if args.trace:
        tracemalloc.start()
    started = time.perf_counter()
    product = load(args.product)
    load_seconds = time.perf_counter() - started
    with np.load(args.rays, allow_pickle=False) as archive:
        origins, directions = archive["origins"], archive["directions"]
    started = time.perf_counter()
    result = query(product, origins, directions)
    query_seconds = time.perf_counter() - started
    current, peak = tracemalloc.get_traced_memory() if args.trace else (None, None)
    report = {
        "load_seconds": load_seconds,
        "query_seconds": query_seconds,
        "rays": len(origins),
        "hits": int(np.isfinite(result[0]).sum()),
        "work": result[-1],
        "process_raw_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "traced_current_bytes": current,
        "traced_peak_bytes": peak,
        "material_edge_rows": len(product[1].edge),
        "material_code_rows": len(product[1].codes),
        "primitive_bound_bytes": product[0].bounds.nbytes,
        "scope": "Complete ready load plus clip/fallback/fields. Normal-priority Python cost, not native/GPU or all allocator/copy-traffic measurement.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--product", type=Path, required=True)
    parser.add_argument("--rays", type=Path, required=True)
    parser.add_argument("--legacy", type=Path)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
