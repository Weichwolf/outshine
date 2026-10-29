"""Equal-product rail construction costs; source preparation and rendering separate."""

import argparse
import gc
import hashlib
import json
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from rail_ground_bed import construct
from rail_ground_inputs import load
from rail_ground_profile import bind_profile
from replay_canonical_bridge_ports import allocations
from replay_rail_ground_bed import arrays


def fingerprint(result):
    digest = hashlib.sha256()
    for name, value in sorted(arrays(result).items()):
        digest.update(name.encode())
        digest.update(str(value.dtype).encode())
        digest.update(str(value.shape).encode())
        digest.update(value.tobytes())
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    producer = Path(__file__).parent
    names = [
        "rail_ground_bed.py",
        "rail_ground_profile.py",
        "rail_ground_inputs.py",
        "coincident_ground_sources.py",
        "coincident_ground_geometry.py",
        "constructive_earthwork_plane.py",
        "measure_rail_ground_bed.py",
        "mvt_source_profile_inputs.py",
        "replay_rail_ground_bed.py",
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *[str(producer / n) for n in names]],
        text=True,
    ):
        raise ValueError("Freeze the rail construction producer before recording costs")
    args.output.mkdir(parents=True, exist_ok=False)
    began = time.process_time()
    index, cases, terrain, budget, metadata, raw, manifest = load()
    case = next(c for c in cases if len(c["selected"]) == 7)
    profile = bind_profile(index, case["selected"], metadata["source_binding"], raw)
    prepare_cpu = time.process_time() - began
    began, wall = time.process_time(), time.perf_counter()
    cold = construct(index, profile, terrain, budget)
    cold_cost = {
        "cpu_s": time.process_time() - began,
        "wall_s": time.perf_counter() - wall,
    }
    expected = fingerprint(cold)
    methods = {"shared": {"rounds": []}, "per_source": {"rounds": []}}
    del cold
    for repetition in range(7):
        order = (True, False) if repetition % 2 == 0 else (False, True)
        for shared in order:
            gc.collect()
            began, wall = time.process_time(), time.perf_counter()
            result = construct(index, profile, terrain, budget, shared=shared)
            timing = {
                "cpu_s": time.process_time() - began,
                "wall_s": time.perf_counter() - wall,
            }
            if fingerprint(result) != expected:
                raise ValueError("Repeated or alternative complete product changed")
            methods["shared" if shared else "per_source"]["rounds"].append(timing)
            del result
    import memray

    for shared, name in ((True, "shared"), (False, "per_source")):
        gc.collect()
        trace = args.output / (name + ".bin")
        with memray.Tracker(
            str(trace), trace_python_allocators=True, native_traces=False
        ):
            result = construct(index, profile, terrain, budget, shared=shared)
        if fingerprint(result) != expected:
            raise ValueError("Allocation replay changed the complete product")
        methods[name].update(
            allocations=allocations(trace)
            | {
                "scope": "Warm construct only; preexisting source/profile/decoded DEM excluded. Native/Python allocator events, no native-stack tracing. Separate from CPU timing."
            },
            work=result["work"],
            owned_bed_array_bytes=result["known_owned_bed_array_bytes"],
            owned_final_terrain_array_bytes=result["terrain"]["mesh"].vertices.nbytes
            + result["terrain"]["mesh"].triangles.nbytes,
            total_internal_copied_bytes=None,
        )
        methods[name]["cpu_median_s"] = float(
            np.median([r["cpu_s"] for r in methods[name]["rounds"]])
        )
        methods[name]["wall_median_s"] = float(
            np.median([r["wall_s"] for r in methods[name]["rounds"]])
        )
        del result
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "memray": memray.__version__,
        "platform": platform.platform(),
        "complete_product_sha256": expected,
        "input_prepare_cpu_s": prepare_cpu,
        "cold_construct_with_declared_raster_loads": cold_cost,
        "methods": methods,
        "new_elevation_sha256": manifest["additional_elevation"]["sha256"],
        "scope": "Same original7-source bed, all15 exported arrays and complete terrain. Warm input/profile/DEM reuse, seven alternating rounds, instrumentation separate. Peak excludes existing shared inputs; requested allocations are not copy bytes. No device/World/p99/UMA claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: {n: v[n] for n in ("cpu_median_s", "wall_median_s")}
                for k, v in methods.items()
            }
        )
    )


if __name__ == "__main__":
    main()
