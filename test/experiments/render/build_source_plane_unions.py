"""Construct one bounded polygon-union aggregate before any image admission."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_plane_diagnostic import plot_meshes
from source_plane_mesh import factorized_positions, union_mesh, unshifted_mesh
from source_plane_union import compile_unions
from source_surface_patches import original_patches


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        patches, basis, anchor, sources, needs = original_patches(
            parent, set(args.cells)
        )
        extracted = time.perf_counter()
        groups, membership, work = compile_unions(patches)
        unioned = time.perf_counter()
        mesh, mesh_report = union_mesh(groups)
        control, control_report = unshifted_mesh(patches)
        meshed = time.perf_counter()
        vertices = np.concatenate(
            [p.origin + shapely.get_coordinates(p.domain) @ p.axes for p in patches]
        )
        mesh.update(
            basis=basis,
            anchor=anchor,
            source_bounds=np.stack((vertices.min(0), vertices.max(0))),
            accepted_sources=sources,
            needs_sources=np.array([n["source"] for n in needs], np.uint32),
            patch_sources=np.array([p.source for p in patches], np.uint32),
            patch_groups=membership,
            group_normals=np.array([g.normal for g in groups]),
            group_offsets=np.array([g.offset for g in groups]),
            group_fallback=np.array([g.fallback for g in groups]),
        )
        report = {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_archive_sha256": parent.archive_sha256,
            "source_archive_bytes": args.input.stat().st_size,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "source_count": len(sources),
            "needs": needs,
            "cells": args.cells,
            "profile": {"epsilon_m": 3.0, "maximum_normal_angle_degrees": 0.5},
            "work": work,
            "mesh": mesh_report,
            "unshifted_source_control": control_report,
            "source_open_seconds": opened - started,
            "source_patches_seconds": extracted - opened,
            "plane_unions_seconds": unioned - extracted,
            "both_meshes_seconds": meshed - unioned,
            "geos_version": shapely.geos_version_string,
        }
    np.savez_compressed(args.output / "mesh.npz", **mesh)
    np.savez_compressed(args.output / "unshifted.npz", **control)
    factored = factorized_positions(mesh)
    unshifted = factorized_positions(control)
    np.savez_compressed(
        args.output / "geometry-only.npz",
        **{"union_" + k: v for k, v in factored.items()},
        **{"unshifted_" + k: v for k, v in unshifted.items()},
    )
    plot_meshes(
        control,
        mesh,
        sum(a.nbytes for a in unshifted.values()),
        sum(a.nbytes for a in factored.values()),
        args.output / "construction.png",
    )
    report.update(
        array_bytes={key: value.nbytes for key, value in mesh.items()},
        all_mesh_array_bytes=sum(a.nbytes for a in mesh.values()),
        position_index_bytes=mesh["positions"].nbytes + mesh["indices"].nbytes,
        bit_factored_position_index_bytes=sum(a.nbytes for a in factored.values()),
        bit_factored_unshifted_position_index_bytes=sum(
            a.nbytes for a in unshifted.values()
        ),
        mesh_file_bytes=(args.output / "mesh.npz").stat().st_size,
        unshifted_control_array_bytes=sum(a.nbytes for a in control.values()),
        process_peak_rss_bytes=peak_rss_bytes(),
        total_allocation_count=None,
        total_physical_copied_bytes=None,
        material_scope="Geometry-only construction screen. Original UV, metre coordinates, colour factors and material/program selectors are not transferred. Packed normals represent chosen geometric plane/source normals, not original stored shading normals. Source/patch lineage does not implement a shading lookup. No replacement UV/BRDF mapping, material atlas or new indirection contract is provided; their eventual costs are absent and cannot rescue an already larger geometry-only result.",
        scope="One source-only 3m/0.5-degree plane-union profile with exact polygon masks, no sampled alpha/normal charts. Each source patch is projected as a whole or kept by its own plane; every accepted source remains mapped, incomplete original sources remain needs. The real-arithmetic slab bound plus measured Float32 vertex rounding is a positional model, not a silhouette/coverage/depth-order/closed-solid/PBR guarantee. Includes cold patch/group provenance; excludes required whole original fallback source products. No runtime/world/device admission or globally optimal plane cover.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
