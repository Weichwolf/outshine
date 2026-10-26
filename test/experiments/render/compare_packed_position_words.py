"""Matched original core byte equality, native-policy Zstd1 and Python costs."""

import argparse
import hashlib
import json
import platform
import resource
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from compare_vertex_field_words import Zstd, measured
from packed_position_oracle import scalar_decode
from packed_position_words import decode, encode, position_words
from vertex_field_cases import cases, shader_records


def compare(name, source, raw, receipt, output, trace, zstd):
    spans = receipt["vertex_spans"]
    if position_words(raw, spans).tobytes() != source["positions"].tobytes():
        raise ValueError("Common native span layout changed original positions")
    original = shader_records(source)
    if hashlib.sha256(original).hexdigest() != receipt["expanded_native_sha256"]:
        raise ValueError("Original fields disagree with archive oracle")
    raw_wire, raw_compress = measured(lambda: zstd.compress(raw), trace)
    restored, raw_decompress = measured(
        lambda: zstd.decompress(raw_wire, len(raw)), trace
    )
    if restored != raw:
        raise ValueError("Raw Zstd control changed bytes")
    candidate, construction = measured(lambda: encode(raw, spans), trace)
    report = {
        "case": name,
        "input": receipt,
        "native_core_bytes": len(raw),
        "native_core_sha256": hashlib.sha256(raw).hexdigest(),
        "native_zstd1_bytes": len(raw_wire[1]),
        "raw_compress": raw_compress,
        "raw_decompress": raw_decompress,
        "construction": construction,
        "candidate": candidate.report,
        "declined": candidate.reason,
    }
    if candidate.payload is None:
        report["selected_path"] = "Raw"
        return report
    payload = candidate.payload
    restored, reconstruction = measured(lambda: decode(payload, spans), trace)
    oracle, scalar_cost = measured(lambda: scalar_decode(payload, spans), False)
    if restored != raw or oracle != raw:
        raise ValueError("Full native core differs from vector/scalar decoder")
    current = source | {"positions": position_words(restored, spans).view("<f4")}
    if shader_records(current) != original:
        raise ValueError("Native corner fields changed")
    wire, compression = measured(lambda: zstd.compress(payload), trace)
    uncompressed, decompression = measured(
        lambda: zstd.decompress(wire, len(payload)), trace
    )
    if uncompressed != payload or decode(uncompressed, spans) != raw:
        raise ValueError("Compressed packed core changed bytes")
    (output / f"{name}.ppw").write_bytes(payload)
    (output / f"{name}-layout.json").write_text(
        json.dumps({"vertex_spans": spans, "raw_core_bytes": len(raw)}, indent=2) + "\n"
    )
    report.update(
        {
            "payload_sha256": hashlib.sha256(payload).hexdigest(),
            "reconstruction": reconstruction,
            "scalar_oracle": scalar_cost,
            "compress": compression,
            "decompress": decompression,
            "zstd1_bytes": len(wire[1]),
            "full_native_core_byte_identical": True,
            "expanded_corner_sha256": hashlib.sha256(original).hexdigest(),
            "selected_path": "packed" if len(wire[1]) < len(raw_wire[1]) else "Raw",
            "known_core_rehydration_copy_bytes": 2 * len(raw),
            "diagnostic_cpu_output_bytes": len(restored),
            "cpu_output_retains_packet": False,
        }
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    if (
        args.output.exists()
        or not args.output.resolve().is_relative_to(temp)
        or args.output.resolve() == temp
    ):
        parser.error("Fresh explicit system-temp directory required")
    files = [
        "test/experiments/render/" + name
        for name in (
            "packed_position_words.py",
            "packed_position_oracle.py",
            "compare_packed_position_words.py",
            "test_packed_position_words.py",
            "vertex_field_cases.py",
            "compare_vertex_field_words.py",
        )
    ]
    if subprocess.check_output(["git", "status", "--porcelain", "--", *files]):
        parser.error("Freeze input/codec/control sources before measuring")
    args.output.mkdir(parents=True)
    zstd = Zstd()
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
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
        row = compare(name, source, raw, receipt, args.output, args.trace, zstd)
        row["input_decode_verify_serialize_seconds"] = setup
        report["cases"].append(row)
        print(
            name,
            row["candidate"]["sum_bits"],
            row["selected_path"],
            row.get("zstd1_bytes"),
            flush=True,
        )
    report["process_peak_rss_bytes"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    report["scope"] = (
        "Original isolated native cores, common layout metadata excluded equally, not actual package grouping/SSD IO. Python/ctypes shared-host times, separate tracing. Raw fallback for overwide words. CPU expands full Raw buffers; no RAM gain. A direct uint32-only GPU vertex consumer is unimplemented and needs agreed bindings/device proof. No native frame/world admission."
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
