"""Same finite original/field opponent: trace, compose-prefix and held lookups."""

import argparse
import gc
import json
import os
import platform
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from building_sample_inputs import digest
from compare_field_external_depth import DATA, bound, compose, prefix_reduced, reduced
from field_external_depth import SampleBinding
from field_external_opponents import foreign_ring, join, load, point_image, terrain
from field_view_integrals import box_integrals, summed_field
from process_memory import peak_rss_bytes
from triangle_image_reference import prepare_source


def run(args):
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Normal measurement priority required")
    paths = [
        Path(__file__),
        *(
            Path(__file__).parent / name
            for name in (
                "field_external_depth.py",
                "field_external_opponents.py",
                "compare_field_external_depth.py",
            )
        ),
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, paths)], text=True
    ):
        raise ValueError("Freeze measured source before recording costs")
    if not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Explicit system-temp output required")
    args.output.mkdir(parents=True, exist_ok=False)
    control = json.loads((args.control / "report.json").read_text())
    original, provenance, _raw, capture, rays, shape = load(args.inputs)
    if provenance != control["originals"]:
        raise ValueError("Same complete original and exact capture subset required")
    binding = SampleBinding(
        **dict(control["binding"], sources=tuple(control["binding"]["sources"]))
    )
    path = args.control / "terrain-and-foreign-opening.npz"
    row = next(v for v in control["cases"] if v["name"] == path.stem)
    if digest(path) != row["arrays_sha256"]:
        raise ValueError("Saved point opponent changed")
    with np.load(path, allow_pickle=False) as saved:
        a = bound(
            tuple(saved["a_" + key] for key in ("rgba", "depth", "error")), binding
        )
        b = bound(
            tuple(saved["b_" + key] for key in ("rgba", "depth", "error")),
            replace(binding, sources=("terrain-and-foreign-opening",)),
        )
        expected = reduced(saved["original_rgba"])
    external = join(terrain(original, capture), foreign_ring(capture, a.reverse_z))
    union = join(original, external)
    prepared = prepare_source(union)
    held = summed_field(compose(a, b).rgba, dtype=np.float64)
    h, w = shape
    y, x = np.mgrid[0:h:4, 0:w:4]
    lower = np.column_stack((x.ravel(), y.ravel())).astype(float)

    def direct():
        return reduced(point_image(union, rays, capture, shape, prepared=prepared)[0])

    def lookup():
        return (box_integrals(held, lower, lower + 4) / 16).reshape(*expected.shape)

    methods = {
        "prepared-original-trace": direct,
        "checked-compose-prefix": lambda: prefix_reduced(compose(a, b).rgba),
        "held-joint-prefix": lookup,
    }
    report = {name: {"rounds": []} for name in methods}
    load_before = os.getloadavg()
    for round_index in range(5):
        order = list(methods)[round_index % 3 :] + list(methods)[: round_index % 3]
        for name in order:
            gc.collect()
            start = time.process_time(), time.perf_counter()
            value = methods[name]()
            report[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - start[0],
                    "wall_s": time.perf_counter() - start[1],
                }
            )
            np.testing.assert_array_equal(value, expected)
            del value
    for name, call in methods.items():
        gc.collect()
        value, memory = allocations(args.output / (name + ".bin"), call)
        np.testing.assert_array_equal(value, expected)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_s"] = float(
                np.median([v[clock + "_s"] for v in report[name]["rounds"]])
            )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "control_report_sha256": digest(args.control / "report.json"),
        "opponent_sha256": digest(path),
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load_before,
        "load_after": os.getloadavg(),
        "measurements": report,
        "work": {
            "whole_original_sources": provenance["source_count"],
            "union_triangles": len(union["indices"]),
            "original_rays_per_trace": len(rays[0]),
            "original_internal_visits": None,
            "compose_point_pairs": int(np.prod(shape)),
            "held_output_pixels": len(lower),
            "held_logical_rgba_fetches": 16 * len(lower),
            "held_field_inputs_bytes": sum(
                v.nbytes
                for s in (a, b)
                for v in (s.rgba, s.reverse_z, s.error, s.known)
            ),
            "joint_prefix_bytes": held.table.nbytes,
            "known_owned_prefix_copy_bytes_per_rebuild": held.table.nbytes,
            "returned_pixels_bytes": expected.nbytes,
            "total_copies_or_physical_traffic": None,
            "native_tree_or_driver_storage": None,
        },
        "combined_process_peak_rss_bytes": peak_rss_bytes(),
        "scope": "Same finite opaque CPU/Lambert point original opponent and exact output; normal-priority i5 measurements. Timings exclude original index construction and source/external field acquisition. Both retain their prepared inputs outside timings. Prefix holds all source/external/radiance revisions; changed demand requires fresh validated inputs. Memray peaks exclude held inputs; combined RSS includes all alternatives and profiler, not device/UMA/frame cost. No per-frame rebuild recommendation or world/device admission.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                name: {k: v for k, v in row.items() if k.endswith("median_s")}
                for name, row in report.items()
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--control", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
