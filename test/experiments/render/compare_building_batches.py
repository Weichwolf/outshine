"""Compare bounded resident stream pages against exact frozen native cell products."""

import argparse
import hashlib
import json
import resource
import statistics
import subprocess
import sys
import tempfile
import time
from collections import Counter
from contextlib import nullcontext
from pathlib import Path

from building_resident_batches import pack, pages, verify
from cached_building_streams import basis_sources, product


def checked(path, expected):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError("frozen source/input hash mismatch")
    return data


def peak_memory():
    raw = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    if sys.platform == "linux":
        for line in Path("/proc/self/status").read_text().splitlines():
            if line.startswith("VmHWM:"):
                return {
                    "peak_process_rss_bytes": int(line.split()[1]) * 1024,
                    "source": "/proc/self/status VmHWM since exec",
                    "rusage_maxrss_bytes": raw * 1024,
                }
        raise ValueError("Linux process high-water RSS unavailable")
    if sys.platform == "darwin":
        return {
            "peak_process_rss_bytes": raw,
            "source": "Darwin getrusage; startup/fork floor not independently excluded",
        }
    return {
        "peak_process_rss_bytes": None,
        "source": "unsupported host; RSS remains unmeasured",
    }


def cohort(inputs, spec, output, track_pack=False):
    base = spec["basis"]
    anchor, sources = basis_sources((inputs / f"{base}.basis").read_bytes())
    items = []
    for record in spec["products"]:
        data = checked(inputs / f"{record['key']}.structure", record["sha256"])
        item = product(data, record["key"]) | {"parent": spec["parent"]}
        if item["footprints"] != int((sources["cell"] == item["cell"]).sum()):
            raise ValueError("source/contact metadata coverage changed")
        items.append(item)
    if {p["cell"] for p in items} != set(sources["cell"].tolist()):
        raise ValueError("incomplete parent source cohort")
    alternatives = []
    for capacity in (1 << 20, 4 << 20, 16 << 20):
        try:
            groups = pages(items, capacity)
            alternatives.append({"capacity_bytes": capacity, "pages": len(groups)})
        except ValueError as error:
            alternatives.append({"capacity_bytes": capacity, "rejected": str(error)})
    groups, durations = pages(items, 16 << 20), []
    output.mkdir()
    scope = nullcontext()
    if track_pack:
        import memray

        scope = memray.Tracker(
            str(output / "allocations.bin"), trace_python_allocators=True
        )
    packed = None
    with scope:
        for _ in range(1 if track_pack else 3):
            packed = None
            started = time.perf_counter()
            packed = [pack(group) for group in groups]
            durations.append(time.perf_counter() - started)
    page_rows = []
    for page, (group, (streams, mapping)) in enumerate(
        zip(groups, packed, strict=True)
    ):
        verify(streams, mapping, group)
        if not track_pack:
            for index, stream in enumerate(streams):
                (output / f"page-{page}-stream-{index}.bin").write_bytes(stream)
        page_rows.append(
            {
                "cells": [p["cell"] for p in group],
                "mapping": mapping,
                "stream_bytes": [len(s) for s in streams],
                "radius_m": min(p["view"]["radius_m"] for p in group),
            }
        )
    stream_bytes = sum(sum(len(s) for s in p["streams"]) for p in items)
    rewritten = sum(
        sum(len(p["streams"][i]) for i in (2, 3, 7, 9))
        + (p["counts"][4] + p["counts"][8]) * 4
        for p in items
    )
    report = {
        "parent": spec["parent"],
        "basis": base,
        "source_count": len(sources),
        "source_identity_sha256": hashlib.sha256(sources.tobytes()).hexdigest(),
        "anchor_ecef": anchor,
        "view": items[0]["view"],
        "cell_count": len(items),
        "alternatives": alternatives,
        "pages": page_rows,
        "stream_bytes": stream_bytes,
        "largest_cell_stream_bytes": max(
            sum(len(s) for s in p["streams"]) for p in items
        ),
        "explicit_destination_copied_bytes": stream_bytes,
        "explicit_offset_write_bytes": rewritten,
        "median_pack_seconds": statistics.median(durations),
        "pack_instrumented": track_pack,
        "pack_seconds": durations,
        "original_counters": [sum(p["counters"][i] for p in items) for i in range(8)],
        "clusters": sum(p["counts"][4] + p["counts"][8] for p in items),
        "triangle_index_references": sum(
            (p["counts"][7] + p["counts"][9]) // 3 for p in items
        ),
        "scope": "Exact ten-stream storage/rebase reference. Not native batches/draws, reduced triangles, a new LOD, lifecycle/streaming acceptance, GPU timing or device budget. Pack timing excludes parse/verification/IO.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "parent": spec["parent"],
                "cells": len(items),
                "pages": len(groups),
                "bytes": stream_bytes,
                "pack_seconds": report["median_pack_seconds"],
            }
        ),
        flush=True,
    )
    return report


def run(inputs, output, track_pack=False):
    manifest_path = inputs / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    occupancy = Counter()
    for base in manifest["basis"]:
        _, sources = basis_sources(checked(inputs / base["file"], base["sha256"]))
        occupancy.update(Counter(sources["cell"].tolist()).values())
    cases = [
        cohort(inputs, spec, output / spec["parent"], track_pack)
        for spec in manifest["cohorts"]
    ]
    report = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "source_sha256": {
            name: hashlib.sha256(
                Path(__file__).with_name(name).read_bytes()
            ).hexdigest()
            for name in (
                "cached_building_streams.py",
                "building_resident_batches.py",
                Path(__file__).name,
            )
        },
        "basis_count": len(manifest["basis"]),
        "occupied_cells": sum(occupancy.values()),
        "sources": sum(n * count for n, count in occupancy.items()),
        "cells_below_16": sum(count for n, count in occupancy.items() if n < 16),
        "sources_in_cells_below_16": sum(
            n * count for n, count in occupancy.items() if n < 16
        ),
        "cases": cases,
        "memory": peak_memory(),
        "scope": "Occupancy gives a necessary eligibility ceiling only, not measured far/unknown/error classifications or actual projected triangles. Local cache cohort, not current master/device scene equivalence.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--track-pack", action="store_true")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    run(args.inputs.resolve(), target, args.track_pack)
