"""Original delivered inputs through cell handoff, shared edges and coarse contacts."""

import argparse
import dataclasses
import json
import resource
from pathlib import Path

import numpy as np
import shapely
from compare_canonical_water_domains import DEFAULT, observed_snapshots
from compare_coarse_water_owner import measured, run_case
from water_body_domains import assemble
from water_cell_edges import align_edges, edge_knots
from water_cell_handoff import route_cells
from water_cell_height import prepare, publish_contacts


def inventory(snapshots, products):
    originals = {
        tile + (p.info["layer"], p.feature, p.part): p.polygon
        for tile, s in snapshots.items()
        for p in s.pieces
    }
    pairs, foreign = 0, 0
    for tile, product in products.items():
        _, x, y = tile
        box = shapely.box(x, y, x + 1, y + 1)
        actual = {}
        for p in product.snapshot.pieces:
            parent = product.parents[tile + (p.feature, p.part)]
            actual.setdefault(parent, []).append(p.polygon)
            foreign += parent[:3] != tile
        expected = {}
        for key, polygon in originals.items():
            pairs += 1
            clipped = polygon.intersection(box)
            if clipped.area > 0:
                expected[key] = clipped
        if set(actual) != set(expected):
            raise ValueError("tree handoff differs from complete source-cell pairs")
        for key, parts in actual.items():
            if not shapely.union_all(parts).equals(expected[key]):
                raise ValueError("handoff changes original source-cell coverage")
    coverage = shapely.union_all(
        [shapely.box(t[1], t[2], t[1] + 1, t[2] + 1) for t in products]
    )
    original = shapely.union_all(list(originals.values()))
    returned = shapely.union_all(
        [p.polygon for c in products.values() for p in c.snapshot.pieces]
    )
    return {
        "independent_cartesian_source_cell_pairs": pairs,
        "foreign_owned_parts": foreign,
        "per_original_cell_geometry_and_identity_match": True,
        "global_union_roundoff_difference_tile_units2": float(
            original.intersection(coverage).symmetric_difference(returned).area
        ),
        "outside_declared_batch_area_tile_units2_retained_in_originals": float(
            original.difference(coverage).area
        ),
    }


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    data, raw, indices = observed_snapshots(path)
    manifest = json.loads(path.read_text())["observed_v3"]
    # This closed input is the finite decoded/checksummed delivered table only.
    snapshots = {t: dataclasses.replace(s, complete=True) for t, s in raw.items()}
    options = {
        "source_tiles": tuple(snapshots),
        "scope": "delivered-WATER3-normalization-batch:" + manifest["sha256"],
    }
    (products, route_work), route_cost = measured(
        lambda: route_cells(snapshots, recipients=snapshots, **options)
    )
    oracle, oracle_cost = measured(lambda: inventory(snapshots, products))
    knots, knot_cost = measured(lambda: edge_knots(products))
    (aligned, align_work), align_cost = measured(lambda: align_edges(products, knots))
    # A missing declared input never becomes complete just because an overhang arrived.
    first = min(snapshots)
    missing = dict(snapshots)
    missing.pop(first)
    pending, _ = route_cells(missing, recipients=(first,), **options)
    if pending[first].snapshot is not None or pending[first].required != (first,):
        raise ValueError("handoff manufactured a complete missing source")
    # Different recipient windows reuse one explicitly published edge-owner cohort.
    for recipient in sorted(products)[::12]:
        window, _ = route_cells(
            dict(reversed(list(snapshots.items()))), recipients=(recipient,), **options
        )
        window, _ = align_edges(window, knots)
        if window[recipient].snapshot != aligned[recipient].snapshot:
            raise ValueError("recipient window/order changed normalized product")
    records = [
        (t, p, c.parents[t + (p.feature, p.part)], c.snapshot.source)
        for t, c in aligned.items()
        for p in c.snapshot.pieces
    ]
    bodies, rejected, _ = assemble(
        [r[1].polygon for r in records],
        [r[1].info for r in records],
        [r[3] for r in records],
        assume_ground=True,
    )
    if rejected:
        raise ValueError("finite normalized source cohort rejected")
    cases = {}
    for kind in ("river", "lake"):
        candidates = [
            b
            for b in bodies
            if records[b.sources[0]][1].info["kind"] == kind
            and len({records[i][0] for i in b.sources}) > 1
        ]
        chosen = min(candidates, key=lambda b: -len(b.sources))
        selected = [records[i] for i in chosen.sources]
        name = "routed-" + kind
        # Preserve the original unmodified solver input even if the owner refuses.
        initial = prepare(selected, data, indices)
        np.savez_compressed(
            output / (name + "-input.npz"),
            vertices=initial[0].vertices,
            faces=initial[0].triangles,
            anchor=initial[1],
            rows=initial[2],
            bounds=initial[3],
        )
        try:
            result = run_case(
                name,
                lambda spacing, selected=selected: prepare(
                    selected, data, indices, spacing_m=spacing
                ),
                output,
                publish_contacts=lambda mesh, anchor, version, selected=selected, name=name: (
                    publish_contacts(
                        mesh,
                        anchor,
                        version,
                        selected,
                        knots,
                        output / (name + "-contacts.npz"),
                    )
                ),
            )
            cases[kind] = {
                "state": "compared",
                "report": name + ".json",
                "pre_fine_contacts": result["pre_fine_contacts"],
                "whole_domain_difference": result["whole_domain_difference"],
            }
        except ValueError as error:
            cases[kind] = {
                "state": "not_admitted",
                "reason": str(error),
                "input": name + "-input.npz",
                "scope": "no contact/fine/runtime product admitted from a failed owner; tolerances and solver budget unchanged",
            }
    report = {
        "scope": "finite explicitly declared delivered source batch; source acquisition and normalized product persistence are outside this experiment",
        "input_sha256": manifest["sha256"],
        "original_source_parts": len(data["surfaces"]),
        "source_bytes_decoded": data["source_bytes"],
        "declared_cells": len(snapshots),
        "routed_parts": sum(len(c.snapshot.pieces) for c in products.values()),
        "normalized_wkb_bytes": sum(
            len(p.polygon.wkb) for c in aligned.values() for p in c.snapshot.pieces
        ),
        "route_work": route_work,
        "route_cost": route_cost,
        "independent_inventory": oracle,
        "independent_inventory_cost": oracle_cost,
        "edge_knots": sum(map(len, knots.values())),
        "edge_preparation_cost": knot_cost,
        "edge_alignment": align_work,
        "edge_alignment_cost": align_cost,
        "recipient_window_and_order_replays": 5,
        "missing_input_refuses_publication": True,
        "original_geometry_unchanged": all(
            snapshots[t].pieces == s.pieces for t, s in raw.items()
        ),
        "height_cases": cases,
        "total_copied_bytes": None,
        "total_allocation_count": None,
        "process_peak_rss_bytes_including_all_diagnostics": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "limits": "No DB/IO/device win. Batch input closure is explicit; unseen external raw sources are not certified. Geometry beyond this batch stays in original inputs. Complete shared edge-height profiles, cross-cohort/corner ownership and bed/native consumers remain open.",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2))
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, default=DEFAULT)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.fixture, args.output)
