"""Fresh-process field-cache consumer; no original geometry/recipe input is accepted."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from layered_tree_plan import select
from layered_tree_render import load_cut, load_header, query
from triangle_image_reference import image_rays


def main():
    parser = argparse.ArgumentParser(__doc__)
    for name in ("product", "request", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    metadata = json.loads((args.product / "cook.json").read_text())
    if metadata["provenance"] != request["provenance"]:
        raise ValueError("source identity mismatch")
    start = time.perf_counter()
    tree, header = load_header(args.product)
    nodes, missing, tests = select(
        tree, np.array(request["observer_bounds"]), request["profile"]
    )
    selection_s = time.perf_counter() - start
    if len(missing):
        raise ValueError("finer or original products required")
    start = time.perf_counter()
    cut = load_cut(args.product, tree, nodes)
    prepare_s = time.perf_counter() - start
    eye, basis, focal, width, height = request["view"]
    view = (np.array(eye), np.array(basis), focal, width, height)
    roi = np.array(request["roi"])
    w, h = (roi[1] - roi[0]).astype(int)
    origins, directions = image_rays(view, roi, w, h, 2)
    start = time.perf_counter()
    depth, fields, source, tag = query(cut, origins, directions)
    query_s = time.perf_counter() - start
    np.savez_compressed(
        args.output / "fields.npz",
        depth=depth,
        source=source,
        original_face_tag=tag,
        **fields,
    )
    result = {
        "selection_wall_s": selection_s,
        "cold_process_cut_prepare_s": prepare_s,
        "warm_held_query_s": query_s,
        "header_array_bytes": sum(a.nbytes for a in header.values()),
        "selection_node_tests": tests,
        "cost": cut["cost"],
        "rays": len(depth),
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "fresh process, cache-hit field assets; OS file cache not flushed; CPU proxy/Rtree, no original geometry input or device budget proof",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
