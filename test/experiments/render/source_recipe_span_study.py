"""Compare indexed original prepared recipe reads with whole-cell decoding.

Offsets and digests are made once offline; this is not a production cache format.
"""

import argparse
import hashlib
import io
import json
import time
from pathlib import Path

import numpy as np
from prepared_surface_recipes import Reader, decode, read_surface
from source_recipe_ranges import load_ranges


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(args):
    index_raw = (args.recipes / "index.json").read_bytes()
    if digest(index_raw) != args.index_sha256:
        raise ValueError("Bound prepared recipe index required")
    metadata = json.loads(index_raw)
    selected = set(
        next(
            row["selected_source_ids"]
            for row in json.loads(args.view.read_text())["cases"]
            if row["angle_degrees"] == args.angle
        )
    )
    rows, oracle = [], {}
    offline_begin = time.process_time()
    full_bytes = full_scalars = full_sources = 0
    for cell in metadata:
        ordinal_raw = (args.recipes / cell["source_file"]).read_bytes()
        raw = (args.recipes / cell["file"]).read_bytes()
        if (
            digest(ordinal_raw) != cell["source_sha256"]
            or digest(raw) != cell["sha256"]
            or len(raw) != cell["bytes"]
        ):
            raise ValueError("Bound whole recipe cell required")
        ids = np.load(io.BytesIO(ordinal_raw), allow_pickle=False)
        reader = Reader(raw)
        if reader.number("I") != 0x33435342 or reader.count(65536) != len(ids):
            raise ValueError("Original cell header and membership required")
        for identity in ids:
            begin = reader.at
            read_surface(reader)
            end = reader.at
            rows.append(
                (
                    int(identity),
                    cell["cell"],
                    begin,
                    end - begin,
                    np.frombuffer(hashlib.sha256(raw[begin:end]).digest(), dtype="u1"),
                )
            )
        if reader.at != len(raw):
            raise ValueError("Complete original cell required")
        if selected.intersection(map(int, ids)):
            surfaces, work = decode(raw)
            full_bytes += len(raw)
            full_scalars += work["scalar_reads"]
            full_sources += len(surfaces)
            oracle.update(
                (int(i), v)
                for i, v in zip(ids, surfaces, strict=True)
                if int(i) in selected
            )
    offline_cpu = time.process_time() - offline_begin
    table = np.array([r[:4] for r in rows], dtype="<u4")
    hashes = np.array([r[4] for r in rows], dtype="u1")
    if len(set(map(int, table[:, 0]))) != len(table) or set(oracle) != selected:
        raise ValueError("Complete unique source identities required")
    np.savez(args.output, table=table, sha256=hashes)
    with np.load(args.output, allow_pickle=False) as stored:
        if not np.array_equal(stored["table"], table) or not np.array_equal(
            stored["sha256"], hashes
        ):
            raise ValueError("Complete stored recipe span index required")
        table, hashes = stored["table"], stored["sha256"]
    began = time.process_time()
    decoded, work = load_ranges(
        table,
        hashes,
        [(cell["cell"], args.recipes / cell["file"]) for cell in metadata],
        selected,
        args.gap_bytes,
    )
    if decoded != oracle:
        raise ValueError("Selected recipes differ from complete cell decode")
    cpu = time.process_time() - began
    report = {
        "angle": args.angle,
        "sources_in_index": len(table),
        "selected_sources": len(decoded),
        "coalesce_gap_bytes": args.gap_bytes,
        "prepared_span_index_bytes": args.output.stat().st_size,
        "selected_recipe_bytes_read": work["recipe_range_bytes"],
        "selected_read_calls": work["recipe_range_reads"],
        "selected_scalar_reads": work["recipe_scalar_reads"],
        "single_selected_cpu_s": cpu,
        "whole_cell_bytes_read": full_bytes,
        "whole_cell_sources_decoded": full_sources,
        "whole_cell_scalar_reads": full_scalars,
        "offline_index_and_oracle_cpu_s": offline_cpu,
        "scope": "Offline index scans and hashes all cells; selected reads are actual separate file reads verified against full-cell decoder. Counts exclude recipe index/membership and actual group IO, OS blocks/cache, allocations, physical IO, device time, rendering and World validity. Both decoders produce complete enriched source recipes; no view/shadow decision is made here.",
    }
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipes", type=Path, required=True)
    parser.add_argument("--index-sha256", required=True)
    parser.add_argument("--view", type=Path, required=True)
    parser.add_argument("--angle", type=int, default=65)
    parser.add_argument("--gap-bytes", type=int, choices=(0, 256, 4096), default=0)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
