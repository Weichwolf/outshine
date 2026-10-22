"""Plan-only original-parent scaling; no Shell/Fine mesh expansion or view assets."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from pathlib import Path

import shapely
from far_height_profiles import plan_source
from parent_surface_inputs import open_parent


def run(args):
    started = time.perf_counter()
    rows, needs = [], []
    with open_parent(args.input) as parent:
        for cell, ordinals, surfaces in parent.iter_forms():
            counts = {
                "sources": len(ordinals),
                "parts": 0,
                "patches": 0,
                "ring_vertices": 0,
                "planar_domain_coordinates": 0,
                "supported_sources": 0,
            }
            elapsed = 0.0
            for ordinal, surface in zip(ordinals, surfaces, strict=True):
                before = time.perf_counter()
                try:
                    parts = plan_source(int(ordinal), surface)
                except ValueError as error:
                    needs.append(
                        {"source": int(ordinal), "cell": cell, "reason": str(error)}
                    )
                else:
                    counts["supported_sources"] += 1
                    counts["parts"] += len(parts)
                    counts["patches"] += sum(len(part.patches) for part in parts)
                    counts["ring_vertices"] += sum(
                        len(ring)
                        for part in parts
                        for ring in [part.shape["ring"], *part.shape["holes"]]
                    )
                    counts["planar_domain_coordinates"] += sum(
                        len(shapely.get_coordinates(polygon))
                        for part in parts
                        for polygon, _ in part.patches
                    )
                elapsed += time.perf_counter() - before
            rows.append(dict(cell=cell, planning_seconds=elapsed, **counts))
        report = {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "archive_sha256": parent.archive_sha256,
            "parent": parent.parent,
            "form_blocks_read": parent.form_reads,
            "native_form_bytes_read": parent.form_bytes,
            "parent_sources": len(parent.plans["records"]),
            "total_seconds": time.perf_counter() - started,
            "cells": rows,
            "needs": needs,
            "raw_process_peak_rss_bytes": resource.getrusage(
                resource.RUSAGE_SELF
            ).ru_maxrss
            * 1024,
            "scope": "All original parent forms, source ordinals and physical archive order; metadata decoding and roof planning only. No native view/mesh, native timing or device-memory claim. Ring/domain counts are actual work/coordinate counts, not GEOS allocator/resident-size measurements. Allocations and copied bytes unmeasured.",
        }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k not in ["cells", "needs"]}))
    print(
        "totals",
        json.dumps({k: sum(row[k] for row in rows) for k in counts}),
        "fallbacks",
        len(needs),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
