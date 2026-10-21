"""Fresh-process camera-support asset consumer; no original source input accepted."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from triangle_image_reference import image_rays
from viewcell_product import load_product, query


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument(
        "--product-kind", choices=("viewcell", "beam"), default="viewcell"
    )
    for name in ("product", "request", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    request = json.loads(args.request.read_text())
    eye, basis, focal, width, height = request["view"]
    view = np.array(eye), np.array(basis), focal, width, height
    start = time.perf_counter()
    if args.product_kind == "beam":
        from beam_field_product import load_product as load_beam

        product = load_beam(args.product, [eye], request["provenance"])
    else:
        product = load_product(args.product, [eye], request["provenance"])
    prepare_s = time.perf_counter() - start
    roi = np.array(request["roi"])
    w, h = (roi[1] - roi[0]).astype(int)
    origins, directions = image_rays(view, roi, w, h, 2)
    start = time.perf_counter()
    if args.product_kind == "beam":
        from beam_field_product import query as query_beam

        depth, fields, source, tag = query_beam(product, origins, directions)
    else:
        depth, fields, source, tag = query(product, origins, directions)
    query_s = time.perf_counter() - start
    np.savez_compressed(
        args.output / "fields.npz",
        depth=depth,
        source=source,
        original_face_tag=tag,
        **fields,
    )
    result = {
        "cold_process_product_prepare_s": prepare_s,
        "held_query_s": query_s,
        "rays": len(depth),
        "stored_array_bytes": product["stored_array_bytes"],
        "decoded_normal_bytes": product["decoded_normal_bytes"],
        "vertex_stage_copy_bytes": product["vertex_stage_copy_bytes"],
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "fresh CPU ready-product process; OS cache not flushed; no original input, native or device evidence",
    }
    if args.product_kind == "beam":
        result.update(
            derived_cpu_bytes=product["derived_cpu_bytes"], query_work=product["work"]
        )
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
