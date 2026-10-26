"""Optional isolated allocation/cost probes on saved shared or dense height inputs."""

import argparse
import contextlib
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from capture_height_targets import sample_targets
from outshine_ref.contracts import Mesh
from transport_field_plan import bounds_for, solve_input
from wien_network_sources import scene


def prepare_refinement(root, reference, dem, output):
    if output.exists():
        raise ValueError("fresh refinement input required")
    output.parent.mkdir(parents=True, exist_ok=True)
    report = json.loads((root / "comparison.json").read_text())
    if report["dirty"] or digest(root / "ground.npz") != report["ground_sha256"]:
        raise ValueError("clean independently replayed geometry required")
    with np.load(root / "ground.npz") as data:
        mesh, road = Mesh(data["vertices"], data["faces"]), data["road"]
    fixture, previous, model, _note = scene(reference)
    xy, faces = mesh.vertices[:, :2], mesh.triangles
    frames = np.broadcast_to(np.eye(2) / (2 / 3), (len(faces), 2, 2)).copy()
    frames[road] = np.eye(2) / 0.025
    bounds, notes = bounds_for(
        xy, faces, faces[road], previous[0], model["roof"], previous[1]
    )
    targets, receipts = sample_targets(
        fixture["origin_lat_lon"], xy, dem, 11, fetch=False
    )
    np.savez_compressed(
        output,
        xy=xy,
        faces=faces,
        frames=frames,
        targets=targets,
        bound_columns=bounds[0],
        bound_coefficients=bounds[1],
        bound_limits=bounds[2],
    )
    output.with_suffix(".json").write_text(
        json.dumps(
            {
                "ground_sha256": report["ground_sha256"],
                "input_sha256": digest(output),
                "bounds": notes,
                "geometry_report_sha256": digest(root / "comparison.json"),
                "dem_receipts": receipts,
                "scope": "Dense independent vertex variables on the exact same accepted fine mesh. The existing shared-height proposal remains the valid fallback.",
            },
            indent=2,
        )
        + "\n"
    )


def measure(source, output, allocations):
    if output.exists() and any(output.iterdir()):
        raise ValueError("fresh cost output required")
    output.mkdir(parents=True, exist_ok=True)
    with np.load(source) as saved:
        arrays = dict(saved)
    tracker = contextlib.nullcontext()
    if allocations:
        import memray

        tracker = memray.Tracker(
            str(output / "allocations.bin"), trace_python_allocators=True
        )
    began = time.perf_counter()
    with tracker:
        candidate, report = solve_input(arrays)
    result = {
        "input_sha256": digest(source),
        "fit": report,
        "seconds": time.perf_counter() - began,
        "instrumented": allocations,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scope": "One isolated cold fit. Instrumented wall times include tracking; compare work/allocations separately from uninstrumented solver timing.",
    }
    if candidate is not None:
        np.savez_compressed(
            output / "candidate.npz",
            vertices=candidate.vertices,
            faces=candidate.triangles,
        )
        result["candidate_sha256"] = digest(output / "candidate.npz")
    (output / "measurement.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("prepare", "measure"))
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference-root", type=Path)
    parser.add_argument("--dem-sources", type=Path)
    parser.add_argument("--allocations", action="store_true")
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3, 3 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (30, 30))
    if args.mode == "prepare":
        if args.reference_root is None or args.dem_sources is None:
            parser.error("explicit reference and cached DEM paths required")
        prepare_refinement(
            args.input, args.reference_root, args.dem_sources, args.output
        )
    else:
        measure(args.input, args.output, args.allocations)
