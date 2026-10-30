"""Same finite original/field opponent: trace, compose-prefix and held lookups."""

import argparse
import ast
import gc
import hashlib
import json
import os
import platform
import runpy
import subprocess
import time
from dataclasses import replace
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from building_sample_inputs import digest
from compare_field_external_depth import DATA, bound, compose, prefix_reduced, reduced
from experiment_output import create_output
from field_external_depth import SampleBinding, interval, over_constant_background
from field_external_opponents import foreign_ring, join, load, point_image, terrain
from field_view_integrals import box_integrals, summed_field
from process_memory import peak_rss_bytes
from triangle_image_reference import prepare_source


def measure(methods, expected, output):
    report = {name: {"rounds": []} for name in methods}
    for round_index in range(5):
        start = round_index % len(methods)
        order = list(methods)[start:] + list(methods)[:start]
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
        value, memory = allocations(output / (name + ".bin"), call)
        np.testing.assert_array_equal(value, expected)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_s"] = float(
                np.median([v[clock + "_s"] for v in report[name]["rounds"]])
            )
    return report


def baseline_compose(commit, a, b, output):
    dependencies = (
        "field_composition.py",
        "field_view_integrals.py",
    )
    changed = subprocess.run(
        [
            "git",
            "diff",
            "--quiet",
            commit,
            "--",
            *(str(Path(__file__).with_name(name)) for name in dependencies),
        ],
        check=False,
    )
    if changed.returncode:
        raise ValueError(
            "Baseline requires unchanged point validation and prefix/mean dependencies"
        )
    compare_path = Path(__file__).with_name("compare_field_external_depth.py")
    previous = subprocess.check_output(
        [
            "git",
            "show",
            commit + ":test/experiments/render/compare_field_external_depth.py",
        ],
        text=True,
    )

    def functions(source):
        return {
            n.name: ast.dump(n, include_attributes=False)
            for n in ast.parse(source).body
            if isinstance(n, ast.FunctionDef)
            and n.name in ("reduced", "prefix_reduced")
        }

    if functions(previous) != functions(compare_path.read_text()):
        raise ValueError("Baseline requires identical mean/prefix evaluation functions")
    source = subprocess.check_output(
        ["git", "show", commit + ":test/experiments/render/field_external_depth.py"]
    )
    snapshot = output / "baseline.py"
    snapshot.write_bytes(source)
    module = runpy.run_path(str(snapshot))

    def convert(s):
        values = {
            k: getattr(s.binding, k)
            for k in module["SampleBinding"].__dataclass_fields__
        }
        # The old arithmetic baseline has no fixture mode. Its context token is
        # used only for this same-fixture value/cost comparison, never a World.
        if values["world_revision"] is None:
            values["world_revision"] = s.binding.fixture_revision
        binding = module["SampleBinding"](**values)
        return module["PointSamples"](binding, s.rgba, s.reverse_z, s.error, s.known)

    left, right = convert(a), convert(b)

    def call():
        result = module["compose_registered"](
            left, right, wanted_a=left.binding, wanted_b=right.binding
        )
        if result.rgba is None:
            raise ValueError("Baseline rejected the same actual point opponent")
        return prefix_reduced(result.rgba)

    return call, {"commit": commit, "source_sha256": hashlib.sha256(source).hexdigest()}


def constant_method(a, b, lookup):
    low_a, _high_a = interval(a)
    _low_b, high_b = interval(b)
    common = (a.reverse_z > 0) & (b.reverse_z > 0)
    if not np.all(b.rgba == b.rgba[0, 0]) or np.any(common & (low_a <= high_b)):
        raise ValueError("Actual ordered constant background required")
    source_prefix = summed_field(a.rgba, dtype=np.float64)
    constant = b.rgba[0, 0].astype(np.float64)
    return (
        lambda: over_constant_background(lookup(source_prefix), constant),
        source_prefix,
    )


def prepare_measurement(output):
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
    create_output(output)


def run(args):
    prepare_measurement(args.output)
    control = json.loads((args.control / "report.json").read_text())
    original, provenance, _raw, capture, rays, shape = load(args.inputs)
    if provenance != control["originals"]:
        raise ValueError("Same complete original and exact capture subset required")
    fields = dict(control["binding"])
    fields["sources"] = tuple(fields["sources"])
    fields["sample_revision"] = tuple(fields.get("sample_revision", ()))
    # Legacy reports from this controlled geometry study called their fixture
    # identity world_revision. Keep that identity diagnostic-only on input.
    fields["fixture_revision"] = (
        fields.get("fixture_revision") or fields["world_revision"]
    )
    fields["world_revision"] = None
    binding = SampleBinding(**fields)
    path = args.control / (args.case + ".npz")
    row = next(v for v in control["cases"] if v["name"] == path.stem)
    if digest(path) != row["arrays_sha256"]:
        raise ValueError("Saved point opponent changed")
    with np.load(path, allow_pickle=False) as saved:
        a = bound(
            tuple(saved["a_" + key] for key in ("rgba", "depth", "error")), binding
        )
        b = bound(
            tuple(saved["b_" + key] for key in ("rgba", "depth", "error")),
            replace(binding, sources=(args.case,)),
        )
        expected = reduced(saved["original_rgba"])
    external = terrain(original, capture)
    if args.case == "terrain-and-foreign-opening":
        external = join(external, foreign_ring(capture, a.reverse_z))
    union = join(original, external)
    prepared = prepare_source(union)
    held = summed_field(compose(a, b).rgba, dtype=np.float64)
    h, w = shape
    y, x = np.mgrid[0:h:4, 0:w:4]
    lower = np.column_stack((x.ravel(), y.ravel())).astype(float)

    def direct():
        return reduced(point_image(union, rays, capture, shape, prepared=prepared)[0])

    def lookup(prefix=held):
        return (box_integrals(prefix, lower, lower + 4) / 16).reshape(*expected.shape)

    methods = {
        "prepared-original-trace": direct,
        "checked-compose-prefix": lambda: prefix_reduced(compose(a, b).rgba),
        "held-joint-prefix": lookup,
    }
    source_prefix = None
    if args.case == "terrain":
        methods["held-source-over-constant"], source_prefix = constant_method(
            a, b, lookup
        )
    baseline = None
    if args.baseline_commit:
        methods["baseline-compose-prefix"], baseline = baseline_compose(
            args.baseline_commit, a, b, args.output
        )
    load_before = os.getloadavg()
    report = measure(methods, expected, args.output)
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
        "baseline": baseline,
        "case": args.case,
        "work": {
            "whole_original_sources": provenance["source_count"],
            "union_triangles": len(union["indices"]),
            "original_rays_per_trace": len(rays[0]),
            "original_internal_visits": None,
            "compose_point_pairs": int(np.prod(shape)),
            "common_covered_points": int(
                np.count_nonzero((a.reverse_z > 0) & (b.reverse_z > 0))
            ),
            "held_output_pixels": len(lower),
            "held_logical_rgba_fetches": 16 * len(lower),
            "held_field_inputs_bytes": sum(
                v.nbytes
                for s in (a, b)
                for v in (s.rgba, s.reverse_z, s.error, s.known)
            ),
            "joint_prefix_bytes": held.table.nbytes,
            "separate_source_prefix_bytes": source_prefix.table.nbytes
            if source_prefix
            else 0,
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
    parser.add_argument("--baseline-commit")
    parser.add_argument(
        "--case",
        choices=("terrain", "terrain-and-foreign-opening"),
        default="terrain-and-foreign-opening",
    )
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
