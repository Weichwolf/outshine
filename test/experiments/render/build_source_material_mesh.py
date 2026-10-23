"""Build the source-side UV/metre contract on the existing fixed cluster profile."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from column_roof_patches import tangent_basis
from coupled_vertex_clusters import cluster_mesh
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_material_mesh import carry_fields, known_vertex_rows, material_mesh
from source_material_patches import shape_material_patches


def source_patches(parent, cells):
    records = [
        (int(i), s)
        for _, ids, surfaces in parent.iter_forms(cells)
        for i, s in zip(ids, surfaces, strict=True)
    ]
    supported = [s for _, s in records if s is not None]
    if not supported:
        raise ValueError("known source frames required")
    basis = tangent_basis(parent.plans)
    anchor = np.mean([s["origin"] for s in supported], axis=0) @ basis.T
    patches, accepted, needs = [], [], []
    for identity, surface in records:
        try:
            if surface is None or not surface["shapes"]:
                raise ValueError("missing source forms")
            parts = [
                p
                for part, shape in enumerate(surface["shapes"])
                for p in shape_material_patches(
                    surface, shape, identity, part, basis, anchor
                )
            ]
        except (
            ValueError,
            np.linalg.LinAlgError,
            shapely.errors.GEOSException,
        ) as error:
            needs.append({"source": identity, "reason": str(error)})
            continue
        patches.extend(parts)
        accepted.append(identity)
    return patches, basis, anchor, np.asarray(accepted, np.uint32), needs


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        patches, basis, anchor, sources, needs = source_patches(parent, set(args.cells))
        patched = time.perf_counter()
        control, fields, work = material_mesh(patches)
        meshed = time.perf_counter()
        centre, placement = cluster_mesh(
            control["positions"], control["indices"], method="centre"
        )
        clustered = time.perf_counter()
        common = {
            "basis": basis,
            "anchor": anchor,
            "source_bounds": np.stack(
                (control["positions"].min(0), control["positions"].max(0))
            ),
            "accepted_sources": sources,
            "needs_sources": np.array([n["source"] for n in needs], np.uint32),
        }
        products = {
            "control": control | fields | common,
            "centre": centre | carry_fields(fields, centre["corner_swap"]) | common,
        }
        accounting = {}
        for name, arrays in products.items():
            rows = known_vertex_rows(arrays, arrays)
            np.savez_compressed(args.output / f"{name}.npz", **arrays)
            accounting[name] = {
                "array_bytes": {k: a.nbytes for k, a in arrays.items()},
                "all_array_bytes": sum(a.nbytes for a in arrays.values()),
                "geometry_bytes": arrays["positions"].nbytes + arrays["indices"].nbytes,
                "known_attribute_tuple_rows": len(rows),
                "known_attribute_tuple_bytes": rows.nbytes,
                "known_attribute_tuple_plus_indices_bytes": rows.nbytes
                + arrays["indices"].nbytes,
                "file_bytes": (args.output / f"{name}.npz").stat().st_size,
            }
        report = {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_archive_sha256": parent.archive_sha256,
            "cells": args.cells,
            "sources": len(sources),
            "needs": needs,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "source_open_seconds": opened - start,
            "source_material_patch_seconds": patched - opened,
            "control_mesh_seconds": meshed - patched,
            "centre_placement_seconds": clustered - meshed,
            "mesh": work,
            "placement": placement,
            "products": accounting,
            "process_peak_rss_bytes": peak_rss_bytes(),
            "total_allocation_count": None,
            "total_physical_copied_bytes": None,
            "scope": "Source-side roof/wall fields, deck-separated gables, pre-weld UV and post-weld masonry metres carried unchanged through centre placement and corner swaps. Unknown native floor/overhang/recess support, colour factors and native shading normals are absent, not zero-cost. All degenerate triangles retained. No complete native PBR, device cost or whole-world residency admission; retained original needs excluded from these arrays.",
        }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
