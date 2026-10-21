"""Fresh ready-product consumer; never reads original plans, forms or native exports."""

import argparse
import json
import resource
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from column_field_query import load_product, trace
from triangle_image_reference import image_rays


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--product", required=True, type=Path)
    parser.add_argument("--request", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    view = request["view"]
    view = (np.array(view[0]), np.array(view[1]), *view[2:])
    origins, directions = image_rays(
        view, np.array(request["roi"]), request["width"], request["height"], 2
    )
    tracemalloc.start()
    start = time.perf_counter()
    product = load_product(args.product)
    loaded = time.perf_counter() - start
    start = time.perf_counter()
    depth, normal, owners, work = trace(product, origins, directions)
    elapsed = time.perf_counter() - start
    peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    np.savez_compressed(
        args.output / "query.npz",
        candidate_depth=depth,
        candidate_normal=normal,
        candidate_interval=owners,
    )
    report = {
        "load_s": loaded,
        "query_s": elapsed,
        "rays": len(origins),
        "work": work,
        "stored_array_bytes": product["header"]["stored_array_bytes"],
        "derived_grid_index_bytes": product["derived_index_bytes"],
        "query_output_bytes": depth.nbytes + normal.nbytes + owners.nbytes,
        "traced_peak_bytes": peak,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
        "copied_bytes": None,
        "scope": "Python finished geometry consumer; traced memory excludes untracked GEOS/RTrees/driver memory; no GPU/device or complete world budget claim",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
