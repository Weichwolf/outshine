"""Lazy coarse metadata pages versus the existing same-model flat query."""

import argparse
import dataclasses
import json
import resource
import time
from pathlib import Path

import numpy as np
from compare_canonical_water_domains import DEFAULT
from compare_water_course_envelope import parent_faces, query_point
from compare_water_grid_height import inputs
from outshine_ref.places.camera import ecef
from outshine_ref.places.sources import tile_geography
from water_colored_reference import ReferenceUpdates
from water_course_envelope import course_block, summarize
from water_envelope_fixture import CourseParent, acquire, prepare_parent
from water_envelope_hierarchy import HierarchicalEnvelope
from water_envelope_pages import metadata_pages
from water_envelope_query import EnvelopeIndex


def compare(flat, blocks, points, root, pages):
    index = HierarchicalEnvelope(
        root, pages.get, scope="fixed-declared-source-snapshot"
    )
    rows = []
    held_pages, held_payloads = set(), set()
    for label, point in points:
        a = flat.query(point, blocks.get)
        b = index.query(point, blocks.get)
        expected = (a.state, a.height_m, a.winner, a.parameter)
        if expected != (b.state, b.height_m, b.winner, b.parameter):
            raise ValueError("hierarchical parent differs from flat same-model query")
        read = b.reason["metadata_pages"]
        restricted = HierarchicalEnvelope(
            root,
            {k: pages[k] for k in read}.get,
            scope="fixed-declared-source-snapshot",
        )
        repeated = restricted.query(
            point, {k: blocks[k] for k in reversed(b.opened)}.get
        )
        if expected != (
            repeated.state,
            repeated.height_m,
            repeated.winner,
            repeated.parameter,
        ):
            raise ValueError("pruned metadata/payload absence changed completed parent")
        rows.append(
            {
                "query": label,
                "result": dataclasses.asdict(b),
                "flat_metadata_entries": len(flat.expected),
                "added_metadata_pages": sorted(set(read) - held_pages),
                "added_payload_keys": sorted(set(b.opened) - held_payloads),
            }
        )
        held_pages.update(read)
        held_payloads.update(b.opened)
    return rows


def scale_case():
    blocks = {}
    for x in range(0, 64, 2):
        for y in range(0, 64, 2):
            key = (6, x, y)
            lat, lon = tile_geography([[0.5, 0.5]], 1, key)
            point = ecef(lat[0], lon[0], 0.0)
            blocks[key] = course_block(
                key,
                [[point, point + [0, 1, 0]]],
                [[100.0, 100.0]],
                [str(key)],
                source_version="synthetic-world-grid-v1",
            )
    summaries = {k: summarize(v) for k, v in blocks.items()}
    flat = EnvelopeIndex(tuple(blocks), summaries, scope="synthetic-world-grid-v1")
    root, pages = metadata_pages(tuple(blocks), summaries)
    lat, lon = tile_geography([[0.5, 0.5]], 1, (6, 32, 32))
    rows = compare(
        flat,
        blocks,
        [("synthetic-near-source", ecef(lat[0], lon[0], 0.0))],
        root,
        pages,
    )
    return {
        "source_cells": len(blocks),
        "pages": len(pages),
        "queries": rows,
        "scope": "synthetic scaling/control layout, not real Earth or device evidence",
    }


def run(fixture, output):
    began = time.perf_counter()
    data, snapshots, epoch, receipts, order = acquire(fixture)
    flat_parent, _ = prepare_parent(data, snapshots, receipts)
    root, pages = metadata_pages(
        flat_parent.index.expected, flat_parent.index.summaries
    )
    index = HierarchicalEnvelope(
        root, pages.get, scope="fixed-declared-source-snapshot"
    )
    parent = CourseParent(index, flat_parent.blocks)
    recipe = ReferenceUpdates(
        receipts,
        epoch,
        snapshots,
        data,
        order,
        parent=parent,
        parent_version=parent.version,
    )
    _, records, _, origin = inputs(fixture)
    mask, faces, arrays = parent_faces(recipe, parent, records)
    points = [(key, query_point(key)) for key in parent.queries]
    rows = compare(flat_parent.index, parent.blocks, points, root, pages)
    visited = {k for row in rows for k in row["result"]["reason"]["metadata_pages"]}
    report = {
        "origin": origin,
        "root": dataclasses.asdict(root),
        "parent_version": index.version,
        "queries": rows,
        "parent_faces": faces,
        "scale_case": scale_case(),
        "work": {
            "source_cells": len(snapshots),
            "producer_metadata_pages": len(pages),
            "distinct_requested_metadata_pages": len(visited),
            "flat_metadata_entries": sum(r["flat_metadata_entries"] for r in rows),
            "hierarchical_metadata_entries": sum(
                r["result"]["reason"]["metadata_entries"] for r in rows
            ),
            "logical_metadata_page_requests": sum(
                len(r["result"]["reason"]["metadata_pages"]) for r in rows
            ),
            "root_json_bytes": len(json.dumps(dataclasses.asdict(root)).encode()),
            "requested_metadata_page_json_bytes": sum(
                len(json.dumps([dataclasses.asdict(c) for c in pages[k]]).encode())
                for r in rows
                for k in r["result"]["reason"]["metadata_pages"]
            ),
            "unique_requested_metadata_page_json_bytes": sum(
                len(json.dumps([dataclasses.asdict(c) for c in pages[k]]).encode())
                for k in visited
            ),
            "flat_catalog_json_bytes": len(
                json.dumps(
                    [
                        dataclasses.asdict(s)
                        for s in flat_parent.index.summaries.values()
                    ]
                ).encode()
            ),
        },
        "scope": [
            "fixed finite snapshot; no newly available worldwide metadata",
            "producer builds from declared source summaries, not camera/resident payloads",
            "runtime index receives one root and page callback, not the full leaf catalog",
            "payloads/pages already resident in this diagnostic; no DB/SSD/FPS or memory winner claim",
            "authoritative root completeness/bounds/version must be supplied; unknown remains pending",
            "added readsets are finite diagnostic demand deltas without eviction, not a DB delta API or cache policy",
        ],
    }
    output.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(output / "parent.npz", **arrays)
    from compare_water_colored_updates import plot

    plot(mask, faces, output, after_title="Same parent through lazy metadata pages")
    report["work"].update(
        elapsed_seconds_including_comparisons_export_plot=time.perf_counter() - began,
        process_peak_rss_kib_linux=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return {
        "work": report["work"],
        "scale_query_work": report["scale_case"]["queries"][0]["result"]["reason"],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(run(args.fixture, args.output), indent=2))


if __name__ == "__main__":
    main()
