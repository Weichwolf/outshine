"""Build one bounded source-aware trilinear field before its image/cost comparison."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from reconstructed_source_field import LEAF, SOLID, UNKNOWN, FieldBuilder
from source_support_function import compile_support


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        support, basis, anchor, needs = compile_support(parent, set(args.cells))
        compiled = time.perf_counter()
        builder = FieldBuilder(support)
        probed = time.perf_counter()
        arrays, work = builder.run()
        built = time.perf_counter()
        arrays.update(
            basis=basis,
            anchor=anchor,
            accepted_sources=support.source_ids,
            needs_sources=np.array([r["source"] for r in needs], np.uint32),
        )
        tags = arrays["tags"]
        report = {
            "source_archive_sha256": parent.archive_sha256,
            "source_archive_bytes": args.input.stat().st_size,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "cells": args.cells,
            "sources": len(support.source_ids),
            "needs": needs,
            "source_open_seconds": opened - started,
            "support_compile_seconds": compiled - opened,
            "source_probe_seconds": probed - compiled,
            "field_build_seconds": built - probed,
            "nodes": len(tags),
            "empty_nodes": int((tags == 0).sum()),
            "solid_leaves": int((tags == SOLID).sum()),
            "reconstructed_leaves": int(((tags & LEAF) != 0).sum()),
            "unresolved_leaves": int(((tags & UNKNOWN) != 0).sum()),
            "unique_unresolved_source_needs": len(np.unique(arrays["unknown_sources"])),
            "work": work,
            "profile": {
                "max_nodes": 4681,
                "minimum_side_m": 1.0,
                "source_probe_offset_m": 0.125,
                "exterior_value_fit_per_side": 0.05,
                "probe_sign_floor_m": 1e-8,
            },
        }
    np.savez_compressed(args.output / "field.npz", **arrays)
    report.update(
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        array_bytes={key: value.nbytes for key, value in arrays.items()},
        field_file_bytes=(args.output / "field.npz").stat().st_size,
        process_peak_rss_bytes=peak_rss_bytes(),
        total_allocation_count=None,
        total_physical_copied_bytes=None,
        scope="One source-aware finite-probe reconstruction profile. Unknown leaves retain local source/finer needs; complete missing sources remain original needs. Cell-clipped trilinear I<=0 is the candidate model, not a unit SDF, global continuity/error certificate, material contract or qualified World replacement. No original Shell/triangle interpretation in construction. Bytes include unknown-source metadata but not the still-required unresolved finer products.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", nargs="+", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
