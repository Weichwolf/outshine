"""Matched native fields, packet residency, rehydration and native-level Zstd1."""

import argparse
import ctypes
import ctypes.util
import hashlib
import json
import platform
import resource
import subprocess
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
from vertex_field_cases import cases, shader_records
from vertex_field_oracle import scalar_decode
from vertex_field_words import decode, encode


class Zstd:
    def __init__(self):
        self.lib = ctypes.CDLL(ctypes.util.find_library("zstd"))
        lib = self.lib
        lib.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
        lib.ZSTD_compressBound.restype = ctypes.c_size_t
        lib.ZSTD_compress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_int,
        ]
        lib.ZSTD_compress.restype = ctypes.c_size_t
        lib.ZSTD_decompress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        lib.ZSTD_decompress.restype = ctypes.c_size_t
        lib.ZSTD_isError.argtypes = [ctypes.c_size_t]
        lib.ZSTD_isError.restype = ctypes.c_uint
        lib.ZSTD_versionNumber.restype = ctypes.c_uint

    def compress(self, blob):
        if len(blob) < 64:
            return 0, blob
        out = ctypes.create_string_buffer(self.lib.ZSTD_compressBound(len(blob)))
        size = self.lib.ZSTD_compress(
            out, len(out), ctypes.c_char_p(blob), len(blob), 1
        )
        if self.lib.ZSTD_isError(size):
            raise RuntimeError("Zstd1 compression failed")
        return (1, ctypes.string_at(out, size)) if size < len(blob) else (0, blob)

    def decompress(self, encoded, size):
        codec, blob = encoded
        if not codec:
            if len(blob) != size:
                raise ValueError("Raw extent mismatch")
            return blob
        out = ctypes.create_string_buffer(size)
        count = self.lib.ZSTD_decompress(out, size, ctypes.c_char_p(blob), len(blob))
        if self.lib.ZSTD_isError(count) or count != size:
            raise ValueError("Zstd1 decompression failed")
        return ctypes.string_at(out, count)


def measured(call, traced):
    if traced:
        tracemalloc.start()
    start = time.perf_counter()
    value = call()
    elapsed = time.perf_counter() - start
    peak = tracemalloc.get_traced_memory()[1] if traced else None
    if traced:
        tracemalloc.stop()
    return value, {"seconds": elapsed, "traced_peak_bytes": peak}


def measure_case(name, source, raw, receipt, args, zstd):
    n, triangles = len(source["positions"]), len(source["indices"])
    original = shader_records(source)
    expected = hashlib.sha256(original).hexdigest()
    if expected != receipt["expanded_native_sha256"]:
        raise ValueError("Source disagrees with its independent native stream oracle")
    raw_wire, raw_compress = measured(lambda: zstd.compress(raw), args.trace)
    uncompressed, raw_decompress = measured(
        lambda: zstd.decompress(raw_wire, len(raw)), args.trace
    )
    if uncompressed != raw:
        raise ValueError("Native core compression changed words")
    candidate, construction = measured(
        lambda: encode(source, mode=args.mode, raw_core_bytes=len(raw)), args.trace
    )
    report = {
        "case": name,
        "input": receipt,
        "vertices": n,
        "triangles": triangles,
        "native_core_bytes": len(raw),
        "native_zstd1_bytes": len(raw_wire[1]),
        "raw_compress": raw_compress,
        "raw_decompress": raw_decompress,
        "construction": construction,
        "candidate": candidate.report,
        "declined": candidate.reason,
    }
    if candidate.payload is None:
        return report
    payload = candidate.payload
    decoded, reconstruction = measured(lambda: decode(payload), args.trace)
    oracle, scalar_cost = measured(lambda: scalar_decode(payload), False)
    for current in (decoded, oracle):
        if shader_records(current) != original:
            raise ValueError("Decoded shader inputs changed")
    wire, compress_cost = measured(lambda: zstd.compress(payload), args.trace)
    restored, decompress_cost = measured(
        lambda: zstd.decompress(wire, len(payload)), args.trace
    )
    if restored != payload or shader_records(decode(restored)) != original:
        raise ValueError("Compressed field packet changed shader inputs")
    path = args.output / f"{name}.vfw"
    path.write_bytes(payload)
    np.savez_compressed(args.output / f"{name}-decoded.npz", **decoded)
    if "binding" in source:
        np.save(args.output / f"{name}-binding.npy", source["binding"])
    report.update(
        {
            "payload_sha256": hashlib.sha256(payload).hexdigest(),
            "expanded_corner_sha256": expected,
            "scalar_oracle": scalar_cost,
            "reconstruction": reconstruction,
            "zstd1_bytes": len(wire[1]),
            "compress": compress_cost,
            "decompress": decompress_cost,
            "known_serialization_copy_bytes": 2 * len(payload),
            "diagnostic_decoded_field_bytes": sum(
                decoded[k].nbytes
                for k in ("positions", "uv", "normal", "metres", "colour")
            ),
            "cpu_decode_retains_payload_via_index_view": True,
            "logical_vertex_order_unchanged": True,
            "binding_bytes_common_unchanged": source["binding"].nbytes
            if "binding" in source
            else None,
            "input_rgba_range": [
                float(source["colour"].min()),
                float(source["colour"].max()),
            ],
        }
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("all", "position-normal"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    if (
        args.output.exists()
        or args.output.resolve() == temp
        or not args.output.resolve().is_relative_to(temp)
    ):
        parser.error("Fresh explicit system-temp directory required")
    tracked = [
        "test/experiments/render/" + name
        for name in (
            "vertex_field_words.py",
            "vertex_field_oracle.py",
            "vertex_field_cases.py",
            "test_vertex_field_words.py",
            "compare_vertex_field_words.py",
        )
    ]
    if subprocess.check_output(["git", "status", "--porcelain", "--", *tracked]):
        parser.error("Freeze the field experiment before measuring it")
    args.output.mkdir(parents=True)
    zstd = Zstd()
    result = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "mode": args.mode,
        "trace": args.trace,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "zstd_version_number": zstd.lib.ZSTD_versionNumber(),
        "cases": [],
    }
    iterator = iter(cases())
    while True:
        start = time.perf_counter()
        try:
            name, source, raw, receipt = next(iterator)
        except StopIteration:
            break
        setup = time.perf_counter() - start
        row = measure_case(name, source, raw, receipt, args, zstd)
        row["input_decode_verify_serialize_seconds"] = setup
        result["cases"].append(row)
        print(
            name,
            row["native_core_bytes"],
            row["candidate"].get("payload_bytes"),
            row.get("native_zstd1_bytes"),
            row.get("zstd1_bytes"),
            flush=True,
        )
    result.update(
        {
            "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            * 1024,
            "scope": "Linux Python shared host, one BLAS thread. Stage times include Python/ctypes, not native runtime. Separate raw-core/field-packet Zstd1 follows native level/min-size/raw-fallback policy; shared asset metadata/real package grouping excluded. Decoder creates diagnostic full arrays and retains encoded packet through index views; no product-RAM saving from CPU expansion. GPU needs new agreed token/table fetch consumer: 40 versus32 logical field bytes/wall vertex (36 for partial control), cache behavior and device time unknown. RGBA stays raw/default-white, no colour palette. Native GPU/IO/upload/p99/world admission open.",
        }
    )
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
