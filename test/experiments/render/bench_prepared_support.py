"""Fresh metadata consumer: no parent archive, original meshes or forms are read."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
import shapely
from prepared_source_support import load_product
from prepared_support_visibility import select_sources


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("product", "request", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    start = time.perf_counter()
    product = load_product(args.product, request["support_identity"])
    projection_bytes = (args.request.parent / "projection.wkb").read_bytes()
    request["fused"] = shapely.from_wkb(projection_bytes)
    load_s = time.perf_counter() - start
    start = time.perf_counter()
    hidden, available, work = select_sources(product, request["requested"], request)
    select_s = time.perf_counter() - start
    np.save(output / "hidden.npy", hidden)
    np.save(output / "available.npy", available)
    report = {
        "load_s": load_s,
        "selection_s": select_s,
        "work": work,
        "hidden_sources": int(hidden.sum()),
        "stored_array_bytes": product["header"]["stored_array_bytes"],
        "prefix_container_bytes": product["header"]["container_bytes"],
        "occluder_projection_wkb_bytes": len(projection_bytes),
        "returned_array_bytes": hidden.nbytes + available.nbytes,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
        "scope": "all-parent prefix plus supplied 2D opaque projection; no blocker construction, native IO/frame/device admission",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
