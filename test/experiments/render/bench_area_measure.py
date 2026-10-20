"""Fresh CPU consumer of an anchored measure, without original scene/form reads."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from area_measure_render import evaluate, program
from viewcell_product import require_domain
from visible_normal_material import tables


def load_product(path, provenance, observers):
    path = Path(path)
    header = json.loads((path / "header.json").read_text())
    if (
        header["format"] != "adaptive-opaque-normal-measure-1"
        or header["provenance"] != provenance
    ):
        raise ValueError("area measure identity/format mismatch")
    require_domain(header["observer_bounds"], observers)
    with np.load(path / "measure.npz", allow_pickle=False) as archive:
        arrays = {k: archive[k] for k in archive.files}
    return arrays, header


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
    view = tuple(np.asarray(v) if i < 2 else v for i, v in enumerate(request["view"]))
    start = time.perf_counter()
    arrays, header = load_product(args.product, request["provenance"], [view[0]])
    load_s = time.perf_counter() - start
    box = np.array(request["box"])
    start = time.perf_counter()
    p = program(arrays, arrays, view, box)
    prepare_s = time.perf_counter() - start
    start = time.perf_counter()
    rgb, coverage = evaluate(
        p, box[1] - box[0], request["light"], request["roughness"], tables()
    )
    shade_s = time.perf_counter() - start
    np.savez_compressed(
        output / "images.npz", candidate=rgb, candidate_coverage=coverage
    )
    report = {
        "load_s": load_s,
        "program_s": prepare_s,
        "shade_s": shade_s,
        "stored_array_bytes": header["stored_array_bytes"],
        "program_array_bytes": sum(
            v.nbytes for v in p.values() if isinstance(v, np.ndarray)
        ),
        "work": p["work"],
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "allocations": None,
        "scope": "one fresh CPU filtered GGX query, no native timing, geometry/form access or device admission",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
