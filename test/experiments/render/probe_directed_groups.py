"""Build the bounded source-only flake counterexample; not a qualified far LOD."""

import argparse
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from directed_surface_groups import compile_groups
from parent_surface_inputs import open_parent
from process_memory import peak_rss_bytes
from source_surface_patches import original_patches


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        patches, basis, anchor, accepted, needs = original_patches(
            parent, set(args.cells)
        )
        projected = time.perf_counter()
        arrays, report = compile_groups(patches, [64, 64, 4])
        compiled = time.perf_counter()
        arrays.update(
            basis=basis,
            anchor=anchor,
            accepted_sources=accepted,
            needs_sources=np.array([row["source"] for row in needs], np.uint32),
        )
        area_error = abs(
            report["input_surface_area_m2"] - report["stored_surface_area_m2"]
        )
        if area_error > 1e-9 * report["input_surface_area_m2"]:
            raise ValueError("brick partition failed surface-area conservation")
        report.update(
            parent=parent.parent,
            cells=args.cells,
            accepted_sources=len(accepted),
            needs=needs,
            source_archive_sha256=parent.archive_sha256,
            source_archive_bytes=args.input.stat().st_size,
            form_reads=parent.form_reads,
            form_bytes=parent.form_bytes,
            open_parent_seconds=opened - started,
            source_projection_seconds=projected - opened,
            group_compile_seconds=compiled - projected,
            surface_partition_absolute_error_m2=area_error,
        )
    np.savez_compressed(args.output / "groups.npz", **arrays)
    report.update(
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        stored_array_bytes={key: value.nbytes for key, value in arrays.items()},
        field_file_bytes=(args.output / "groups.npz").stat().st_size,
        elapsed_seconds=time.perf_counter() - started,
        process_peak_rss_bytes=peak_rss_bytes(),
        total_allocation_count=None,
        total_physical_copied_bytes=None,
        scope="Source-only PSB1/BSC3 roof/wall measures, no native Shell expansion. Fixed 64x64x4m bricks/16x16 octahedral normal distribution. Homogeneous two-sided neutral-GGX flakes discard closed occupancy; this profile is a negative image experiment, not a World/material/opaque-depth contract or device result.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--cells", nargs="+", required=True, type=int)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
