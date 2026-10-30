"""Same original support, bounded vertex scratch versus whole-vertex control."""

import argparse
import hashlib
import json
import os
import platform
import subprocess
from pathlib import Path

import numpy as np
from compare_source_raster_candidates import payload, score
from experiment_output import create_output
from field_view_cost import measured
from native_source_triangle_materials import DATA, bound_selection
from native_world_inputs import load
from source_raster_candidates import MAX_VERTICES, projection, select

SIZES = (MAX_VERTICES, 512, 8192, 32768)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    names = (
        "source_raster_projection_cost.py",
        "source_raster_candidates.py",
        "compare_source_raster_candidates.py",
        "native_source_triangle_materials.py",
        "source_triangle_materials.py",
        "native_world_inputs.py",
        "field_view_cost.py",
        "building_sample_inputs.py",
        "building_sample_originals.py",
    )
    paths = [Path(__file__).with_name(name) for name in names]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze the measured projection and consumers")
    create_output(args.output)
    manifest, _, _, source = load(DATA, attributes=True)
    _, binding = bound_selection(DATA, source)
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths
        },
        "native_packet_sha256": binding["samples_sha256"],
        "platform": platform.platform(),
        "numpy": np.__version__,
        "logical_cpus": os.cpu_count(),
        "load_average": list(os.getloadavg()),
        "vertices": len(source["positions"]),
        "triangles": len(source["indices"]),
        "projected_output_bytes": len(source["positions"]) * 32,
        "original_numeric_bytes": payload(source, np.arange(len(source["indices"]))),
        "poses": {},
        "scope": "Matched Python projection scratch comparison. Full geometry loaded; each selector reprojects it. Separate projection and full query traces must not be added. Explicit wrapper output-copy bytes only; NumPy physical copies not measured. No native raster/HDR/depth, device/World/IO/UMA/frame admission.",
    }
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as archive:
        placement = archive["placements/words"]
        for pose in ("near-direction", "revealing-side"):
            view = archive[pose + "/view_words"]
            reference = archive[pose + "/triangle_ordinal"]
            row = {}
            expected_projection = expected_ids = None
            for size in SIZES:
                projected, projection_cost = measured(
                    lambda size=size, view=view: projection(
                        source["positions"],
                        view,
                        placement,
                        np.asarray(manifest["raster"]),
                        vertex_batch_size=size,
                    ),
                    args.output,
                    pose + "-projection-" + str(size),
                )
                if projected is None:
                    raise ValueError("Fixed projection unexpectedly unsupported")
                (ids, work), query_cost = measured(
                    lambda size=size, view=view: select(
                        source,
                        view,
                        placement,
                        manifest["raster"],
                        raster_guard_px=0.0,
                        vertex_batch_size=size,
                    ),
                    args.output,
                    pose + "-query-" + str(size),
                )
                if expected_projection is None:
                    expected_projection, expected_ids = projected, ids
                for actual, expected in zip(
                    projected, expected_projection, strict=True
                ):
                    np.testing.assert_array_equal(
                        actual.view(np.uint64), expected.view(np.uint64)
                    )
                np.testing.assert_array_equal(ids, expected_ids)
                scored = score(ids, reference)
                if scored["missing_native_ordinals"] or not work["complete_model"]:
                    raise ValueError("Finite native-owner containment changed")
                row[str(size)] = {
                    **scored,
                    "projection": projection_cost,
                    "whole_query": query_cost,
                    "work": work,
                    "source_payload": payload(source, ids),
                    "vertex_chunks": int(np.ceil(len(source["positions"]) / size)),
                    "max_vertex_chunk": min(len(source["positions"]), size),
                    "explicit_wrapper_output_copy_bytes": 0
                    if len(source["positions"]) <= size
                    else len(source["positions"]) * 32,
                    "projection_sha256": [
                        hashlib.sha256(a.tobytes()).hexdigest() for a in projected
                    ],
                    "original_ordinals_sha256": hashlib.sha256(
                        ids.tobytes()
                    ).hexdigest(),
                }
            report["poses"][pose] = row
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": report["commit"],
                "poses": {
                    p: {
                        s: {
                            "ms": round(c["whole_query"]["wall_median_s"] * 1000, 3),
                            "peak": c["whole_query"]["scoped_peak_bytes"],
                        }
                        for s, c in row.items()
                    }
                    for p, row in report["poses"].items()
                },
            }
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
