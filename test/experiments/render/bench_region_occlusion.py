"""Fresh source-prefix consumer accepting no source geometry, recipes or images."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from region_occlusion_product import load_product, select


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
    product = load_product(args.product, request["observers"], request["provenance"])
    prepare_s = time.perf_counter() - start
    start = time.perf_counter()
    keep = select(product, request["observers"], request["requested"])
    selection_s = time.perf_counter() - start
    np.save(output / "keep.npy", keep)
    report = {
        "prepare_s": prepare_s,
        "selection_s": selection_s,
        "source_queries": len(keep),
        "retained": int(keep.sum()),
        "stored_array_bytes": product["header"]["stored_array_bytes"],
        "decoded_keep_bytes": product["keep"].nbytes,
        "returned_keep_bytes": keep.nbytes,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
        "scope": "fresh CPU source-prefix selection only; no original geometry/recipes, native IO savings, frame or device admission",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
