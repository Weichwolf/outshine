"""Equal pinned raw-readback inputs: owned NumPy/hash-copy versus borrowed bytes."""

import argparse
import gc
import hashlib
import json
import os
import platform
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from allocation_measure import allocations
from building_sample_source import read_source
from compare_field_external_depth import DATA
from field_external_depth import SampleBinding
from field_native_points import read_file, read_native_points
from process_memory import peak_rss_bytes


def owned_file(path, size, expected):
    if (
        not isinstance(expected, str)
        or len(expected) != 64
        or path.stat().st_size != size
    ):
        raise ValueError("Pinned native raw file size/SHA256 required")
    result = np.fromfile(path, dtype="<f4", count=size // 4)
    if (
        result.nbytes != size
        or hashlib.sha256(result.tobytes()).hexdigest() != expected
    ):
        raise ValueError("Owned baseline differs from its pinned raw bytes")
    result.setflags(write=False)
    return result


def measure(call, paths, output):
    expected = call(read_file)
    methods = {
        "owned-array-and-hash-copy": owned_file,
        "borrowed-immutable-bytes": read_file,
    }
    results = {name: {"rounds": []} for name in methods}
    for round_index in range(9):
        names = list(methods) if round_index % 2 else list(reversed(methods))
        for name in names:
            gc.collect()
            start = time.process_time(), time.perf_counter()
            value = call(methods[name])
            results[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - start[0],
                    "wall_s": time.perf_counter() - start[1],
                }
            )
            np.testing.assert_array_equal(value.rgba, expected.rgba)
            np.testing.assert_array_equal(value.reverse_z, expected.reverse_z)
            assert value.binding == expected.binding and not value.known.any()
            del value
    for name, reader in methods.items():
        gc.collect()
        value, memory = allocations(
            output / (name + ".bin"), lambda reader=reader: call(reader)
        )
        np.testing.assert_array_equal(value.rgba, expected.rgba)
        np.testing.assert_array_equal(value.reverse_z, expected.reverse_z)
        results[name].update(memory)
        for clock in ("cpu", "wall"):
            results[name][clock + "_median_s"] = float(
                np.median([v[clock + "_s"] for v in results[name]["rounds"]])
            )
    return results, sum(p.stat().st_size for p in paths)


def run(args):
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Normal measurement priority required")
    paths = [Path(__file__), Path(__file__).with_name("field_native_points.py")]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze the measured source first")
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Explicit system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    metadata, arrays, _grid, _view = read_source(
        args.inputs / "source-512", args.inputs / "high-sampling"
    )
    colour, depth = args.output / "source-field.f32", args.output / "source-depth.f32"
    arrays["source_lit_rgba"].tofile(colour)
    arrays["source_raster_depth"].tofile(depth)
    pins = {
        "colour_sha256": hashlib.sha256(colour.read_bytes()).hexdigest(),
        "depth_sha256": hashlib.sha256(depth.read_bytes()).hexdigest(),
    }
    record = {
        "field_asset_key": metadata["stored_field_sha256"],
        "source_readback_bytes": 0,
        "source_depth": metadata["capture"],
        "lighting_observer": {
            "orthographic": True,
            "eye_m": metadata["capture"]["eye_m"],
            "forward": metadata["capture"]["forward"],
        },
    }
    provenance = SampleBinding(
        "exact-archived-source-frame",
        "requested-capture",
        "controlled-cost-scene",
        (record["field_asset_key"],),
        "archived-source-materials",
        "archived-capture-lighting",
        "requested-observer",
    )

    def call(reader):
        # Both alternatives include identical temporary patch dispatch overhead.
        with patch("field_native_points.read_file", reader):
            return read_native_points(colour, depth, record, provenance, **pins)

    load = os.getloadavg()
    measured, payload = measure(call, (colour, depth), args.output)
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "python": platform.python_version(),
        "platform": platform.platform(),
        "load_before": load,
        "load_after": os.getloadavg(),
        "controlled_record": record,
        "pins": pins,
        "measurements": measured,
        "work": {
            "file_reads_per_call": 2,
            "raw_read_bytes_per_call": payload,
            "payload_hash_bytes_per_call": payload,
            "owned_baseline_known_extra_hash_copy_bytes": payload,
            "borrowed_known_extra_hash_copy_bytes": 0,
            "owned_raw_payload_bytes": payload,
            "unqualified_domain_scalar_payload_bytes": 9,
            "all_unqualified_points": metadata["capture"]["width_px"]
            * metadata["capture"]["height_px"],
            "other_copies_and_physical_traffic": None,
        },
        "combined_process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "Controlled current-layout metadata built from an existing archived native512 raw payload; not a new/current native producer run. Both alternatives use exactly the same header, pinned payload, hash and coupling checks and return the same unqualified immutable point values. File cache warmed before nine alternating normal-priority rounds; archive decoding/export are outside timings. Shared patch dispatch is included in both. Known extra copies cover hashing only, not IO/validation/total physical traffic. Memray excludes held preparation inputs; combined RSS includes both alternatives and profiler. No compressed AssetCache, frame, GPU or World admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: {n: v for n, v in row.items() if n.endswith("median_s")}
                for k, row in measured.items()
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
