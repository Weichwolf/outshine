#!/usr/bin/env python3
"""CPU-only package/delta experiment on exact #926 atoms; no runtime LOD admission."""

import argparse
import gc
import hashlib
import json
import platform
import resource
from native_package_storage import open_packages, prepare_packages
import subprocess
import sys
import sqlite3
import tempfile
import time
import zlib
from collections import OrderedDict
from contextlib import closing
from pathlib import Path

import native_road_page_layout as layout
import native_road_parts as corpus
import zstandard


def delta(wanted, resident, pending=frozenset()):
    """Already requested is not ready; only avoid issuing a duplicate request."""
    return wanted - resident - pending, pending - wanted


def disk_read_bytes():
    path = Path("/proc/self/io")
    if not path.exists():
        return None
    return int(
        dict(line.split(": ") for line in path.read_text().splitlines())["read_bytes"]
    )


def process_peak():
    status = Path("/proc/self/status")
    if status.exists():
        for line in status.read_text().splitlines():
            if line.startswith("VmHWM:"):
                return int(line.split()[1]) * 1024, "Linux /proc/self/status VmHWM"
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * (
        1 if sys.platform == "darwin" else 1024
    ), "getrusage ru_maxrss; pre-exec inheritance not excluded"


def load_page(connection, key, specification):
    stored = connection.execute(
        "SELECT bytes FROM native_packages WHERE key=?", (key,)
    ).fetchone()[0]
    crc, codec, native_bytes = specification
    if zlib.crc32(stored) != crc:
        raise ValueError("package CRC differs")
    if codec == 1:
        if zstandard.frame_content_size(stored) != native_bytes:
            raise ValueError("package frame size differs")
        decoded = zstandard.ZstdDecompressor().decompress(
            stored, max_output_size=native_bytes, allow_extra_data=False
        )
    elif codec == 0:
        decoded = stored
    else:
        raise ValueError("unsupported package codec")
    if len(decoded) != native_bytes:
        raise ValueError("package decoded size differs")
    return decoded, len(stored), codec


def replay(directory, trace, budget):
    """Serial CPU load/retire; no GPU fences, asynchronous latency or visibility proof."""
    start_reads = disk_read_bytes()
    with closing(
        open_packages(
            (directory / "assets.sqlite").resolve().as_uri() + "?mode=ro", uri=True
        )
    ) as connection:
        result = replay_pages(connection, directory, trace, budget)
    end_reads = disk_read_bytes()
    peak, source = process_peak()
    result.update(
        process_peak_rss_bytes=peak,
        process_peak_rss_source=source,
        os_physical_read_bytes=None if start_reads is None else end_reads - start_reads,
    )
    return result


def resolve_metadata(connection, directory):
    connection.execute("PRAGMA cache_size=-4096")
    records = {
        i: (key, offset, size)
        for i, key, offset, size in connection.execute(
            "SELECT id,package,offset,bytes FROM assets"
        )
    }
    specs = {
        key: (crc, codec, size)
        for key, crc, codec, size in connection.execute(
            "SELECT key,crc,codec,native_bytes FROM native_packages"
        )
    }
    hashes = json.loads((directory / "hashes.json").read_text())
    members = {}
    for i, (key, offset, size) in records.items():
        members.setdefault(key, []).append((i, offset, size))
    return records, specs, hashes, members


def replay_pages(connection, directory, trace, budget):
    prepare_packages(connection)
    started = time.perf_counter()
    records, specs, hashes, members = resolve_metadata(connection, directory)
    metadata_ms = (time.perf_counter() - started) * 1000
    material = max(records)
    resident = OrderedDict()
    totals = {
        name: 0
        for name in (
            "requests",
            "read_bytes",
            "decoded_bytes",
            "loaded_native_bytes",
            "decode_allocations",
            "blob_copy_bytes",
            "unrequested_atom_bytes",
            "evictions",
            "over_budget_frames",
        )
    }
    frames, payload_peak, resident_peak = [], 0, 0
    resident_bytes = 0
    for frame in trace["frames"]:
        frame_started = time.perf_counter()
        ids = {i + 1 for i in frame["parts"] if records[i + 1][2]}
        if ids:
            ids.add(material)
        wanted = {records[i][0] for i in ids}
        missing, _ = delta(wanted, set(resident))
        required_bytes = sum(specs[key][2] for key in wanted)
        incoming_bytes = sum(specs[key][2] for key in missing)
        retired = []
        for key in list(resident):
            if resident_bytes + incoming_bytes <= budget:
                break
            if key not in wanted:
                resident_bytes -= len(resident.pop(key))
                retired.append(key)
        row = {
            "name": frame["name"],
            "wanted_atoms": len(frame["parts"]),
            "wanted_packages": len(wanted),
            "requests": 0,
            "read_bytes": 0,
            "decoded_bytes": 0,
            "loaded_native_bytes": 0,
            "decode_allocations": 0,
            "unrequested_atom_bytes": 0,
        }
        load_ms, verify_ms = 0.0, 0.0
        selection_ms = (time.perf_counter() - frame_started) * 1000
        for key in sorted(missing):
            began = time.perf_counter()
            decoded, stored_bytes, codec = load_page(connection, key, specs[key])
            load_ms += (time.perf_counter() - began) * 1000
            # Compressed input and decoded output coexist; raw input/output alias.
            payload_peak = max(
                payload_peak,
                resident_bytes + len(decoded) + (stored_bytes if codec else 0),
            )
            resident[key] = decoded
            resident_bytes += len(decoded)
            row["requests"] += 1
            row["read_bytes"] += stored_bytes
            row["decoded_bytes"] += len(decoded) if codec else 0
            row["loaded_native_bytes"] += len(decoded)
            row["decode_allocations"] += bool(codec)
            row["unrequested_atom_bytes"] += sum(
                size for i, _, size in members[key] if i not in ids
            )
            began = time.perf_counter()
            for i, offset, size in members[key]:
                view = memoryview(decoded)[offset : offset + size]
                if (
                    len(view) != size
                    or hashlib.sha256(view).hexdigest() != hashes[str(i)]
                ):
                    raise ValueError("loaded atom differs from original native payload")
            verify_ms += (time.perf_counter() - began) * 1000
            del decoded, view
        if not wanted <= resident.keys():
            raise ValueError("required package evicted or missing")
        for key in sorted(wanted):
            resident.move_to_end(key)
        row.update(
            selection_ms=selection_ms,
            load_decode_ms=load_ms,
            verification_ms=verify_ms,
            resident_bytes=resident_bytes,
            required_bytes=required_bytes,
            evictions=len(retired),
            over_budget_bytes=max(0, required_bytes - budget),
        )
        for key in (
            "requests",
            "read_bytes",
            "decoded_bytes",
            "loaded_native_bytes",
            "decode_allocations",
            "unrequested_atom_bytes",
        ):
            totals[key] += row[key]
        totals["blob_copy_bytes"] += row["read_bytes"]
        totals["evictions"] += len(retired)
        totals["over_budget_frames"] += required_bytes > budget
        resident_peak = max(resident_peak, resident_bytes)
        row["total_cpu_ms"] = (time.perf_counter() - frame_started) * 1000
        frames.append(row)
    return {
        "totals": totals,
        "frames": frames,
        "metadata_resolve_ms": metadata_ms,
        "resident_payload_peak_bytes": resident_peak,
        "payload_and_decode_peak_bytes": payload_peak,
        "sqlite_version": sqlite3.sqlite_version,
        "zstandard_version": zstandard.__version__,
        "resident_payload_target_bytes": budget,
    }


def experiment(args):
    started = time.perf_counter()
    manifest, metadata, parts, payload, materials = corpus.load(args.input)
    boxes = layout.bounds(parts, payload)
    sizes = [p["payload_bytes"] for p in parts]
    traces = {
        "corridor": layout.demands(parts, metadata, boxes, args.samples, args.radius),
        "dense": layout.demands(
            parts, metadata, boxes, args.samples, args.radius, "dense"
        ),
        "dense-wide": layout.demands(
            parts, metadata, boxes, args.samples, args.wide_radius, "dense"
        ),
    }
    args.output.mkdir(parents=True)
    for name, trace in traces.items():
        (args.output / f"trace-{name}.json").write_text(json.dumps(trace))
    groups = {
        "monolithic": [list(range(len(parts)))],
        "fixed": layout.partition(boxes, sizes, args.cell_metres),
        "adaptive": layout.partition(boxes, sizes, args.cell_metres, args.page_bytes),
    }
    prepared = {
        name: layout.prepare(
            args.output / name,
            group,
            parts,
            boxes,
            payload,
            materials,
            manifest["archive_sha256"],
        )
        for name, group in groups.items()
    }
    report = {
        "input_archive_sha256": manifest["archive_sha256"],
        "host": platform.platform(),
        "cell_metres": args.cell_metres,
        "adaptive_target_bytes": args.page_bytes,
        "traces": {
            name: {k: v for k, v in trace.items() if k != "frames"}
            for name, trace in traces.items()
        },
        "frames_per_trace": len(trace["frames"]),
        "empty_geometry_atoms": sum(p["payload_bytes"] == 0 for p in parts),
        "offline_work": {
            "native_atoms": len(parts),
            "bounds_vertices_scanned": sum(p["counts"][0] // 3 for p in parts),
            "seed_aabb_tests": len(metadata["core_edges"])
            * sum(len(t["frames"]) for t in traces.values()),
            "contact_legs_checked": sum(
                len(parts[i]["legs"])
                for t in traces.values()
                for f in t["frames"]
                for i in f["parts"]
                if parts[i]["kind"] == "junction"
            ),
        },
        "offline_prepare_ms": (time.perf_counter() - started) * 1000,
        "offline_preparer_peak_rss_bytes": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "layouts": prepared,
        "scope": "Fixed fine-atom packing and synthetic ROI-demand replay only. Fine bounds/contact closure prepared offline. No LOD/occlusion/native GPU/async or device-budget admission.",
    }
    del metadata, parts, payload, materials, boxes, groups
    gc.collect()
    for name, item in prepared.items():
        item["replays"] = {}
        for case in traces:
            result = args.output / name / f"replay-{case}.json"
            command = [
                sys.executable,
                "-B",
                str(Path(__file__).resolve()),
                "--database",
                str(args.output / name),
                "--trace",
                str(args.output / f"trace-{case}.json"),
                "--budget-bytes",
                str(args.budget_bytes),
                "--output",
                str(result),
            ]
            subprocess.run(command, check=True)
            item["replays"][case] = json.loads(result.read_text())
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    for name, item in prepared.items():
        for case, r in item["replays"].items():
            print(
                json.dumps(
                    {
                        "layout": name,
                        "case": case,
                        "packages": len(item["packages"]),
                        **r["totals"],
                        "payload_peak": r["payload_and_decode_peak_bytes"],
                    }
                )
            )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=corpus.DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--database", type=Path)
    parser.add_argument("--trace", type=Path)
    parser.add_argument("--budget-bytes", type=int, default=2 * 1024**2)
    parser.add_argument("--page-bytes", type=int, default=64 * 1024)
    parser.add_argument("--cell-metres", type=float, default=256.0)
    parser.add_argument("--radius", type=float, default=150.0)
    parser.add_argument("--wide-radius", type=float, default=500.0)
    parser.add_argument("--samples", type=int, default=17)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error(
            "private experiment output must be in the system temporary directory"
        )
    if (
        min(
            args.budget_bytes,
            args.page_bytes,
            args.cell_metres,
            args.radius,
            args.wide_radius,
        )
        <= 0
        or args.samples < 2
    ):
        parser.error("positive sizes/radius and at least two route samples required")
    if args.database:
        if args.trace is None:
            parser.error("--database requires --trace")
        result = replay(
            args.database, json.loads(args.trace.read_text()), args.budget_bytes
        )
        args.output.write_text(json.dumps(result, indent=2) + "\n")
    else:
        experiment(args)


if __name__ == "__main__":
    main()
