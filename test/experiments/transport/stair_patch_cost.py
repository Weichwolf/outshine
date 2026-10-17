"""Equal-surface prepared resident load and query costs, separate from generation."""

import argparse
import contextlib
import hashlib
import json
import resource
import time
from pathlib import Path

from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches
from stair_module_experiment import write_json
from stair_module_inputs import read_arrays
from stair_patch_inputs import frame_for, fresh, load_prepared
from stair_terrain_experiment import frozen


def loaded(args, manifest):
    if args.representation == "patches":
        return load_prepared(args.prepared)[0]
    path = args.prepared / "flat-reference.npz"
    if digest(path) != manifest["flat_reference_sha256"]:
        raise ValueError("flat equal-surface reference changed")
    saved = read_arrays(path)
    patch = SurfacePatch(
        "flat-reference",
        frame_for(manifest["origin_lat_lon"]),
        "terrain_top",
        Mesh(saved["vertices"], saved["faces"]),
        {k: saved[k] for k in ("ground", "constructed")},
    )
    return SurfacePatches(patch)


def work(args, manifest, points):
    began = time.perf_counter()
    view = loaded(args, manifest)
    load_seconds = time.perf_counter() - began
    result_hash = hashlib.sha256()
    counts = {
        "face_tests": 0,
        "patch_bounds_tests": 0,
        "returned_faces": 0,
        "raw_triangle_xyz_bytes": 0,
        "canonical_face_label_bytes": 0,
        "owned_mesh_bytes": 0,
    }
    for point in points:
        result = view.query([*(point - 0.5), *(point + 0.5)], method=args.method)
        for key in counts:
            counts[key] += result.report.get(key, 0)
        if result.mesh is None:
            if result.reason != "outside_surface":
                raise ValueError("cost query has no equal-quality complete surface")
            result_hash.update(result.reason.encode())
        else:
            sample = SurfaceSampler(result.mesh).sample([point])
            result_hash.update(
                sample.points_enu_m.tobytes() + sample.normals_enu.tobytes()
            )
            result_hash.update(str(sample.reasons).encode())
    return {
        "load_index_validation_seconds": load_seconds,
        "total_seconds": time.perf_counter() - began,
        "queries": len(points),
        "result_sha256": result_hash.hexdigest(),
        "work": counts,
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    manifest = json.loads((args.prepared / "patches.json").read_text())
    if digest(args.prepared / "query-cases.npz") != manifest["query_cases_sha256"]:
        raise ValueError("fixed real query inputs changed")
    points = read_arrays(args.prepared / "query-cases.npz")["xy"]
    tracker = contextlib.nullcontext()
    if args.profile:
        import memray

        tracker = memray.Tracker(
            str(args.profile), trace_python_allocators=True, native_traces=False
        )
    with tracker:
        report = work(args, manifest, points)
    report.update(
        commit=commit,
        representation=args.representation,
        method=args.method,
        peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        scope="Prepared resident payload load, index/invariant validation and same fixed local queries/samples after imports. No provider/CDT/solver/whole-body generation or rendering; native/Python allocation events, no native call stacks. This is not native cache/streaming/LOD acceptance.",
    )
    args.output.mkdir()
    write_json(args.output / "cost.json", report)
    print(json.dumps(report), flush=True)
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepared", type=Path, required=True)
    parser.add_argument(
        "--representation", choices=("patches", "flat"), default="patches"
    )
    parser.add_argument("--method", choices=("index", "scan"), default="index")
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
