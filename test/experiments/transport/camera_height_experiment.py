"""Fresh bounded workers for four pinned bed-height comparisons (#265)."""

import argparse
import json
import resource
import subprocess
from pathlib import Path

import numpy as np
from camera_height_inputs import digest, prepare_case
from camera_height_solve import audit, fit
from outshine_ref.transport import heights as height_library

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PLACES = ("Wien", "Tokyo", "CentralPark", "Feldkirch")


def provenance():
    return {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            ).strip()
        ),
        "implementations": {p.name: digest(p) for p in HERE.glob("camera_height_*.py")},
        "shared_implementation_sha256": {
            "transport/heights.py": digest(height_library.__file__),
            "surface_height_frames.py": digest(HERE / "surface_height_frames.py"),
            "capture_height_targets.py": digest(HERE / "capture_height_targets.py"),
        },
    }


def prepare(args):
    rows = []
    for name in PLACES:
        rows.append(
            prepare_case(
                args.camera_products,
                args.rail_products,
                name,
                args.dem_sources,
                args.output,
            )
        )
        print(json.dumps(rows[-1]), flush=True)
    report = dict(
        **provenance(),
        cases=rows,
        scope="Frozen existing whole camera meshes, coarse retained z11 DEM; actual complete hardware footprints fix all intersecting bed facets. Original XY/topology/ledgers unchanged. No acquisition or native proof.",
    )
    (args.output / "preparation.json").write_text(json.dumps(report, indent=2) + "\n")


def worker(args):
    name, mode = args.place, args.mode
    metadata = json.loads((args.inputs / (name + "-input.json")).read_text())
    path = args.inputs / (name + "-input.npz")
    if digest(path) != metadata["input_sha256"]:
        raise ValueError("prepared comparison input changed")
    with np.load(path) as data:
        arrays = {k: data[k] for k in data.files}
    heights, report = fit(arrays, metadata["fixed_datum_m"], mode, args.seconds)
    flat = np.full(len(arrays["xy"]), metadata["fixed_datum_m"])
    report["flat_baseline"] = audit(
        *(arrays[k] for k in ("xy", "faces", "targets", "frames")),
        flat,
        arrays["fixed"],
        metadata["fixed_datum_m"],
        arrays["weights"],
    )
    report.update(
        **provenance(),
        place=name,
        vertices=len(arrays["xy"]),
        faces=len(arrays["faces"]),
        fixed_vertices=len(arrays["fixed"]),
        input_array_payload_bytes=sum(a.nbytes for a in arrays.values()),
        input_sha256=digest(path),
        peak_worker_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        scope="One whole camera-window bed, same anisotropic face/contact constraints, no topology/source or rail changes. Whole-face replay required; solver completion alone is not acceptance.",
    )
    if heights is not None:
        path = args.output / (name + "-" + mode + ".npz")
        np.savez_compressed(
            path, xy=arrays["xy"], faces=arrays["faces"], heights=heights
        )
        report["mesh_sha256"] = digest(path)
    (args.output / (name + "-" + mode + ".json")).write_text(
        json.dumps(report, indent=2) + "\n"
    )
    print(json.dumps(report), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", action="store_true")
    parser.add_argument("--camera-products", type=Path)
    parser.add_argument("--rail-products", type=Path)
    parser.add_argument("--dem-sources", type=Path)
    parser.add_argument("--inputs", type=Path)
    parser.add_argument("--place", choices=PLACES)
    parser.add_argument("--mode", choices=("minimax", "least_squares"))
    parser.add_argument("--seconds", type=float, default=8)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not np.isfinite(args.seconds) or not 0 < args.seconds <= 8:
        parser.error("solver seconds must be finite in (0,8]")
    required = (
        (args.camera_products, args.rail_products, args.dem_sources)
        if args.prepare
        else (args.inputs, args.place, args.mode)
    )
    if any(v is None for v in required) or args.output.exists():
        parser.error("explicit inputs and fresh external output directory required")
    args.output.mkdir(parents=True)
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (60, 60))
    (prepare if args.prepare else worker)(args)


if __name__ == "__main__":
    main()
