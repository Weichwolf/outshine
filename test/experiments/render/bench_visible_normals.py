"""Fresh ready-product consumer accepting no original source or geometry input."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from triangle_image_reference import image_rays
from visible_normal_material import tables
from visible_normal_query import load_product, query


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("product", "request", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs in system temp")
    output.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    eye, basis, focal, width, height = request["view"]
    view = np.array(eye), np.array(basis), focal, width, height
    box = np.array(request["roi"])
    shape = (box[1] - box[0]).astype(int)
    start = time.perf_counter()
    product = load_product(args.product, [eye], request["provenance"])
    energy = tables()
    prepare_s = time.perf_counter() - start
    origins, directions = image_rays(view, box, *shape, 4)
    start = time.perf_counter()
    result = query(
        product, origins, directions, request["light"], request["roughness"], energy
    )
    query_s = time.perf_counter() - start
    arrays = {k: a for k, a in result.items() if isinstance(a, np.ndarray)}
    np.savez_compressed(output / "fields.npz", **arrays)
    report = {
        "prepare_s": prepare_s,
        "query_s": query_s,
        "stored_array_bytes": product["header"]["stored_array_bytes"],
        "derived_cpu_bytes": product["derived_cpu_bytes"],
        "returned_array_bytes": sum(a.nbytes for a in arrays.values()),
        "request_ray_bytes": origins.nbytes + directions.nbytes,
        "work": product["work"],
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
        "scope": "fresh CPU finished-product consumer; OS cache not flushed; no original geometry, native/device/frame evidence",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
