"""Build the fixed shared-vertex centre/QEF comparison from original BSC3 forms."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from coupled_vertex_clusters import cluster_mesh
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_plane_mesh import factorized_positions, unshifted_mesh
from source_surface_patches import original_patches


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        patches, basis, anchor, sources, needs = original_patches(
            parent, set(args.cells)
        )
        patched = time.perf_counter()
        original, mesh_work = unshifted_mesh(patches)
        control = factorized_positions(original)
        corners = original["normal_words"][original["indices"]]
        if not np.array_equal(corners, np.broadcast_to(corners[:, :1], corners.shape)):
            raise ValueError(
                "source geometric normals must remain constant per triangle"
            )
        common = {
            "face_normal_words": corners[:, 0],
            "face_patches": original["triangle_groups"],
            "patch_sources": np.array([p.source for p in patches], np.uint32),
            "basis": basis,
            "anchor": anchor,
            "source_bounds": np.stack(
                (control["positions"].min(0), control["positions"].max(0))
            ),
            "accepted_sources": sources,
            "needs_sources": np.array([n["source"] for n in needs], np.uint32),
        }
        prepared = time.perf_counter()
        reports = {}
        for method in ("centre", "qef"):
            before = time.perf_counter()
            arrays, report = cluster_mesh(
                control["positions"], control["indices"], method=method
            )
            report["placement_and_pack_seconds"] = time.perf_counter() - before
            arrays.update(common)
            np.savez_compressed(args.output / f"{method}.npz", **arrays)
            report.update(
                array_bytes={k: a.nbytes for k, a in arrays.items()},
                all_array_bytes=sum(a.nbytes for a in arrays.values()),
                file_bytes=(args.output / f"{method}.npz").stat().st_size,
            )
            reports[method] = report
        np.savez_compressed(args.output / "control.npz", **control, **common)
        result = {
            "producer": subprocess.check_output(
                ["git", "rev-parse", "HEAD"], text=True
            ).strip(),
            "source_archive_sha256": parent.archive_sha256,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "sources": len(sources),
            "needs": needs,
            "profile": {
                "radius_m": 3,
                "methods": ["centre", "qef"],
                "grid_origin": [0, 0, 0],
            },
            "control_geometry_bytes": sum(a.nbytes for a in control.values()),
            "control_mesh": mesh_work,
            "variants": reports,
            "source_open_seconds": opened - started,
            "source_polygons_seconds": patched - opened,
            "source_control_mesh_seconds": prepared - patched,
            "process_peak_rss_bytes": peak_rss_bytes(),
            "total_allocation_count": None,
            "total_physical_copied_bytes": None,
            "scope": "One declared camera-independent shared-cluster profile, before image admission. Fixed labelled triangle sets include zero-area triangles and a recorded corner swap; no face disappears. Barycentric correspondence bounds point displacement, not area, topology, visible ordering, filtered C/T/S or PBR. Source geometric normal words remain unchanged per triangle. Native material/UV/metre/program fields and native stored shading normals are absent, not reassigned; required whole original needs are excluded from these geometry bytes. No standard GPU binding/layout or frame-time claim.",
        }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
