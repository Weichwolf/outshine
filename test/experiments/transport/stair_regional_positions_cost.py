"""Equal actual shared-boundary geometry; allocation profiles exclude source preload."""

import argparse
import hashlib
import resource
import statistics
import time
from pathlib import Path

from camera_height_inputs import digest
from stair_module_experiment import write_json
from stair_patch_inputs import fresh
from stair_regional_positions import encode_regions, reconstruct
from stair_regional_precision import position_payload
from stair_regional_snapshot import original
from stair_terrain_experiment import frozen


def measured(args, points, origin, anchors, peer_name, peer_origin, method):
    peer = position_payload(points[anchors], peer_origin)
    groups = [(anchors, peer, peer_origin)]
    times, previous, geometry_digest = [], None, None
    for _repeat in range(args.repeats):
        start = time.perf_counter()
        streams = encode_regions(points, origin, groups, method=method)
        times.append(time.perf_counter() - start)
        geometry_digest = hashlib.sha256(reconstruct(*streams).tobytes()).hexdigest()
        if previous is not None and geometry_digest != previous:
            raise ValueError("repeated actual regional geometry changed")
        previous = geometry_digest
    output_bytes = sum(array.nbytes for array in streams)
    report = {
        "seconds": times,
        "median_seconds": statistics.median(times),
        "actual_geometry_sha256": geometry_digest,
        "owned_output_array_bytes": output_bytes,
        "position_bytes": streams[0].nbytes,
        "origin_bytes": streams[1].nbytes,
        "origin_id_bytes": streams[2].nbytes,
    }
    if args.profile:
        import memray

        start = time.perf_counter()
        with memray.Tracker(
            str(args.output / (peer_name + "-" + method + ".bin")),
            trace_python_allocators=True,
            native_traces=False,
        ):
            actual = encode_regions(points, origin, groups, method=method)
        report["profiled_seconds"] = time.perf_counter() - start
        if (
            hashlib.sha256(reconstruct(*actual).tobytes()).hexdigest()
            != geometry_digest
        ):
            raise ValueError("profiled actual geometry differs")
    return report


def run(args):
    commit = frozen()
    fresh(args.output)
    if type(args.repeats) is not int or not 1 <= args.repeats <= 30:
        raise ValueError("bounded encoding repetitions required")
    points, faces, anchors, origin, peers = original(args.input)
    args.output.mkdir()
    cases = {}
    for name, peer_origin in peers.items():
        cases[name] = {
            method: measured(args, points, origin, anchors, name, peer_origin, method)
            for method in ("copy", "rebase_if_exact")
        }
        if len({case["actual_geometry_sha256"] for case in cases[name].values()}) != 1:
            raise ValueError("shared-boundary alternatives differ physically")
    report = {
        "commit": commit,
        "input_sha256": digest(args.input),
        "vertices": len(points),
        "faces": len(faces),
        "boundary_vertices": len(anchors),
        "cases": cases,
        "scope": "Only encoding and its validation timed/profiled; original data, boundary extraction and peer encoding preloaded. Decoding/hashing after each timer/tracker. Owned array sizes are not GPU stride or total copied bytes; no runtime/device/world budget proof.",
    }
    write_json(args.output / "comparison.json", report)
    return report


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    parser.add_argument("--profile", action="store_true")
    report = run(parser.parse_args())
    print(
        {
            name: {
                method: case["owned_output_array_bytes"]
                for method, case in variants.items()
            }
            for name, variants in report["cases"].items()
        }
    )
